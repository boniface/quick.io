include Makefile.mk

.PHONY: all build clean debug deps docs test

all: debug

run: debug
	$(BUILD_DIR_DEBUG)/qio

deps:
	@echo '-------- Checking compile requirements --------'
	pkg-config --exists '$(LIBS_REQUIREMENTS)'
	@echo '-------- All good. Compiling... --------'

build:
	$(MAKE) -C src

debug: export BUILD_DIR = $(BUILD_DIR_DEBUG)
debug: export QIOINI = quickio.ini
debug: export CFLAGS += -g -rdynamic -fno-inline -fno-default-inline -DCOMPILE_DEBUG=1
debug: deps build

profile: export BUILD_DIR = $(BUILD_DIR_PROFILE)
profile: export CFLAGS += -g -DCOMPILE_PROFILE=1
profile: deps build

release: CFLAGS += -O2
release: LDFLAGS += -O2
release: deps build

test: export BUILD_DIR ?= $(BUILD_DIR_TEST)
test: export OBJECTS += $(BUILD_DIR)/test.o
test: deps test-build
	@G_SLICE=debug-blocks $(BUILD_DIR)/qio
	@./tools/gcovr $(GCOVR_ARGS_SRC) $(BUILD_DIR)
	@./tools/gcovr $(GCOVR_ARGS_APPS) $(BUILD_DIR)/apps

test-jenkins: export BUILD_DIR = $(BUILD_DIR_TEST)
test-jenkins: export CFLAGS += -DTEST_OUTPUT_XML=1 -DTESTING=1
test-jenkins: clean deps test
	@./tools/gcovr $(GCOVR_ARGS_SRC) -x -o $(BUILD_DIR)/test_coverage.xml --exclude='src/qsys*' --exclude='src/main*' $(BUILD_DIR)
	@./tools/gcovr $(GCOVR_ARGS_APPS) -x -o $(BUILD_DIR)/test_coverage_apps.xml $(BUILD_DIR)/apps

test-valgrind: export BUILD_DIR = $(BUILD_DIR_VALGRIND)
test-valgrind: export OBJECTS += $(BUILD_DIR)/valgrind.o
test-valgrind: clean deps test-build
	@G_SLICE=always-malloc G_DEBUG=gc-friendly valgrind --quiet --tool=memcheck --leak-check=full --leak-resolution=high --num-callers=20 $(VALGRIND_XML) $(BUILD_DIR)/qio

test-build: export CFLAGS += -g -fno-inline -fno-default-inline --coverage -DTESTING=1
test-build: export LDFLAGS += ../lib/check/libcheck$(ARCH).a --coverage
test-build: export OBJECTS += $(BUILD_DIR)/utils.o $(BUILD_DIR)/utils_stats.o
test-build: export QIOINI = quickio.test.ini
test-build:
	$(MAKE) -C app
	$(MAKE) -C src

docs:
	$(MAKE) -C docs html
	doxygen

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BUILD_DIR_DEBUG)
	rm -rf $(BUILD_DIR_PROFILE)
	rm -rf $(BUILD_DIR_TEST)
	rm -rf $(BUILD_DIR_VALGRIND)
	rm -f test*.xml
	$(MAKE) -C docs clean
	$(MAKE) -C tools clean
	$(MAKE) -C tools clean