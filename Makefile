include Makefile.inc

GCOVR_ROOT = src/
GCOVR_ARGS = --object-directory=$(GCOVR_ROOT) -r . --exclude='$(GCOVR_ROOT)debug.c' --exclude='test.*'

.PHONY: all build clean debug test

all: debug

debug:
	@$(eval export BUILDDIR=$(shell pwd)/$(DIR_BUILD_DEBUG))
	$(MAKE) build DEBUG=1

docs:
	mkdir -p build/docs
	doxygen

run: debug
	./$(DIR_BUILD_DEBUG)/server

build:
	pkg-config --exists '$(LIBS_VERSIONS)'
	mkdir -p $(BUILDDIR)
	cp quickio.ini $(BUILDDIR)
	$(MAKE) -C src
	$(MAKE) -C app

clean:
	rm -rf $(DIR_BUILD)
	rm -rf $(DIR_BUILD_DEBUG)
	rm -rf $(DIR_BUILD_TEST)
	rm -f test*.xml
	$(MAKE) -C app clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test:
	@$(eval export BUILDDIR=$(shell pwd)/$(DIR_BUILD_TEST))
	@$(MAKE) build DEBUG=1 TESTING=1
	@$(MAKE) -C test test DEBUG=1
	@./ext/gcovr -p $(GCOVR_ARGS)
	
test-jenkins: clean
	$(MAKE) test TEST_OUTPUT_XML=1
	@./ext/gcovr -x -o test_coverage.xml $(GCOVR_ARGS) --exclude='src/qsys*'