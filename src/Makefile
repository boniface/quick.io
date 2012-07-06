include ../Makefile.inc

CFLAGS += -c
LDFLAGS = -Wl,-E $(shell pkg-config --libs $(LIBS))
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