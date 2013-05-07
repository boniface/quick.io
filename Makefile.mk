# Disable implicit rules
.SUFFIXES:

# Not directory-specific
HEADERS = $(wildcard *.h)

# Because we need it for check...
ARCH := $(shell getconf LONG_BIT)

export ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

BUILD_DIR_DEBUG = $(ROOT)/build_debug
BUILD_DIR_PROFILE = $(ROOT)/build_profile
BUILD_DIR_RELEASE = $(ROOT)/build_release
BUILD_DIR_TEST = $(ROOT)/build_test
BUILD_DIR_APP_TEST = $(ROOT)/build_app_test
BUILD_DIR_VALGRIND = $(ROOT)/build_valgrind

GCOVR = ./tools/gcovr

BINARY = quickio
QIOINI_NAME = quickio.ini
QIOINI ?= $(QIOINI_NAME)

CC = gcc
LIBS += glib-2.0 gmodule-2.0 openssl
LIBS_REQUIREMENTS += glib-2.0 >= 2.32 gmodule-2.0 >= 2.32 openssl >= 1

CFLAGS += -g -std=gnu99 -Wall -Woverride-init -Wsign-compare -Wtype-limits -Wuninitialized -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security -I. -I$(ROOT)/lib/ $(shell pkg-config --cflags $(LIBS)) -I$(ROOT)/lib/
CFLAGS_OBJ ?= -c -fvisibility=hidden
LDFLAGS += $(shell pkg-config --libs $(LIBS)) -Wl,-E

DEPENDENCIES = \
	http_parser.o \
	reconnecting_socket.o

GCOVR_ARGS = -p -r . --exclude='src/log.*' --exclude='test.*' $(patsubst %.o, --exclude='src/%.c', $(DEPENDENCIES)) --exclude='lib.*' --single-directory
GCOVR_ARGS_SRC = $(GCOVR_ARGS) --object-directory=src/
GCOVR_ARGS_APPS = $(GCOVR_ARGS) --object-directory=$(ROOT)/app/
