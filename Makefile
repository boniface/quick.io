.PHONY: clean docs test

#
# Hide annoying messages
#
MAKEFLAGS += --no-print-directory

VERSION_NAME = banished bongo
VERSION_MAJOR = 0
VERSION_MINOR = 2
VERSION_MICRO = 0

#
# If the test cases should be run in valgrind. Comment this out to disable
#
# USE_VALGRIND = 1

ifeq ($(CC),cc)
	export CC = clang
endif

INSTALL_BIN = install
INSTALL = $(INSTALL_BIN) -m 644

LIB_DIR = lib
SRC_DIR = src
TEST_DIR = test
TEST_APPS_DIR = $(TEST_DIR)/apps
QEV_DIR = $(CURDIR)/lib/quick-event

BINARY = quickio
BINARY_TESTAPPS = quickio-testapps

HEADERS = $(shell find $(SRC_DIR) -name '*.h')

OBJECTS = \
	$(LIB_DIR)/http-parser/http_parser.o \
	$(SRC_DIR)/apps.o \
	$(SRC_DIR)/client.o \
	$(SRC_DIR)/config.o \
	$(SRC_DIR)/evs.o \
	$(SRC_DIR)/evs_qio.o \
	$(SRC_DIR)/evs_query.o \
	$(SRC_DIR)/protocols.o \
	$(SRC_DIR)/protocols/flash.o \
	$(SRC_DIR)/protocols/raw.o \
	$(SRC_DIR)/protocols/rfc6455.o \
	$(SRC_DIR)/protocols/stomp.o \
	$(SRC_DIR)/qev.o \
	$(SRC_DIR)/quickio.o \
	$(SRC_DIR)/sub.o

OBJECTS_TEST = \
	$(patsubst %,../%,$(OBJECTS))

BINARY_OBJECTS = \
	$(OBJECTS) \
	$(SRC_DIR)/main.o

TESTS = \
	test_apps \
	test_client \
	test_config \
	test_coverage \
	test_evs \
	test_protocol_flash \
	test_protocol_raw \
	test_protocol_rfc6455 \
	test_sub

TEST_APPS = \
	$(TEST_APPS_DIR)/test_app_fatal_init.so \
	$(TEST_APPS_DIR)/test_app_fatal_exit.so \
	$(TEST_APPS_DIR)/test_app_invalid.so \
	$(TEST_APPS_DIR)/test_app_sane.so

BENCHMARKS = \
	bench_evs_query

LIBS = glib-2.0 gmodule-2.0 openssl
LIBS_TEST = check
LIBQEV = $(QEV_DIR)/libqev.a
LIBQEV_TEST = $(QEV_DIR)/libqev_test.a

CFLAGS = \
	-Wall \
	-Wextra \
	-Werror=format-security \
	-fPIE \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-std=gnu99 \
	-DQIO_SERVER \
	-DVERSION_NAME="$(VERSION_NAME)" \
	-DVERSION_MAJOR=$(VERSION_MAJOR) \
	-DVERSION_MINOR=$(VERSION_MINOR) \
	-DVERSION_MICRO=$(VERSION_MICRO) \
	-I$(CURDIR)/$(LIB_DIR) \
	-I$(CURDIR)/$(SRC_DIR) \
	$(shell pkg-config --cflags $(LIBS))

ifeq ($(CC),gcc)
	CFLAGS += -flto
endif

CFLAGS_TEST = \
	-g \
	--coverage \
	-fno-inline \
	-I../$(SRC_DIR) \
	-DQEV_ENABLE_MOCK \
	-DPORT=$(shell echo $$(((($$$$ % (32766 - 1024)) + 1024) * 2))) \
	$(shell pkg-config --cflags $(LIBS_TEST))

CFLAGS_DEBUG = \
	-g \
	-fno-inline \
	-DQEV_LOG_DEBUG

CFLAGS_RELEASE = \
	-O3

LDFLAGS = \
	-pie \
	-Wl,-z,relro \
	-Wl,-z,now \
	-lm \
	$(shell pkg-config --libs $(LIBS))

