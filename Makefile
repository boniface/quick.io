include Makefile.inc

.PHONY: all build clean debug test

all: debug

debug:
	$(MAKE) build DEBUG=1

run: debug
	./build/server

build:
	pkg-config --exists '$(LIBS_VERSIONS)'
	mkdir -p $(BUILDDIR)
	cp quickio.ini $(BUILDDIR)
	$(MAKE) -C src
	$(MAKE) -C app

clean:
	rm -rf $(BUILDDIR)
	$(MAKE) -C app clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test: all
	$(MAKE) -C test test