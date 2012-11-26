include Makefile.inc

GCOVR_ARGS = -p -r . --exclude='src/log.*' --exclude='test.*' --exclude='src/http_parser.c' --exclude='src/persistent_socket.c' --exclude="ext.*" --single-directory
GCOVR_ARGS_SRC = $(GCOVR_ARGS) --object-directory=src/ 
GCOVR_ARGS_APPS = $(GCOVR_ARGS) --object-directory=$(ROOT)/app/

.PHONY: all build clean debug docs test

all: debug

debug:
	@$(eval export BUILDDIR=$(shell pwd)/$(DIR_BUILD_DEBUG))
	$(MAKE) build DEBUG=1

profile:
	@$(eval export BUILDDIR=$(shell pwd)/$(DIR_BUILD_PROFILE))
	$(MAKE) build PROFILE=1

docs:
	$(MAKE) -C docs html
	doxygen

run: debug
	./$(DIR_BUILD_DEBUG)/qio

build_dir:
	mkdir -p $(BUILDDIR)

build: build_dir
	pkg-config --exists '$(LIBS_VERSIONS)'
	cp $(QIOINI) $(BUILDDIR)/$(QIOINI_DEFAULT)
	$(MAKE) -C src
	$(MAKE) -C app

clean:
	rm -rf $(DIR_BUILD)
	rm -rf $(DIR_BUILD_DEBUG)
	rm -rf $(DIR_BUILD_PROFILE)
	rm -rf $(DIR_BUILD_TEST)
	rm -f test*.xml
	$(MAKE) -C app clean
	$(MAKE) -C docs clean
	$(MAKE) -C ext clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test:
	@$(eval export BUILDDIR=$(shell pwd)/$(DIR_BUILD_TEST))
	@$(MAKE) build DEBUG=1 TESTING=1
	@$(MAKE) -C test test DEBUG=1
	@./ext/gcovr $(GCOVR_ARGS_SRC) $(BUILDDIR)
	@./ext/gcovr $(GCOVR_ARGS_APPS) $(BUILDDIR)/apps
	
test-jenkins: clean
	@$(MAKE) test TEST_OUTPUT_XML=1
	@./ext/gcovr $(GCOVR_ARGS_SRC) -x -o test_coverage.xml --exclude='src/qsys*' --exclude='src/main*' $(DIR_BUILD_TEST)
	@./ext/gcovr $(GCOVR_ARGS_APPS) -x -o test_coverage_apps.xml $(DIR_BUILD_TEST)/apps