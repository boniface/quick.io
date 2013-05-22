#
# Disable implicit rules
#
.SUFFIXES:

#
# Hide annoying messages
#
MAKEFLAGS += -rR --no-print-directory

#
# Where the different files live
#
APP = app
LIB = lib
SRC = src
TEST = test
TEST_APP = $(TEST)/app
TOOLS = tools

BUILD_APP_DIR = $(BUILD_DIR)/$(APP)
BUILD_APP_DIR = $(BUILD_DIR)/$(APP)
BUILD_LIB_DIR = $(BUILD_DIR)/$(LIB)
BUILD_SRC_DIR = $(BUILD_DIR)/$(SRC)
BUILD_TEST_DIR = $(BUILD_DIR)/$(TEST)
BUILD_TEST_APP_DIR = $(BUILD_DIR)/$(TEST_APP)

BUILD_DIRS = \
	$(BUILD_DIR) \
	$(BUILD_APP_DIR) \
	$(BUILD_LIB_DIR) \
	$(BUILD_SRC_DIR) \
	$(BUILD_TEST_DIR) \
	$(BUILD_TEST_APP_DIR)

#
# Which apps to build by default
#
APPS ?= \
	cluster.so

#
# External dependencies for the server
#
DEPENDENCIES ?= \
	$(LIB)/http_parser.o \
	$(LIB)/reconnecting_socket.o

