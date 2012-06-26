.PHONY: all clean test

all:
	$(MAKE) -C src server

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean

test: all
	$(MAKE) -C test test