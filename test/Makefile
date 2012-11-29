include ../Makefile.inc
include ../src/Makefile

CFLAGS += -I../src -I../lib/check/ -DTESTING=1
LDFLAGS += --coverage
LIBCHECK = ../lib/check/libcheck$(ARCH).a
OBJECTS = $(BUILDDIR)/test.o $(BUILDDIR)/utils.o $(BUILDDIR)/utils_locks.o $(BUILDDIR)/utils_stats.o
QIO_OBJECTS = $(filter-out $(OBJECTS), $(wildcard $(BUILDDIR)/*.o))
TEST_OUTPUT_XML = 0

.PHONY: clean test

test: $(OBJECTS)
	@echo "\n\n"-------- Compiling test suite --------
	@$(CC) $(OBJECTS) $(LIBCHECK) $(QIO_OBJECTS) -o $(BUILDDIR)/test $(LDFLAGS)
	
	@echo "\n\n"-------- Running test suite --------
	@$(BUILDDIR)/test

$(BUILDDIR)/%.o: %.c test.h
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) -DTEST_OUTPUT_XML=$(TEST_OUTPUT_XML) $< -o $@