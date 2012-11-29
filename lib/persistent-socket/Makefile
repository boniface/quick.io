include ../../Makefile.inc

CFLAGS += -fPIC -c

all: $(BUILDDIR)/persistent_socket.o
	
$(BUILDDIR)/persistent_socket.o: persistent_socket.c persistent_socket.h
	$(CC) $(CFLAGS) $< -o $@