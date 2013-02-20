include Makefile.inc

GCOVR_ARGS = -p -r . --exclude='src/log.*' --exclude='test.*' $(patsubst %.o, --exclude='src/%.c', $(DEPENDENCIES)) --exclude='lib.*' --single-directory
GCOVR_ARGS_SRC = $(GCOVR_ARGS) --object-directory=src/ 
GCOVR_ARGS_APPS = $(GCOVR_ARGS) --object-directory=$(ROOT)/app/

.PHONY: all build clean debug docs test

all: debug

debug: export DEBUG=1
debug: export BUILDDIR=$(shell pwd)/$(DIR_BUILD_DEBUG)
debug:
	$(MAKE) build

profile: export BUILDDIR=$(shell pwd)/$(DIR_BUILD_PROFILE)
profile:
	$(MAKE) build PROFILE=1

docs:
	$(MAKE) -C docs html
	doxygen

run: debug
	./$(DIR_BUILD_DEBUG)/qio

build-dir:
	mkdir -p $(BUILDDIR)

build: build-dir
	pkg-config --exists '$(LIBS_VERSIONS)'
	cp $(QIOINI) $(BUILDDIR)/$(QIOINI_DEFAULT)
	@$(MAKE) -C src --no-print-directory

clean:
	rm -rf $(DIR_BUILD)
	rm -rf $(DIR_BUILD_DEBUG)
	rm -rf $(DIR_BUILD_PROFILE)
	rm -rf $(DIR_BUILD_TEST)
	rm -f test*.xml
	$(MAKE) -C app clean
	$(MAKE) -C docs clean
	$(MAKE) -C tools clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	$(MAKE) -C tools clean

test-build: export DEBUG=1
test-build: export TESTING=1
test-build: export BUILDDIR=$(shell pwd)/$(DIR_BUILD_TEST)
test-build:
	@$(MAKE) build
	@$(MAKE) -C app

test-all: test test-valgrind
	
test-valgrind: test-build
	@$(MAKE) valgrind DEBUG=1

test: test-build
	@G_SLICE=debug-blocks $(MAKE) -C test test DEBUG=1
	@./tools/gcovr $(GCOVR_ARGS_SRC) $(BUILDDIR)
	@./tools/gcovr $(GCOVR_ARGS_APPS) $(BUILDDIR)/apps
	
valgrind: test-build
	@$(MAKE) -C test valgrind DEBUG=1
	
test-jenkins: clean
	@$(MAKE) test TEST_OUTPUT_XML=1
	@./tools/gcovr $(GCOVR_ARGS_SRC) -x -o $(DIR_BUILD_TEST)/test_coverage.xml --exclude='src/qsys*' --exclude='src/main*' $(DIR_BUILD_TEST)
	@./tools/gcovr $(GCOVR_ARGS_APPS) -x -o $(DIR_BUILD_TEST)/test_coverage_apps.xml $(DIR_BUILD_TEST)/apps