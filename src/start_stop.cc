#include "start_stop.h"
#include "utils.h"
#include <atlas/atlas_client.h>
#include <unordered_map>
#include <sys/resource.h>

using atlas::meter::Counter;
using atlas::meter::Gauge;
using atlas::meter::IdPtr;
using atlas::meter::Tag;
using atlas::meter::Tags;
using atlas::meter::Timer;
using v8::HeapSpaceStatistics;
using v8::GCType;

static bool started = false;
static bool timer_started = false;
static uv_timer_t lag_timer;
static uv_timer_t fd_timer;
static int64_t prev_timestamp;
static constexpr unsigned int POLL_PERIOD_MS = 500;
static constexpr unsigned int FD_PERIOD_MS = 30 * 1000;
static constexpr int64_t POLL_PERIOD_NS = POLL_PERIOD_MS * 1000000;
static Tags runtime_tags;

// how far behind are we in scheduling a periodic timer
static std::shared_ptr<Timer> atlas_lag_timer;
static std::shared_ptr<Gauge<double>> open_fd_gauge;
static std::shared_ptr<Gauge<double>> max_fd_gauge;

static void record_lag(uv_timer_t* handle) {
  auto now = atlas_registry.clock().MonotonicTime();
  // compute lag
  if (prev_timestamp == 0) {
    prev_timestamp = now;
    return;
  }

  auto elapsed = now - prev_timestamp;
  auto lag = elapsed - POLL_PERIOD_NS;
  prev_timestamp = now;
  atlas_lag_timer->Record(lag);
}

static size_t get_dir_count(const char* dir) {
  auto fd = opendir(dir);
  if (fd == nullptr) {
    return 0;
  }
  size_t count = 0;
  struct dirent* dp;
  while ((dp = readdir(fd)) != nullptr) {
    if (dp->d_name[0] == '.') {
      // ignore hidden files (including . and ..)
      continue;
    }
    ++count;
  }
  closedir(fd);
  return count;
}

static void record_fd_activity(uv_timer_t* handle) {
  auto fd_count = get_dir_count("/proc/self/fd");
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);

  open_fd_gauge->Update(fd_count);
  max_fd_gauge->Update(rl.rlim_cur);
}

static HeapSpaceStatistics* beforeStats;
static int64_t startGC;
static std::shared_ptr<Counter> alloc_rate_counter;
static std::shared_ptr<Counter> promotion_rate_counter;
static std::shared_ptr<Gauge<double>> live_data_size;
static std::shared_ptr<Gauge<double>> max_data_size;

static std::unordered_map<int, std::shared_ptr<Timer>> gc_timers;

inline IdPtr node_id(const char* name) {
  return atlas_registry.CreateId(name, runtime_tags);
}

static void create_memory_meters() {
  alloc_rate_counter =
      atlas_registry.counter(node_id("nodejs.gc.allocationRate"));
  promotion_rate_counter =
      atlas_registry.counter(node_id("nodejs.gc.promotionRate"));
  live_data_size = atlas_registry.gauge(node_id("nodejs.gc.liveDataSize"));
  max_data_size = atlas_registry.gauge(node_id("nodejs.gc.maxDataSize"));
}

static void create_gc_timers() {
  auto base_id = node_id("nodejs.gc.pause");
  gc_timers[v8::kGCTypeScavenge] =
      atlas_registry.timer(base_id->WithTag(Tag::of("id", "scavenge")));
  gc_timers[v8::kGCTypeMarkSweepCompact] =
      atlas_registry.timer(base_id->WithTag(Tag::of("id", "markSweepCompact")));
  gc_timers[v8::kGCTypeIncrementalMarking] =
      atlas_registry.timer(base_id->WithTag(Tag::of("id", "incrementalMarking")));
  gc_timers[v8::kGCTypeProcessWeakCallbacks] =
      atlas_registry.timer(base_id->WithTag(Tag::of("id", "processWeakCallbacks")));
}

static std::shared_ptr<Timer> get_gc_timer(GCType type) {
  auto int_type = static_cast<int>(type);
  auto it = gc_timers.find(int_type);
  if (it != gc_timers.end()) {
    return it->second;
  }

  // Unknown GC type - should never happen with node 6.x or 7.x
  return atlas_registry.timer(
      node_id("nodejs.gc.pause")->WithTag(Tag::of("id", std::to_string(int_type))));
}

inline size_t number_heap_spaces() {
  auto isolate = v8::Isolate::GetCurrent();
  return isolate->NumberOfHeapSpaces();
}

inline HeapSpaceStatistics* alloc_heap_stats() {
  return new HeapSpaceStatistics[number_heap_spaces()];
}

inline void free_heap_stats(HeapSpaceStatistics* stats) { delete[] stats; }

static bool fill_heap_stats(HeapSpaceStatistics* stats) {
  bool ok = true;
  auto n = number_heap_spaces();
  auto isolate = v8::Isolate::GetCurrent();
  for (size_t i = 0; i < n; ++i) {
    if (!isolate->GetHeapSpaceStatistics(stats + i, i)) {
      ok = false;
    }
  }
  return ok;
}

static NAN_GC_CALLBACK(beforeGC) {
  startGC = atlas_registry.clock().MonotonicTime();
  fill_heap_stats(beforeStats);
}

