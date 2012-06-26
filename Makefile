.PHONY: all clean test

all:
	$(MAKE) -C src server

clean:
	$(MAKE) -C src clean
	rm unit*.xml

test: all
	$(MAKE) -C test test