{
  "targets": [
  {
    "target_name": "atlas",
      "dependencies": ["libatlas"],
      "sources": [ "src/atlas.cc", "src/utils.cc",
        "src/start_stop.cc", "src/functions.cc"],
      "cflags": [ "-std=c++11", "-Wall", "-g", "-Os" ],
      "libraries": [
        "-L<(module_root_dir)/build/Release/",
        "-Wl,-rpath,\$$ORIGIN",
        "-latlasclient"
      ],
      "include_dirs" : [
        "<!(node -e \"require('nan')\")",
        "/usr/local/include"
        ],
      "conditions": [
        [ 'OS=="mac"', {
          "xcode_settings": {
            'OTHER_CPLUSPLUSFLAGS' : ['-std=c++11','-stdlib=libc++', '-v'],
            'OTHER_LDFLAGS': ['-stdlib=libc++'], 
            'MACOSX_DEPLOYMENT_TARGET': '10.12',
            'GCC_ENABLE_CPP_EXCEPTIONS': 'NO'
          }
        }]]
  },
  {
    "target_name": "libatlas",
    "type": "none",
    "actions": [{
      "action_name": "install_libatlasclient",
      "inputs": [""],
      "outputs": [""],
      "action": ["sh", "scripts/install-lib.sh"]
    }]
  },
  ]
}
