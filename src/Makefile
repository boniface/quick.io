include ../Makefile.inc

# Hide everything by default, unless annotated with APP_EXPORT
CFLAGS += -I. -c -fvisibility=hidden
LDFLAGS += -Wl,-E

OBJECTS = $(patsubst %.c, $(BUILDDIR)/%.o, $(wildcard *.c))
TARGET = $(BUILDDIR)/qio
TESTING = 0

.PHONY: default clean

default: $(TARGET)

dependencies:
	@$(MAKE) -C ../lib deps

ifeq ($(TESTING),1)
$(TARGET): dependencies $(OBJECTS)
else
$(TARGET): dependencies $(OBJECTS)
	@echo "\n\n"-------- Compiling server --------
	@$(CC) $(OBJECTS) $(DEPS) -o $@ $(LDFLAGS)
endif

$(BUILDDIR)/%.o: %.c %.h ../test/test_%.c
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@