#
# All of the objects to build by default
#
OBJECTS ?= \
	$(patsubst %.c, $(BUILD_DIR)/%.o, $(wildcard $(SRC)/*.c)) \
	$(patsubst %.o, $(BUILD_DIR)/%.o, $(DEPENDENCIES))

# Extra utilities used for testing
TEST_UTILS_OBJECTS ?= \
	$(BUILD_TEST_DIR)/utils.o \
	$(BUILD_TEST_DIR)/utils_stats.o \

# Includes the full test suite runner
TEST_OBJECTS ?= \
	$(TEST_UTILS_OBJECTS) \
	$(BUILD_TEST_DIR)/test.o

# The apps in test/app
TEST_APP_OBJECTS ?= \
	$(patsubst %.c, $(BUILD_DIR)/%.so, $(wildcard $(TEST_APP)/*.c))

# The apps in app/
APP_OBJECTS ?= \
	$(patsubst %.so, $(BUILD_APP_DIR)/%.so, $(APPS))

# The test apps in app/test_*
APP_TESTS ?= \
	$(patsubst %.c, $(BUILD_DIR)/%.so, $(wildcard $(APP)/test_*.c))

#
# Server dependencies
#
LIBS ?= glib-2.0 gmodule-2.0 openssl
LIBS_REQUIREMENTS ?= glib-2.0 >= 2.32 gmodule-2.0 >= 2.32 openssl >= 1

#
# The name of the binary we're building
#
BINARY = quickio
TARGET = $(BUILD_DIR)/$(BINARY)

#
# Default configuration name
#
QIOINI ?= quickio.ini
BUILD_QIOINI = $(BUILD_DIR)/quickio.ini

RUNAPPTESTS = $(BUILD_DIR)/runapptests

#
# Where all of the different build files are placed
#
BUILD_DIR_APP_TEST = build_app_test
BUILD_DIR_DEBUG = build_debug
BUILD_DIR_PROFILE = build_profile
BUILD_DIR_RELEASE = build_release
BUILD_DIR_TEST = build_test
BUILD_DIR_VALGRIND = build_valgrind

#
# When the object files depend on a directory that already exists, they will all be rebuilt
# every time, so only depend on the directories when they don't exist.
#
BUILD_DEP_DIR = $(filter-out $(wildcard $(BUILD_DIR)), $(BUILD_DIR))
BUILD_APP_DEP_DIR = $(filter-out $(wildcard $(BUILD_APP_DIR)), $(BUILD_APP_DIR))
BUILD_LIB_DEP_DIR = $(filter-out $(wildcard $(BUILD_LIB_DIR)), $(BUILD_LIB_DIR))
BUILD_SRC_DEP_DIR = $(filter-out $(wildcard $(BUILD_SRC_DIR)), $(BUILD_SRC_DIR))
BUILD_TEST_APP_DEP_DIR = $(filter-out $(wildcard $(BUILD_TEST_APP_DIR)), $(BUILD_TEST_APP_DIR))
BUILD_TEST_DEP_DIR = $(filter-out $(wildcard $(BUILD_TEST_DIR)), $(BUILD_TEST_DIR))

#
# Build Options
#
CC = gcc

CFLAGS ?= \
	-g \
	-std=gnu99 \
	-Wall \
	-Werror=format-security \
	-Wformat \
	-Wformat-security \
	-Woverride-init \
	-Wsign-compare \
	-Wtype-limits \
	-Wuninitialized \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-I. \
	-I$(SRC) \
	$(shell pkg-config --cflags $(LIBS))

CFLAGS_OBJ ?= \
	$(CFLAGS) \
	-c \
	-fvisibility=hidden

CFLAGS_APP ?= \
	$(CFLAGS) \
	-fPIC \
	-shared

CFLAGS_DEBUG ?= \
	-rdynamic \
	-fno-inline \
	-fno-default-inline \
	-DCOMPILE_DEBUG=1

CFLAGS_TEST ?= \
	-fno-inline \
	-fno-default-inline \
	--coverage \
	-DTESTING=1

CFLAGS_APP_TEST ?= \
	$(CFLAGS_APP) \
	-DAPP_TESTING \
	-DAPP_DEBUG

LDFLAGS ?= \
	-Wl,-E \
	-Wl,-z,relro

LD_LIBS ?= \
	$(shell pkg-config --libs $(LIBS))

#
# .deb configuration
#

DPKG_BUILDPACKAGE_ARGS = \
	-uc \
	-us \
	-tc \
	-I.git* \
	-I*sublime* \
	-Iapp/example

#
# Coverage Configuration
#
GCOVR = ./tools/gcovr

GCOVR_ARGS = \
	-p

GCOVR_SRC_ARGS = $(GCOVR_ARGS) \
	-r build_test/src/ \
	--exclude="src/log.c" \
	--exclude="(src/)?lib/.*" \
	--exclude="(src/)?lib/.*" \
	--exclude="src/test/.*" \
	--exclude="test/test/.*" \
	--exclude="test/.*" \

GCOVR_APP_ARGS = $(GCOVR_ARGS) \
	-r test/

GCOVR_JENKINS_ARGS = \
	-x

GCOVR_JENKINS_SRC_ARGS = \
	$(GCOVR_SRC_ARGS) \
	$(GCOVR_JENKINS_ARGS) \
	-o $(BUILD_DIR)/test_coverage.xml

GCOVR_JENKINS_APP_ARGS = \
	$(GCOVR_APP_ARGS) \
	$(GCOVR_JENKINS_ARGS) \
	-o $(BUILD_DIR)/test_coverage_apps.xml

#
# High-level rules
# -------------------------------------------------------------------------
#

.PHONY: app debug docs test

all: debug

clean:
	rm -rf build_*
	rm -f *.gcno *.gcda
	rm -f gmon.out
	$(MAKE) -C docs clean

clean-all: clean
	$(MAKE) -C client/c clean
	$(MAKE) -C client/java clean
	$(MAKE) -C tools clean

deb:
	dpkg-buildpackage $(DPKG_BUILDPACKAGE_ARGS)
	lintian

deps:
	@echo '-------- Checking compile requirements --------'
	pkg-config --exists '$(LIBS_REQUIREMENTS)'
	@echo '-------- All good. Compiling... --------'

docs:
	$(MAKE) -C docs html
	doxygen

docs-watch:
	while [ true ]; do inotifywait -r docs; $(MAKE) docs; sleep .5; done

debug: export BUILD_DIR ?= $(BUILD_DIR_DEBUG)
debug: export QIOINI = quickio.ini
debug: export CFLAGS += $(CFLAGS_DEBUG)
debug: deps _build

profile: export BUILD_DIR = $(BUILD_DIR_PROFILE)
profile: export CFLAGS += -pg -O2 -DPROFILING=1
profile: deps _build

release: export BUILD_DIR ?= $(BUILD_DIR_RELEASE)
release: export CFLAGS += -O2
release: deps app _build
	strip -s $(BUILD_DIR)/$(BINARY)
	find -name '*.so' -exec strip -s {} \;

run: debug
	$(BUILD_DIR_DEBUG)/$(BINARY)

test: export BUILD_DIR ?= $(BUILD_DIR_TEST)
test: test-build
	@G_SLICE=debug-blocks $(BUILD_DIR)/quickio
	@$(GCOVR) $(GCOVR_SRC_ARGS)
	@$(GCOVR) $(GCOVR_APP_ARGS)

test-apps: export BUILD_DIR ?= $(BUILD_DIR_DEBUG)
test-apps: export LIBS += check
test-apps: export LIBS_REQUIREMENTS += check < 0.9.9
test-apps:
	$(MAKE) _test-apps

test-build: export CFLAGS += $(CFLAGS_TEST)
test-build: export OBJECTS += $(TEST_OBJECTS)
test-build: export APP_OBJECTS += $(TEST_APP_OBJECTS)
test-build: export QIOINI = quickio.test.ini
test-build: export LIBS += check
test-build: export LIBS_REQUIREMENTS += check < 0.9.9
test-build: deps _build

test-jenkins: export BUILD_DIR ?= $(BUILD_DIR_TEST)
test-jenkins: export CFLAGS += -DTEST_OUTPUT_XML=1
test-jenkins: clean test-build
	@G_SLICE=debug-blocks $(BUILD_DIR)/quickio
	@$(GCOVR) $(GCOVR_JENKINS_SRC_ARGS)
	@$(GCOVR) $(GCOVR_JENKINS_APP_ARGS)

test-valgrind: export BUILD_DIR = $(BUILD_DIR_VALGRIND)
test-valgrind: export TEST_OBJECTS = $(TEST_UTILS_OBJECTS) $(BUILD_TEST_DIR)/valgrind.o
test-valgrind: clean test-build
	@G_SLICE=always-malloc G_DEBUG=gc-friendly valgrind --quiet --tool=memcheck --leak-check=full --leak-resolution=high --num-callers=20 $(VALGRIND_XML) $(BUILD_DIR)/quickio

#
# Rules that need to be executed with $(MAKE): they depend on updated
# variables only set at make instantiation
# -------------------------------------------------------------------------
#

_build:
	$(MAKE) $(TARGET)

_test-apps: debug $(APP_TESTS) $(RUNAPPTESTS)
	$(RUNAPPTESTS) $(BUILD_DIR)/quickio $(APP_TESTS)

#
# Compilation rules
# -------------------------------------------------------------------------
#

$(TARGET): $(BUILD_DIR_DEP) $(BUILD_QIOINI) $(OBJECTS) $(APP_OBJECTS)
	@echo '-------- Compiling server --------'
	@$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS) $(LD_LIBS)

$(BUILD_LIB_DIR)/%.o: $(LIB)/%.c $(LIB)/%.h $(BUILD_LIB_DEP_DIR)
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS_OBJ) $< -o $@

$(BUILD_SRC_DIR)/%.o: $(SRC)/%.c $(SRC)/%.h test/test_%.c $(BUILD_SRC_DEP_DIR)
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS_OBJ) $< -o $@

$(BUILD_TEST_DIR)/%.o: $(TEST)/%.c $(TEST)/%.h $(BUILD_TEST_DEP_DIR)
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS_OBJ) $< -o $@

$(BUILD_TEST_APP_DIR)/%.so: $(TEST_APP)/%.c $(BUILD_TEST_APP_DEP_DIR)
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS) $(CFLAGS_APP) $< -o $@

$(BUILD_TEST_DIR)/valgrind.o: $(TEST)/valgrind.c $(BUILD_TEST_DEP_DIR)
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS_OBJ) $< -o $@

$(RUNAPPTESTS): $(TOOLS)/runapptests.c
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LD_LIBS)

$(BUILD_DIRS):
	@mkdir -p $@

$(BUILD_QIOINI): $(QIOINI) $(BUILD_DEP_DIR)
	@cp $< $@

#
# App-specific compilation rules
# -------------------------------------------------------------------------
#

$(BUILD_APP_DIR)/cluster.so: $(APP)/cluster.c $(BUILD_APP_DEP_DIR)
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS_APP) -Iclient/c/ $< client/c/quickio.c -o $@ $(LDFLAGS) $(shell pkg-config --libs libevent_pthreads glib-2.0) -lm

$(BUILD_APP_DIR)/test_cluster.so: $(APP)/test_cluster.c $(APP)/cluster.c $(BUILD_APP_DEP_DIR)
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS_APP_TEST) -Iclient/c/ $< client/c/quickio.c -o $@ $(LDFLAGS) $(shell pkg-config --libs libevent_pthreads glib-2.0) -lm
