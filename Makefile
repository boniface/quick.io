.PHONY: all clean

all:
	$(MAKE) -C src server

clean:
	$(MAKE) -C src clean