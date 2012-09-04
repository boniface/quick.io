# Disable implicit rules
.SUFFIXES:

# Not directory-specific
HEADERS = $(wildcard *.h)

ROOT = $(shell pwd)
DIR_BUILD = build
DIR_BUILD_DEBUG = build_debug
DIR_BUILD_TEST = build_test
QIOINI_DEFAULT = quickio.ini
QIOINI = $(QIOINI_DEFAULT)

# The output directory
export BUILDDIR ?= $(ROOT)/$(DIR_BUILD)

CC = gcc
LIBS += glib-2.0 libsoup-2.4
LIBS_VERSIONS = glib-2.0 >= 2.32 libsoup-2.4 >= 2.38

# The flags
CFLAGS = -std=gnu99 -Wall -Woverride-init -Wsign-compare -Wtype-limits -Wuninitialized $(shell pkg-config --cflags $(LIBS))
LDFLAGS = $(shell pkg-config --libs $(LIBS))

# If we should include debug everything
ifdef DEBUG
	QIOINI = quickio.ini
	OPTIMIZE = 0
	CFLAGS += -g -rdynamic -fno-inline -fno-default-inline -DCOMPILE_DEBUG=1
endif

#From: http://stackoverflow.com/questions/3432542/how-can-i-get-more-accurate-results-from-gcov
ifdef TESTING
	QIOINI = quickio.test.ini
	OPTIMIZE = 0
	CFLAGS += -fno-inline -fno-default-inline --coverage -DTESTING=1
endif

# Don't optimize when debugging, profiling, testing, etc
# Gives better information about coverage and everything
ifndef OPTIMIZE
	CFLAGS += -O2
endif