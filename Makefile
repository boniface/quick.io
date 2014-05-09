#
# Hide annoying messages
#
MAKEFLAGS += --no-print-directory

VERSION_NAME = banished bongo
VERSION_MAJOR = 0
VERSION_MINOR = 2
VERSION_MICRO = 0

ifeq ($(CC),cc)
	export CC = clang
endif

BENCH_DIR = bench
LIB_DIR = lib
DOCS_DIR = docs
SRC_DIR = src
TEST_DIR = test
QEV_DIR = $(LIB_DIR)/quick-event

INSTALL_BIN = install
INSTALL = $(INSTALL_BIN) -m 644

LIBS = glib-2.0 gmodule-2.0 openssl
LIBS_TEST = check
LIBS_QEV = $(QEV_DIR)/libqev.a
LIBS_QEV_TEST = $(QEV_DIR)/libqev_test.a

BINARY = quickio
BINARY_HELPERS = quickio-clienttest quickio-fuzzer quickio-testapps

HTML_SRCS = \
	$(SRC_DIR)/protocols_http_html_iframe.c \
	$(SRC_DIR)/protocols_http_html_error.c

HTML_COMPRESSOR = $(LIB_DIR)/htmlcompressor-1.5.3.jar
COMPRESSOR_JARS = \
	$(HTML_COMPRESSOR) \
	$(LIB_DIR)/yuicompressor-2.4.8.jar

#
# Base flags used everywhere
#
# ==============================================================================
#

# When recursing on this Makefile, make sure the flags aren't overridden.
# Using "?=" allows dh_* to override all the flags, so protect them with this
# export.
ifndef QUICKIO_MAKEFILE
	export CFLAGS = \
		-g \
		-Wall \
		-Wextra \
		-Wshadow \
		-Wformat=2 \
		-Werror \
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
		-mfpmath=sse \
		-msse \
		-msse2 \
		$(shell pkg-config --cflags $(LIBS))

	export CFLAGS_BIN = $(CFLAGS)

	export CFLAGS_SO = \
		$(CFLAGS) \
		-shared \
		-fPIC

	export LDFLAGS = \
		-rdynamic \
		-Wl,-z,now \
		-Wl,-z,relro \
		-lm \
		$(shell pkg-config --libs $(LIBS))

	export LDFLAGS_BIN = \
		$(LDFLAGS)

	export LDFLAGS_SO = \
		$(LDFLAGS)

	export QUICKIO_MAKEFILE = 1
endif

#
# Extra flags for debug
#
# ==============================================================================
#

CFLAGS_BIN_DEBUG = \
	-fno-inline \
	-DQIO_DEBUG \
	-DQEV_LOG_DEBUG

LDFLAGS_BIN_DEBUG =

#
# Extra flags for testing
#
# ==============================================================================
#

CFLAGS_BIN_TEST = \
	--coverage \
	-fno-inline \
	-I../$(SRC_DIR) \
	-DQEV_ENABLE_MOCK \
	-DPORT=$(shell echo $$(((($$$$ % (32766 - 1024)) + 1024) * 2))) \
	$(shell pkg-config --cflags $(LIBS_TEST))

LDFLAGS_BIN_TEST = \
	--coverage \
	$(shell pkg-config --libs $(LIBS_TEST))

#
# Extra flags for release
#
# ==============================================================================
#

CFLAGS_BIN_RELEASE = \
	-O3

CFLAGS_SO_RELEASE = \
	-O3

LDFLAGS_BIN_RELEASE =

# In order to ensure that as-needed is used, it has to come BEFORE all the
# libraries, so the define for this one is a bit different (see _release)
LDFLAGS_SO_RELEASE = \
	-Wl,--as-needed

ifdef TCMALLOC_PROFILE
	export HEAPPROFILE=tcmalloc
	export HEAP_PROFILE_INUSE_INTERVAL=10485760
	export TCMALLOC_RELEASE_RATE=10

	LDFLAGS_BIN_RELEASE += \
		-ltcmalloc
else
	CFLAGS_BIN_RELEASE += \
		-fPIE

	LDFLAGS_BIN_RELEASE += \
		-pie \
		-ltcmalloc_minimal
endif

ifeq ($(CC),gcc)
	CFLAGS_BIN_RELEASE += -flto -fPIC
	LDFLAGS_BIN_RELEASE += -flto
endif

#
# What actually gets built
#
# ==============================================================================
#

OBJECTS = \
	$(SRC_DIR)/apps.o \
	$(SRC_DIR)/client.o \
	$(SRC_DIR)/config.o \
	$(SRC_DIR)/evs.o \
	$(SRC_DIR)/evs_qio.o \
	$(SRC_DIR)/evs_query.o \
	$(SRC_DIR)/periodic.o \
	$(SRC_DIR)/protocols.o \
	$(SRC_DIR)/protocols_flash.o \
	$(SRC_DIR)/protocols_http.o \
	$(SRC_DIR)/protocols_raw.o \
	$(SRC_DIR)/protocols_rfc6455.o \
	$(SRC_DIR)/protocols_util.o \
	$(SRC_DIR)/qev.o \
	$(SRC_DIR)/quickio.o \
	$(SRC_DIR)/sub.o

