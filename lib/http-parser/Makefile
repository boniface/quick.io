include ../../Makefile.inc

CFLAGS += -fPIC -c

all: $(BUILDDIR)/http_parser.o
	
$(BUILDDIR)/http_parser.o: http_parser.c http_parser.h
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@