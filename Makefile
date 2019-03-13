#
# Directories
#
ROOT           := $(shell pwd)
NODE_MODULES   := $(ROOT)/node_modules
NODE_BIN       := $(NODE_MODULES)/.bin
TOOLS          := $(ROOT)/tools
TMP            := $(ROOT)/tmp

# Publish Paths
STAGE       := $(shell node ./tools/publish_paths.js stage)
PUBLISH_URL := $(shell node ./tools/publish_paths.js url)


#
# Tools and binaries
#
ESLINT		:= $(NODE_BIN)/eslint
JSCS		:= $(NODE_BIN)/jscs
PREGYP       := $(NODE_BIN)/node-pre-gyp
MOCHA       := $(NODE_BIN)/mocha
_MOCHA      := $(NODE_BIN)/_mocha
ISTANBUL    := $(NODE_BIN)/istanbul
COVERALLS   := $(NODE_BIN)/coveralls
NSP         := $(NODE_BIN)/nsp
NPM		    := npm
GIT         := git
NSP_BADGE   := $(TOOLS)/nspBadge.js
COVERAGE_BADGE := $(TOOLS)/coverageBadge.js


#
# Directories
#
LIB_FILES  	   := $(ROOT)/
TEST_FILES     := $(ROOT)/test
COVERAGE_FILES := $(ROOT)/coverage


#
# Files and globs
#
CHANGESMD		:= $(ROOT)/CHANGES.md
GIT_HOOK_SRC    = '../../tools/githooks/pre-push'
GIT_HOOK_DEST   = '.git/hooks/pre-push'
SHRINKWRAP     := $(ROOT)/npm-shrinkwrap.json
LCOV           := $(ROOT)/coverage/lcov.info
TEST_ENTRY     := $(shell find $(TEST_FILES) -name '*.test.js')

ALL_FILES		:= $(shell find $(ROOT) \
	-not \( -path '*node_modules*' -prune \) \
	-not \( -path $(COVERAGE_FILES) -prune \) \
	-not \( -path '*/\.*' -prune \) \
	-type f -name '*.js')

#
# Targets
#

.PHONY: all
all: clean node_modules lint codestyle post-install test

SYSTEM := $(shell uname -s)
CUR_PATH := $(shell otool -L build/Release/atlas.node | awk '/libatlasclient/{print $$1}')
.PHONY: post-install
post-install:
ifeq ($(SYSTEM), Darwin)
	@echo Current path: ${CUR_PATH}
	install_name_tool -change ${CUR_PATH} @loader_path/libatlasclient.dylib build/Release/atlas.node
endif
	rm -rf build/Release/obj.target build/Release/.deps

node_modules: package.json build/Release
	@$(NPM) install
	@touch $(NODE_MODULES)

build/Release:
	@$(NPM) install

.PHONY: githooks
githooks:
	@ln -s $(GIT_HOOK_SRC) $(GIT_HOOK_DEST)


.PHONY: lint
lint: node_modules $(ALL_FILES)
	@$(ESLINT) $(ALL_FILES)


.PHONY: codestyle
codestyle: node_modules $(ALL_FILES)
	@$(JSCS) $(ALL_FILES)


.PHONY: codestyle-fix
codestyle-fix: node_modules $(ALL_FILES)
	@$(JSCS) $(ALL_FILES) --fix


.PHONY: nsp
nsp: node_modules $(ALL_FILES)
	@$(NPM) shrinkwrap --dev
	@($(NSP) check) | $(NSP_BADGE)
	@rm $(SHRINKWRAP)


.PHONY: prepush
prepush: node_modules lint codestyle coverage nsp 

.PHONY: package
package: node_modules test lint codestyle
	@$(PREGYP) package

.PHONY: publish
publish: package
	# TODO
	@echo Not implemented

.PHONY: test
test: node_modules $(ALL_FILES)
	@$(MOCHA) -R spec $(TEST_ENTRY) --full-trace


.PHONY: coverage
coverage: node_modules $(ALL_FILES)
	@$(ISTANBUL) cover $(_MOCHA) --report json-summary --report html -- -R spec
	@$(COVERAGE_BADGE)


.PHONY: report-coverage
report-coverage: coverage
	@cat $(LCOV) | $(COVERALLS)


.PHONY: clean-coverage
clean-coverage:
	@rm -rf $(COVERAGE_FILES)


.PHONY: clean
clean: clean-coverage
	@rm -rf $(NODE_MODULES) nc


#
## Debug -- print out a a variable via `make print-FOO`
#
print-%  : ; @echo $* = $($*)
