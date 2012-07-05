.PHONY: all build clean debug test

all: debug

debug:
	$(MAKE) build DEBUG=1

build:
	$(MAKE) -C src $(ARSOCKLIB)
	$(MAKE) -C app

clean:
	$(MAKE) -C app clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test: all
	$(MAKE) -C test test