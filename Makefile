.PHONY: all clean test

all:
	$(MAKE) -C src $(ARSOCKLIB)
	$(MAKE) -C app

clean:
	$(MAKE) -C app clean
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test: all
	$(MAKE) -C test test