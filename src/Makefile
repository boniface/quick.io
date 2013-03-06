include ../Makefile.inc

# Hide everything by default, unless annotated with APP_EXPORT
CFLAGS += -I. -c -fvisibility=hidden -I$(ROOT)/lib/
LDFLAGS += -Wl,-E

OBJECTS = $(patsubst %.c, $(BUILDDIR)/%.o, $(wildcard *.c)) $(BUILDDIR)/http_parser.o $(BUILDDIR)/reconnecting_socket.o
TARGET = $(BUILDDIR)/qio

.PHONY: default clean

default: $(TARGET)

ifdef TESTING
$(TARGET): $(OBJECTS)
else
$(TARGET): $(OBJECTS)
	@echo "\n\n"-------- Compiling server --------
	@$(CC) $^ -o $@ $(LDFLAGS)
endif

$(BUILDDIR)/http_parser.o:
	@$(MAKE) -C ../lib deps
	
$(BUILDDIR)/reconnecting_socket.o:
	@$(MAKE) -C ../lib deps

$(BUILDDIR)/%.o: %.c %.h ../test/test_%.c
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@