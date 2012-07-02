LIBS = glib-2.0 libsoup-2.4
CFLAGS = -c -O2 -std=gnu99 -Wall $(shell pkg-config --cflags $(LIBS))
LDFLAGS = $(shell pkg-config --libs $(LIBS))
CC = gcc

CSOCKLIB = csocketio
ARSOCKLIB = lib$(CSOCKLIB).a

TARGET = server
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c %.h
	$(CC) $(CFLAGS) $< -o $@