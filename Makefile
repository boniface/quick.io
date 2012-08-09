include Makefile.inc

.PHONY: all build clean debug test

all: debug

debug:
	$(MAKE) build DEBUG=1

docs:
	mkdir -p build/docs
	doxygen

run: debug
	./build/server

build:
	pkg-config --exists '$(LIBS_VERSIONS)'
	mkdir -p $(BUILDDIR)
	cp quickio.ini $(BUILDDIR)
	$(MAKE) -C src
	$(MAKE) -C app

clean:
	rm -rf $(DIR_BUILD)
	rm -rf $(DIR_BUILD_TEST)
	rm -f test*.xml
	$(MAKE) -C app clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test:
	@$(eval export BUILDDIR=$(shell pwd)/$(DIR_BUILD_TEST))
	@$(MAKE) debug TESTING=1
	@$(MAKE) -C test test DEBUG=1
	@./ext/gcovr
	
test-jenkins: clean
	$(MAKE) test TEST_OUTPUT_XML=1
	./ext/gcovr -x --object-directory=src/ -r . -o test_coverage.xml