static int find_space_idx(HeapSpaceStatistics* stats, const char* name) {
  auto n = static_cast<int>(number_heap_spaces());
  for (auto i = 0; i < n; ++i) {
    auto space_name = stats[i].space_name();
    if (strcmp(space_name, name) == 0) {
      return i;
    }
  }
  return -1;
}

static NAN_GC_CALLBACK(afterGC) {
  auto elapsed = atlas_registry.clock().MonotonicTime() - startGC;
  get_gc_timer(type)->Record(elapsed);

  auto stats = alloc_heap_stats();
  fill_heap_stats(stats);

  v8::HeapStatistics heapStats;
  Nan::GetHeapStatistics(&heapStats);
  max_data_size->Update(heapStats.heap_size_limit());

  auto old_space_idx = find_space_idx(beforeStats, "old_space");
  bool live_data_updated = false;

  if (old_space_idx >= 0) {
    auto old_before = beforeStats[old_space_idx].space_used_size();
    auto old_after = stats[old_space_idx].space_used_size();
    auto delta = old_after - old_before;
    if (delta > 0) {
      promotion_rate_counter->Add(delta);
    }

    if (old_after < old_before || type == v8::kGCTypeMarkSweepCompact) {
      live_data_updated = true;
      live_data_size->Update(old_after);
    }
  }

  auto new_space_idx = find_space_idx(beforeStats, "new_space");
  if (new_space_idx >= 0) {
    auto young_before =
        static_cast<int64_t>(beforeStats[new_space_idx].space_used_size());
    auto young_after =
        static_cast<int64_t>(stats[new_space_idx].space_used_size());
    auto delta = young_before - young_after;
    alloc_rate_counter->Add(delta);
  }
  free_heap_stats(stats);

  if (!live_data_updated) {
    // refresh the last-updated time
    live_data_size->Update(live_data_size->Value());
  }
}

NAN_METHOD(start) {
  if (started) {
    return;
  }

  std::vector<std::string> log_dirs;
  if (info.Length() == 1 && info[0]->IsObject()) {
    auto isolate = info.GetIsolate();
    auto context = isolate->GetCurrentContext();
    const auto& options = info[0].As<v8::Object>();
    const auto& logDirsKey = Nan::New("logDirs").ToLocalChecked();
    const auto& runtimeMetricsKey = Nan::New("runtimeMetrics").ToLocalChecked();
    const auto& runtimeTagsKey = Nan::New("runtimeTags").ToLocalChecked();

    auto maybeLogDirs = options->Get(context, logDirsKey);
    if (!maybeLogDirs.IsEmpty()) {
      auto input_log_dirs = maybeLogDirs.ToLocalChecked().As<v8::Array>();
      const auto n = input_log_dirs->Length();
      for (size_t i = 0; i < n; ++i) {
        auto dir = input_log_dirs->Get(i);
        const auto& path = std::string(*Nan::Utf8String(dir));
        log_dirs.push_back(path);
      }
    }

    Tags runtime_tags;
    auto maybe_runtime_tags = options->Get(context, runtimeTagsKey);
    if (!maybe_runtime_tags.IsEmpty()) {
      tagsFromObject(isolate,
                     maybe_runtime_tags.ToLocalChecked().As<v8::Object>(),
                     &runtime_tags);
    }

    auto maybeRuntimeMetrics = options->Get(context, runtimeMetricsKey);
    if (!maybeRuntimeMetrics.IsEmpty()) {
      auto runtimeMetrics =
          maybeRuntimeMetrics.ToLocalChecked().As<v8::Boolean>()->Value();
      if (runtimeMetrics) {
        // setup lag timer
        atlas_lag_timer = atlas_registry.timer(node_id("nodejs.eventLoopLag"));
        uv_timer_init(uv_default_loop(), &lag_timer);
        uv_timer_start(&lag_timer, record_lag, POLL_PERIOD_MS, POLL_PERIOD_MS);

        uv_timer_init(uv_default_loop(), &fd_timer);
        uv_timer_start(&fd_timer, record_fd_activity, FD_PERIOD_MS, FD_PERIOD_MS);
        timer_started = true;

        create_memory_meters();
        create_gc_timers();
        beforeStats = alloc_heap_stats();

        Nan::AddGCPrologueCallback(beforeGC);
        Nan::AddGCEpilogueCallback(afterGC);

        open_fd_gauge = atlas_registry.gauge(node_id("openFileDescriptorsCount"));
        max_fd_gauge = atlas_registry.gauge(node_id("maxFileDescriptorsCount"));
      }
    }
  }

  if (!log_dirs.empty()) {
    SetLoggingDirs(log_dirs);
  }
  InitAtlas();
  started = true;
}

NAN_METHOD(stop) {
  if (started) {
    ShutdownAtlas();
    started = false;
  }

  if (timer_started) {
    uv_timer_stop(&lag_timer);
    uv_timer_stop(&fd_timer);
    prev_timestamp = 0;

    Nan::RemoveGCPrologueCallback(beforeGC);
    Nan::RemoveGCEpilogueCallback(afterGC);
    free_heap_stats(beforeStats);
    beforeStats = nullptr;
  }
}