APPS = \
	$(SRC_DIR)/quickio-clienttest.so

OBJECTS_TEST = \
	$(patsubst %,../%,$(OBJECTS))

BINARY_OBJECTS = \
	$(OBJECTS) \
	$(SRC_DIR)/main.o

BENCHES = \
	bench_routing \
	bench_ws_decode

TESTS = \
	test_apps \
	test_client \
	test_config \
	test_coverage \
	test_evs \
	test_protocol_flash \
	test_protocol_http \
	test_protocol_raw \
	test_protocol_rfc6455 \
	test_protocol_util \
	test_sub

TEST_APPS = \
	$(TEST_DIR)/app_test_fatal_init.so \
	$(TEST_DIR)/app_test_fatal_exit.so \
	$(TEST_DIR)/app_test_invalid.so \
	$(TEST_DIR)/app_test_sane.so

#
# Human-friendly rules
#
# ==============================================================================
#

all:
	@echo "Choose one of the following:"
	@echo "    make bench              run all micro benchmarks"
	@echo "    make clean              clean up everything"
	@echo "    make deb                make QuickIO debs for the current release"
	@echo "    make deb-stable         make QuickIO debs for stable"
	@echo "    make debug              build QuickIO with all debugging information in place"
	@echo "    make docs               build all documentation"
	@echo "    make docs-watch         rebuild all documentation any time something changes"
	@echo "    make helper-clienttest  run QuickIO for client testing"
	@echo "    make helper-fuzzer      run QuickIO for fuzzing"
	@echo "    make install            install the binaries locally"
	@echo "    make release            build a release-ready version of QuickIO"
	@echo "    make run                run quickio in debug mode"
	@echo "    make test               run the test suite"
	@echo "    make uninstall          remove all installed files"

bench: _release
	@$(MAKE) $(BENCHES)

