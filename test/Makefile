include ../Makefile.inc

CFLAGS += -I../src -c
LIBCHECK = /usr/lib/libcheck.a
OBJECTS = $(patsubst %.c, $(BUILDDIR)/%.o, $(wildcard test*.c))
QIO_OBJECTS = $(filter-out $(OBJECTS), $(wildcard $(BUILDDIR)/*.o))
TEST_OUTPUT_XML = 0

.PHONY: clean test

pound: pound.c
	gcc -g $(CFLAGS) $(LDFLAGS) pound.c -o pound

test: $(OBJECTS)
	@echo "\n\n"-------- Compiling test suite --------
	@$(CC) $(LDFLAGS) $(OBJECTS) $(LIBCHECK) $(QIO_OBJECTS) --coverage -o $(BUILDDIR)/test
	
	@echo "\n\n"-------- Running test suite --------
	@$(BUILDDIR)/test
	@# $(MAKE) -C python test

clean:
	$(MAKE) -C python clean

$(BUILDDIR)/%.o: %.c
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) -DTEST_OUTPUT_XML=$(TEST_OUTPUT_XML) $< -o $@