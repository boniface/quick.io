include ../../Makefile.inc

CFLAGS += -fPIC -c

all: http_parser.o
	
http_parser.o: http_parser.c http_parser.h
	$(CC) $(CFLAGS) $< -o $@
	
clean:
	rm -f http_parser.o