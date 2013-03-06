include ../Makefile.inc
include ../src/Makefile

CFLAGS += -I../src -I../lib/check/ -DTESTING=1
LDFLAGS += --coverage
LIBCHECK = ../lib/check/libcheck$(ARCH).a
TARGETS = $(BUILDDIR)/test.o $(BUILDDIR)/valgrind.o
OBJECTS = $(BUILDDIR)/utils.o $(BUILDDIR)/utils_stats.o
QIO_OBJECTS = $(filter-out $(OBJECTS) $(TARGETS), $(wildcard $(BUILDDIR)/*.o))
TEST_OUTPUT_XML = 0

ifneq ($(TEST_OUTPUT_XML),0)
	VALGRIND_XML = --xml=yes --xml-file=$(BUILDDIR)/test_valgrind.xml
endif

.PHONY: clean test

test: $(BUILDDIR)/test.o $(OBJECTS)
	@echo "\n\n"-------- Compiling test suite --------
	@$(CC) $^ $(LIBCHECK) $(QIO_OBJECTS) -o $(BUILDDIR)/test $(LDFLAGS)
	
	@echo "\n\n"-------- Running test suite --------
	@$(BUILDDIR)/test

valgrind: $(BUILDDIR)/valgrind.o $(OBJECTS)
	@echo "\n\n"-------- Compiling valgrind --------
	@$(CC) $^ $(LIBCHECK) $(QIO_OBJECTS) -o $(BUILDDIR)/valgrind $(LDFLAGS)
	
	@echo "\n\n"-------- Running valgrind suite --------
	@G_SLICE=always-malloc G_DEBUG=gc-friendly valgrind --quiet --tool=memcheck --leak-check=full --leak-resolution=high --num-callers=20 $(VALGRIND_XML) $(BUILDDIR)/valgrind

# Compiles test.c to test.o
$(BUILDDIR)/%.o: %.c test.h
	@echo "\n\n"-------- Compiling $< --------
	@# Compile using WD so that gcov can easily filter these out
	@$(CC) $(CFLAGS) -DTEST_OUTPUT_XML=$(TEST_OUTPUT_XML) $(WD)/$< -o $@