LDFLAGS_DEBUG = \
	$(LIBQEV) \
	-g \
	-rdynamic

LDFLAGS_TEST = \
	$(LIBQEV_TEST) \
	-g \
	-rdynamic \
	--coverage \
	$(shell pkg-config --libs $(LIBS_TEST))

LDFLAGS_RELEASE = \
	$(LIBQEV)

ifdef USE_VALGRIND
	CFLAGS += -DFATAL_SIGNAL=9
	MEMTEST = \
		G_SLICE=always-malloc \
		G_DEBUG=gc-friendly \
		valgrind \
			--quiet \
			--suppressions=$(QEV_DIR)/test/valgrind.supp \
			--suppressions=$(QEV_DIR)/test/valgrind_expected.supp \
			--tool=memcheck \
			--leak-check=full \
			--leak-resolution=high \
			--num-callers=20 \
			--track-origins=yes
else
	CFLAGS += -DFATAL_SIGNAL=5
	MEMTEST =
endif

INSTALL_ROOT ?=
INSTALL_PREFIX ?= $(INSTALL_ROOT)/usr
INSTALL_BIN_DIR ?= $(INSTALL_PREFIX)/bin
INSTALL_ETC_DIR ?= $(INSTALL_ROOT)/etc/quickio
INSTALL_APP_LIB_DIR ?= $(INSTALL_ROOT)/usr/lib/quickio
INSTALL_INCLUDE_DIR ?= $(INSTALL_PREFIX)/include/quickio
INSTALL_INCLUDE_PROT_DIR ?= $(INSTALL_PREFIX)/include/quickio/protocols
INSTALL_INCLUDE_QEV_DIR ?= $(INSTALL_PREFIX)/include/quickio/quick-event
INSTALL_INIT_DIR = $(INSTALL_ROOT)/etc/init.d
INSTALL_LIMITS_DIR = $(INSTALL_ROOT)/etc/security/limits.d
INSTALL_PKGCFG_DIR ?= $(INSTALL_PREFIX)/lib/pkgconfig
INSTALL_SYSCTL_DIR = $(INSTALL_ROOT)/etc/sysctl.d

DEBUILD_ARGS = \
	-uc \
	-us \
	-tc \
	-I.git*

all:
	@echo "Choose one of the following:"
	@echo "    make run - run quickio in debug mode"
	@echo "    make clean - clean up everything"

debug: export CFLAGS += $(CFLAGS_DEBUG)
debug: export LDFLAGS += $(LDFLAGS_DEBUG)
debug: $(BINARY)

run: debug
	./$(BINARY)

release: export CFLAGS += $(CFLAGS_RELEASE)
release: export LDFLAGS += $(LDFLAGS_RELEASE)
release: $(BINARY)

deb:
	debuild $(DEBUILD_ARGS)

test: $(TESTS)
	./lib/quick-event/ext/gcovr \
		--root=. \
		--exclude=test \
		--exclude=lib \
		--exclude='.*\.h'

docs:
	$(MAKE) -C docs html
	doxygen

docs-watch:
	while [ true ]; do inotifywait -r docs; $(MAKE) docs; sleep .5; done