clean:
	@find -name '*.gcno' -exec rm {} \;
	@find -name '*.gcda' -exec rm {} \;
	@find -name '*.xml' -exec rm {} \;
	@find test/ -name 'test_*.ini' -exec rm {} \;
	@rm -f .build_*
	@rm -f quickio.ini
	@rm -f $(BINARY)
	@rm -f $(BINARY_OBJECTS)
	@rm -f $(APPS) $(TEST_APPS)
	@rm -f $(patsubst %,%.html,$(HTML_SRCS))
	@rm -f $(SRC_DIR)/protocols_http_iframe.c*
	@rm -f $(patsubst %,$(BENCH_DIR)/%,$(BENCHES))
	@rm -f $(patsubst %,$(TEST_DIR)/%,$(TESTS))
	@rm -f test/*.sock
	@rm -f tcmalloc.*
	@$(MAKE) -s -C $(QEV_DIR) clean
	@$(MAKE) -s -C $(DOCS_DIR) clean

deb:
	debuild -tc -b

deb-stable:
	sbuild -d wheezy

debug: _debug

.PHONY: docs
docs:
	$(MAKE) -C $(DOCS_DIR)
	doxygen

docs-watch:
	while [ true ]; do inotifywait -r $(DOCS_DIR); $(MAKE) docs; sleep 2; done

helper-clienttest: _debug
	sudo $(SRC_DIR)/quickio-clienttest

helper-fuzzer: _release
	$(SRC_DIR)/quickio-fuzzer

install: release
	mkdir -p \
		$(DESTDIR)/etc/quickio \
		$(DESTDIR)/etc/sysctl.d \
		$(DESTDIR)/usr/bin \
		$(DESTDIR)/usr/include/quickio \
		$(DESTDIR)/usr/include/quickio/quick-event \
		$(DESTDIR)/usr/lib/pkgconfig \
		$(DESTDIR)/usr/lib/quickio
	$(INSTALL_BIN) $(BINARY) $(patsubst %,$(SRC_DIR)/%,$(BINARY_HELPERS)) \
		$(DESTDIR)/usr/bin/
	$(INSTALL) quickio.ini $(DESTDIR)/etc/quickio
	$(INSTALL) src/*.h $(DESTDIR)/usr/include/quickio
	$(INSTALL) lib/quick-event/src/*.h $(DESTDIR)/usr/include/quickio/quick-event
	$(INSTALL) quickio.pc $(DESTDIR)/usr/lib/pkgconfig
	$(INSTALL) $(APPS) $(DESTDIR)/usr/lib/quickio
	$(INSTALL) sysctl.conf $(DESTDIR)/etc/sysctl.d/quickio.conf

release: _release docs

run: _debug
	./$(BINARY) -f quickio.ini

test: _test
	./lib/quick-event/ext/gcovr \
		--root=. \
		--exclude=test \
		--exclude=lib \
		--exclude='.*\.h'

uninstall:
	rm -rf $(DESTDIR)/usr/bin/quickio
	$(foreach h,$(BINARY_HELPERS),rm -rf $(DESTDIR)/usr/bin/$h;)
	rm -rf $(DESTDIR)/etc/quickio
	rm -rf $(DESTDIR)/usr/include/quickio
	rm -rf $(DESTDIR)/usr/lib/pkgconfig/quickio.pc
	rm -rf $(DESTDIR)/etc/sysctl.d/quickio.conf

#
# Used internally for changing build type
#
# ==============================================================================
#

.build_%:
	@$(MAKE) clean
	@touch $@

_debug: export CFLAGS_BIN += $(CFLAGS_BIN_DEBUG)
_debug: export LDFLAGS_BIN += $(LDFLAGS_BIN_DEBUG)
_debug: .build_debug
	@ \
		BIND_PATH=/tmp/quickio.sock \
		BIND_PORT=8080 \
		BIND_PORT_SSL=4433 \
		MAX_CLIENTS=65536 \
		PUBLIC_ADDRESS=localhost \
		SUPPORT_FLASH=false \
			./quickio.ini.sh > quickio.ini
	@$(MAKE) $(BINARY) $(APPS)

_test: .build_test
	@$(MAKE) $(TESTS)

_release: export CFLAGS_SO += $(CFLAGS_SO_RELEASE)
_release: export CFLAGS_BIN += $(CFLAGS_BIN_RELEASE)
_release: export LDFLAGS_SO := $(LDFLAGS_SO_RELEASE) $(LDFLAGS_SO)
_release: export LDFLAGS_BIN += $(LDFLAGS_BIN_RELEASE)
_release: .build_release
	@ \
		BIND_PORT=80 \
		BIND_PORT_SSL=443 \
		INCLUDE=/etc/quickio/apps/*.ini \
		LOG_FILE=/var/log/quickio.log \
		MAX_CLIENTS=4194304 \
		SUPPORT_FLASH=true \
		USER=quickio \
			./quickio.ini.sh > quickio.ini
	@$(MAKE) $(BINARY) $(APPS)

#
# Rules to build all the files
#
# ==============================================================================
#

$(BINARY): $(BINARY_OBJECTS) $(LIBS_QEV)
	@echo '-------- Linking quickio --------'
	@$(CC) $^ -o $@ $(LDFLAGS_BIN)

.PHONY: $(BENCHES)
$(BENCHES): % : $(BENCH_DIR)/%
	@cd $(BENCH_DIR) && ./$@

.PHONY: $(TESTS)
$(TESTS): % : $(TEST_APPS) $(TEST_DIR)/%
	@cd $(TEST_DIR) && G_SLICE=debug-blocks ./$@

$(SRC_DIR)/protocols_http_html_%.c: $(SRC_DIR)/protocols_http_%.html $(COMPRESSOR_JARS)
	@echo '-------- Generating $@ --------'
	@java -jar $(HTML_COMPRESSOR) --compress-js $< > $@.html
	@xxd -i $@.html > $@

$(BENCH_DIR)/%: CFLAGS_BIN += $(CFLAGS_BIN_RELEASE)
$(BENCH_DIR)/%: LDFLAGS_BIN += $(LDFLAGS_BIN_RELEASE)
$(BENCH_DIR)/%: $(BENCH_DIR)/%.c $(OBJECTS) _release
	@echo '-------- Compiling $@ --------'
	@$(CC) $(CFLAGS_BIN) $< $(OBJECTS) $(LIBS_QEV) -o $@ $(LDFLAGS_BIN)

$(TEST_DIR)/%: CFLAGS_BIN += $(CFLAGS_BIN_TEST)
$(TEST_DIR)/%: LDFLAGS_BIN += $(LDFLAGS_BIN_TEST)
$(TEST_DIR)/%: $(TEST_DIR)/%.c $(TEST_DIR)/test.c $(OBJECTS) $(LIBS_QEV_TEST)
	@echo '-------- Compiling $@ --------'
	@cd $(TEST_DIR) && $(CC) $(CFLAGS_BIN) $*.c test.c $(OBJECTS_TEST) -o $* ../$(LIBS_QEV_TEST) $(LDFLAGS_BIN)

$(SRC_DIR)/protocols_http.o: $(HTML_SRCS)
%.o: %.c
	@echo '-------- Compiling $@ --------'
	@$(CC) -c $(CFLAGS_BIN) $< -o $@

%.so: %.c
	@echo '-------- Compiling app $@ --------'
	@$(CC) $(CFLAGS_SO) $< -o $@ $(LDFLAGS_SO)

$(LIBS_QEV) $(LIBS_QEV_TEST): export CFLAGS := $(CFLAGS_BIN)
$(LIBS_QEV) $(LIBS_QEV_TEST): $(QEV_DIR)/% :
	@cd $(QEV_DIR) && $(MAKE) $*

#
# Downloads external dependencies for builds
#
# ==============================================================================
#

.PRECIOUS: $(COMPRESSOR_JARS)

$(LIB_DIR)/htmlcompressor-%.jar:
	wget --quiet -O $@ https://htmlcompressor.googlecode.com/files/htmlcompressor-$*.jar

$(LIB_DIR)/yuicompressor-%.jar:
	wget --quiet -O $@ https://github.com/yui/yuicompressor/releases/download/v$*/yuicompressor-$*.jar
