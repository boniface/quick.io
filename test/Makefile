include ../Makefile.inc
include ../src/Makefile

CFLAGS += -I../src -DTESTING=1
LIBCHECK = /usr/lib/libcheck.a
OBJECTS = $(BUILDDIR)/test.o $(BUILDDIR)/utils.o $(BUILDDIR)/utils_locks.o $(BUILDDIR)/utils_stats.o
QIO_OBJECTS = $(filter-out $(OBJECTS), $(wildcard $(BUILDDIR)/*.o))
TEST_OUTPUT_XML = 0

.PHONY: clean test

client_statuses.inc: ../src/client.h
	$(CC) $(CFLAGS) -E -Wp,-dM $< | grep 'define CLIENT_' | grep -v 'CLIENT_BAD ' | gawk '{ printf "case %d: return \"%s\";", lshift(1,$$5), $$2 }' > client_statuses.inc

test: client_statuses.inc $(OBJECTS)
	@echo "\n\n"-------- Compiling test suite --------
	@$(CC) $(LDFLAGS) $(OBJECTS) $(LIBCHECK) $(QIO_OBJECTS) --coverage -o $(BUILDDIR)/test
	
	@echo "\n\n"-------- Running test suite --------
	@$(BUILDDIR)/test

clean:
	rm -f client_statuses.inc

$(BUILDDIR)/%.o: %.c test.h
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) -DTEST_OUTPUT_XML=$(TEST_OUTPUT_XML) $< -o $@