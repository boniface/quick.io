include ../../Makefile.inc
CFLAGS += -fPIC -c -I$(ROOT)/src

OBJECTS = $(patsubst %.c, $(BUILDDIR)/%.o, $(wildcard *.c))

all: $(OBJECTS)

$(BUILDDIR)/%.o: %.c %.h
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@