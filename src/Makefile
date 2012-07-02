include ../Makefile.inc

TARGET = server
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

.PHONY: default clean

default: $(ARSOCKLIB)

$(ARSOCKLIB): $(OBJECTS)
	$(AR) rcs $(ARSOCKLIB) $^

clean:
	-rm $(ARSOCKLIB)
	-rm $(OBJECTS)
	-rm $(TARGET)