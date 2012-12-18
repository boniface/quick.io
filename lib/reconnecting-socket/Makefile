include ../../Makefile.inc

CFLAGS += -fPIC -c

all: $(BUILDDIR)/reconnecting_socket.o
	
$(BUILDDIR)/reconnecting_socket.o: reconnecting_socket.c reconnecting_socket.h
	@echo "\n\n"-------- Compiling $< --------
	@$(CC) $(CFLAGS) $< -o $@