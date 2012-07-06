include Makefile.inc

.PHONY: all build clean debug test

all: debug

debug:
	$(MAKE) build DEBUG=1

build:
	mkdir -p $(BUILDDIR)
	cp csocket.ini $(BUILDDIR)
	$(MAKE) -C src
	$(MAKE) -C app

clean:
	rm -rf $(BUILDDIR)
	$(MAKE) -C app clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test: all
	$(MAKE) -C test test