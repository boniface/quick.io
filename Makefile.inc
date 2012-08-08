# Disable implicit rules
.SUFFIXES:

# Not directory-specific
HEADERS = $(wildcard *.h)

DIR_BUILD = build
DIR_BUILD_TEST = build_test

# The output directory
export BUILDDIR ?= $(shell pwd)/$(DIR_BUILD)

CC = gcc
LIBS += glib-2.0 libsoup-2.4
LIBS_VERSIONS = glib-2.0 >= 2.32 libsoup-2.4 >= 2.38

# The flags
CFLAGS = -O2 -std=gnu99 -Wall -Woverride-init -Wsign-compare -Wtype-limits -Wuninitialized $(shell pkg-config --cflags $(LIBS))
LDFLAGS = $(shell pkg-config --libs $(LIBS))

# If we should include debug everything
DEBUG_FLAGS = 
ifneq ($(DEBUG),)
	DEBUG_FLAGS = -g -rdynamic
	CFLAGS += $(DEBUG_FLAGS) -DCOMPILE_DEBUG=1
endif