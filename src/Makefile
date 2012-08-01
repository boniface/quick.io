include ../Makefile.inc

# Hide everything by default, unless annotated with MODULE_EXPORT
CFLAGS += -c -fvisibility=hidden
LDFLAGS += -Wl,-E

TARGET = $(CURDIR)/../$(BUILDDIR)/server
OBJECTS = $(patsubst %.c, $(OBJPATH)%.o, $(wildcard *.c))

.PHONY: default clean

default: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f $(OBJECTS)

%.o: %.c %.h
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@