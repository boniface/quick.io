include ../Makefile.inc

CFLAGS += -I../src -c
LIBCHECK = /usr/lib/libcheck.a
LIBQIO = ../src/qio.a
OBJECTS = $(patsubst %.c, %.o, $(wildcard test*.c))
TEST_OUTPUT_XML = 0

.PHONY: clean test

pound: pound.c
	gcc -g $(CFLAGS) $(LDFLAGS) pound.c -o pound

test: $(OBJECTS)
	@echo "\n\n"-------- Compiling test --------
	@$(CC) $(LDFLAGS) $(OBJECTS) $(LIBCHECK) $(LIBQIO) --coverage -o test
	./test
	# $(MAKE) -C python test

clean:
	$(MAKE) -C python clean
	rm -f $(OBJECTS)
	rm -f pound
	rm -f test

$(OBJPATH)%.o: %.c
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) -DTEST_OUTPUT_XML=$(TEST_OUTPUT_XML) $< -o $@