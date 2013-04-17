include Makefile.mk

.PHONY: all build clean debug deps docs test

all: debug

run: debug
	$(BUILD_DIR_DEBUG)/$(BINARY)

deps:
	@echo '-------- Checking compile requirements --------'
	pkg-config --exists '$(LIBS_REQUIREMENTS)'
	@echo '-------- All good. Compiling... --------'

build:
	$(MAKE) -C src

debug: export BUILD_DIR = $(BUILD_DIR_DEBUG)
debug: export QIOINI = quickio.ini
debug: export CFLAGS += -rdynamic -fno-inline -fno-default-inline -DCOMPILE_DEBUG=1
debug: deps build

profile: export BUILD_DIR = $(BUILD_DIR_PROFILE)
profile: export CFLAGS += -pg -DCOMPILE_PROFILE=1
profile: deps build

release: export BUILD_DIR ?= $(BUILD_DIR_RELEASE)
release: export CFLAGS += -O2
release: export LDFLAGS += -O2
release: deps build
	strip -s -o $(BUILD_DIR)/$(BINARY) $(BUILD_DIR)/$(BINARY)

test: export BUILD_DIR ?= $(BUILD_DIR_TEST)
test: export OBJECTS += $(BUILD_DIR)/test.o
test: test-build
	@G_SLICE=debug-blocks $(BUILD_DIR)/quickio
	@./tools/gcovr $(GCOVR_ARGS_SRC) $(BUILD_DIR)
	@./tools/gcovr $(GCOVR_ARGS_APPS) $(BUILD_DIR)/apps

test-jenkins: export BUILD_DIR = $(BUILD_DIR_TEST)
test-jenkins: export CFLAGS += -DTEST_OUTPUT_XML=1 -DTESTING=1
test-jenkins: clean test
	@./tools/gcovr $(GCOVR_ARGS_SRC) -x -o $(BUILD_DIR)/test_coverage.xml --exclude='src/qsys*' --exclude='src/main*' $(BUILD_DIR)
	@./tools/gcovr $(GCOVR_ARGS_APPS) -x -o $(BUILD_DIR)/test_coverage_apps.xml $(BUILD_DIR)/apps

test-valgrind: export BUILD_DIR = $(BUILD_DIR_VALGRIND)
test-valgrind: export OBJECTS += $(BUILD_DIR)/valgrind.o
test-valgrind: clean test-build
	@G_SLICE=always-malloc G_DEBUG=gc-friendly valgrind --quiet --tool=memcheck --leak-check=full --leak-resolution=high --num-callers=20 $(VALGRIND_XML) $(BUILD_DIR)/quickio

test-build: export CFLAGS += -fno-inline -fno-default-inline --coverage -DTESTING=1
test-build: export LDFLAGS += --coverage
test-build: export OBJECTS += $(BUILD_DIR)/utils.o $(BUILD_DIR)/utils_stats.o
test-build: export QIOINI = quickio.test.ini
test-build: export LIBS += check
test-build: export LIBS_REQUIREMENTS += check < 0.9.9
test-build: deps
	$(MAKE) -C app
	$(MAKE) -C src

deb:
	dpkg-buildpackage -uc -us -tc -I.git* -I*sublime* -Ivars
	lintian

docs:
	$(MAKE) -C docs html
	doxygen

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BUILD_DIR_DEBUG)
	rm -rf $(BUILD_DIR_PROFILE)
	rm -rf $(BUILD_DIR_RELEASE)
	rm -rf $(BUILD_DIR_TEST)
	rm -rf $(BUILD_DIR_VALGRIND)
	rm -f test*.xml
	$(MAKE) -C docs clean
	$(MAKE) -C tools clean
