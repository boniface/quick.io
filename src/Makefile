include ../Makefile.inc

CFLAGS += -c
TARGET = $(CURDIR)/../$(BUILDDIR)/server
OBJECTS = $(patsubst %.c, $(OBJPATH)%.o, $(wildcard *.c))

.PHONY: default clean

default: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo -e "\n\n"-------- Compiling $< --------
	@$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f $(OBJECTS)

%.o: %.c %.h
	@echo -e "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@