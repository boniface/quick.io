include ../Makefile.inc

.PHONY: clean test

pound: pound.c
	gcc -g $(CFLAGS) $(LDFLAGS) pound.c -o pound

test:
	$(MAKE) -C python test

clean:
	$(MAKE) -C python clean
	rm -f pound