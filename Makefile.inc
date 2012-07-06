# Disable implicit rules
.SUFFIXES:

# Not directory-specific
HEADERS = $(wildcard *.h)

# The output directory
BUILDDIR = build/

CC = gcc
LIBS = glib-2.0 libsoup-2.4 libmemcached

# The flags
CFLAGS = -std=gnu99 -Wall -Woverride-init -Wsign-compare -Wtype-limits -Wuninitialized  $(shell pkg-config --cflags $(LIBS))

# If we should include debug everything
DEBUG_FLAGS = 
ifneq ($(DEBUG),)
	DEBUG_FLAGS = -g -rdynamic
	CFLAGS += $(DEBUG_FLAGS) -DCOMPILE_DEBUG=1
endif