LIBS = glib-2.0 libsoup-2.4
CFLAGS = -std=gnu99 -Wall -pedantic $(shell pkg-config --cflags $(LIBS))
LDFLAGS = $(shell pkg-config --libs $(LIBS)) #-static
CC = gcc

TARGET = server
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

.PHONY: default $(TARGET) clean

default: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o ../$@

clean:
	rm $(OBJECTS)
	rm ../$(TARGET)