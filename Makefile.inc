# Disable implicit rules
.SUFFIXES:

# Not directory-specific
HEADERS = $(wildcard *.h)

# Because we need it for check...
ARCH := $(shell getconf LONG_BIT)

export ROOT ?= $(shell pwd)
DIR_BUILD = build
DIR_BUILD_DEBUG = build_debug
DIR_BUILD_PROFILE = build_profile
DIR_BUILD_TEST = build_test
QIOINI_DEFAULT = quickio.ini
QIOINI = $(QIOINI_DEFAULT)

# The output directory
export BUILDDIR ?= $(ROOT)/$(DIR_BUILD)

CC = gcc
LIBS += glib-2.0 gmodule-2.0 openssl
LIBS_VERSIONS = glib-2.0 >= 2.32 gmodule-2.0 >= 2.32 openssl >= 1

CFLAGS = -std=gnu99 -Wall -Woverride-init -Wsign-compare -Wtype-limits -Wuninitialized $(shell pkg-config --cflags $(LIBS)) -I$(ROOT)/lib/
LDFLAGS = $(shell pkg-config --libs $(LIBS))

DEPENDENCIES = \
	http_parser.o \
	reconnecting_socket.o \
	ring_queue.o

DEPS = $(patsubst %.o, $(BUILDDIR)/%.o, $(DEPENDENCIES))

# If we should include debug everything
ifdef DEBUG
	QIOINI = quickio.ini
	OPTIMIZE = 0
	CFLAGS += -g -rdynamic -fno-inline -fno-default-inline -DCOMPILE_DEBUG=1
endif

# If we should include gprof stuffs
ifdef PROFILE
	CFLAGS += -g -DCOMPILE_PROFILE=1
endif

#From: http://stackoverflow.com/questions/3432542/how-can-i-get-more-accurate-results-from-gcov
ifdef TESTING
	QIOINI = quickio.test.ini
	OPTIMIZE = 0
	CFLAGS += -fno-inline -fno-default-inline --coverage -DTESTING=1
endif

# Don't optimize when debugging, testing, etc
# Gives better information about coverage and everything
ifndef OPTIMIZE
	CFLAGS += -O2
	LDFLAGS += -O2
endif