include ../Makefile.inc

# Hide everything by default, unless annotated with MODULE_EXPORT
CFLAGS += -c -fvisibility=hidden
LDFLAGS += -Wl,-E

OBJECTS = $(patsubst %.c, $(OBJPATH)%.o, $(wildcard *.c))
TARGET = $(CURDIR)/../$(BUILDDIR)/server
TESTING = 0

# If compiling in shared mode, make sure we have coverage support
ifeq ($(TESTING),1)
	CFLAGS += --coverage
endif

.PHONY: default clean

default: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(LDFLAGS) $^ -o $@

test:
	@echo "\n\n"-------- Creating archive --------
	@$(MAKE) $(OBJECTS) TESTING=1
	@ar rcs qio.a $(OBJECTS)

clean:
	rm -f qio.a
	rm -f $(OBJECTS)
	rm -f *.gcda *.gcno

%.o: %.c %.h
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) -DTESTING=$(TESTING) $< -o $@