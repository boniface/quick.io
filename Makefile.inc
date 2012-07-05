LIBS = glib-2.0 libsoup-2.4
CFLAGS = -c -std=gnu99 -Wall -Woverride-init -Wsign-compare -Wtype-limits -Wuninitialized  $(shell pkg-config --cflags $(LIBS))
DEBUG_FLAGS = 
LDFLAGS = $(shell pkg-config --libs $(LIBS))
CC = gcc

ifneq ($(DEBUG),)
	DEBUG_FLAGS = -g -rdynamic
	CFLAGS += $(DEBUG_FLAGS) -DCOMPILE_DEBUG=1
endif

CSOCKLIB = csocketio
ARSOCKLIB = lib$(CSOCKLIB).a

TARGET = server
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c %.h
	$(CC) $(CFLAGS) $< -o $@