install: release $(BINARY_TESTAPPS)
	mkdir -p \
		$(INSTALL_APP_LIB_DIR) \
		$(INSTALL_BIN_DIR) \
		$(INSTALL_ETC_DIR) \
		$(INSTALL_INCLUDE_DIR) \
		$(INSTALL_INCLUDE_PROT_DIR) \
		$(INSTALL_INCLUDE_QEV_DIR) \
		$(INSTALL_LIMITS_DIR) \
		$(INSTALL_PKGCFG_DIR) \
		$(INSTALL_SYSCTL_DIR)
	$(INSTALL_BIN) $(BINARY) $(INSTALL_BIN_DIR)
	$(INSTALL_BIN) $(BINARY_TESTAPPS) $(INSTALL_BIN_DIR)
	$(INSTALL) src/*.h $(INSTALL_INCLUDE_DIR)
	$(INSTALL) src/protocols/*.h $(INSTALL_INCLUDE_PROT_DIR)
	$(INSTALL) lib/quick-event/src/*.h $(INSTALL_INCLUDE_QEV_DIR)
	$(INSTALL) quickio.ini $(INSTALL_ETC_DIR)
	$(INSTALL) quickio.pc $(INSTALL_PKGCFG_DIR)
	$(INSTALL) -D limits.conf $(INSTALL_LIMITS_DIR)/quickio.conf
	$(INSTALL) -D sysctl.conf $(INSTALL_SYSCTL_DIR)/quickio.conf
	$(INSTALL) debian/quickio.init $(INSTALL_INIT_DIR)

uninstall:
	rm -f $(INSTALL_BIN_DIR)/$(BINARY)
	rm -f $(INSTALL_BIN_DIR)/$(BINARY_TESTAPPS)
	rm -rf $(INSTALL_APP_LIB_DIR)
	rm -rf $(INSTALL_ETC_DIR)
	rm -rf $(INSTALL_INCLUDE_DIR)
	rm -f $(INSTALL_LIMITS_DIR)/quickio.conf
	rm -f $(INSTALL_PKGCFG_DIR)/quickio.pc
	rm -f $(INSTALL_SYSCTL_DIR)/quickio.conf

clean:
	find -name '*.gcno' -exec rm {} \;
	find -name '*.gcda' -exec rm {} \;
	find -name '*.xml' -exec rm {} \;
	find test -name 'test_*.ini' -exec rm {} \;
	rm -f $(BINARY_OBJECTS)
	rm -f $(TEST_APPS)
	$(MAKE) -C lib/quick-event/ clean
	rm -f $(patsubst %,$(TEST_DIR)/%,$(TESTS) $(BENCHMARKS))
	rm -f $(BINARY)
	rm -f $(BINARY_TESTAPPS)
	$(MAKE) -C docs clean

$(BINARY): $(BINARY_OBJECTS) $(LIBQEV)
	@echo '-------- Compiling quickio --------'
	@$(CC) $^ -o $@ $(LDFLAGS)

$(BINARY_TESTAPPS): src/quickio-testapps.c
	@echo '-------- Compiling quickio-testapps --------'
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@strip -s $@

.PHONY: $(TESTS)
$(TESTS): % : $(TEST_APPS) $(TEST_DIR)/%
	@cd $(TEST_DIR) && $(MEMTEST) ./$@

.PHONY: $(BENCHMARKS)
$(BENCHMARKS): % : $(TEST_APPS) $(TEST_DIR)/%
	@cd $(TEST_DIR) && ./$@

%.o: %.c $(HEADERS)
	@echo '-------- Compiling $@ --------'
	@$(CC) -c $(CFLAGS) $< -o $@

$(TEST_APPS_DIR)/%.so: $(TEST_APPS_DIR)/%.c $(HEADERS)
	@echo '-------- Compiling app $@ --------'
	@cd $(TEST_APPS_DIR) && $(CC) -shared -fPIC $(CFLAGS) $*.c -o $*.so

$(TEST_DIR)/test_%: export CFLAGS += $(CFLAGS_TEST)
$(TEST_DIR)/bench_%: export CFLAGS += $(CFLAGS_RELEASE)
$(TEST_DIR)/%: export LDFLAGS += $(LDFLAGS_TEST)
$(TEST_DIR)/%: $(TEST_DIR)/%.c $(TEST_DIR)/test.c $(OBJECTS) $(LIBQEV_TEST)
	@echo '-------- Compiling $@ --------'
	@cd $(TEST_DIR) && $(CC) $(CFLAGS) $*.c test.c $(OBJECTS_TEST) -o $* $(LDFLAGS)

$(LIBQEV) $(LIBQEV_TEST): $(QEV_DIR)/%:
	cd $(QEV_DIR) && $(MAKE) $*
