include ../Makefile.inc

# Hide everything by default, unless annotated with MODULE_EXPORT
CFLAGS += -I. -c -fvisibility=hidden
LDFLAGS += -Wl,-E

OBJECTS = $(patsubst %.c, $(BUILDDIR)/%.o, $(wildcard *.c))
TARGET = $(BUILDDIR)/server
TESTING = 0

.PHONY: default clean

default: $(TARGET)

# If compiling in shared mode, make sure we have coverage support
ifeq ($(TESTING),1)
CFLAGS += --coverage -DTESTING=1
$(TARGET): $(OBJECTS)
else
$(TARGET): $(OBJECTS)
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(LDFLAGS) $^ -o $@
endif

$(BUILDDIR)/%.o: %.c %.h ../test/test_%.c
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@