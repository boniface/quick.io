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

LIB_DIR = lib
SRC_DIR = src
TEST_DIR = test
QEV_DIR = $(LIB_DIR)/quick-event

LIBS = glib-2.0 gmodule-2.0 openssl uuid
LIBS_TEST = check
LIBS_QEV = $(QEV_DIR)/libqev.a
LIBS_QEV_TEST = $(QEV_DIR)/libqev_test.a

BINARY = quickio

#
# Base flags used everywhere
#
# ==============================================================================
#

CFLAGS ?= \
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

LDFLAGS ?= \
	-rdynamic \
	-Wl,-z,now \
	-Wl,-z,relro \
	-Wl,--as-needed \
	-lm \
	$(shell pkg-config --libs $(LIBS))

#
# Extra flags for debug
#
# ==============================================================================
#

CFLAGS_DEBUG = \
	-fno-inline \
	-DQIO_DEBUG \
	-DQEV_LOG_DEBUG

LDFLAGS_DEBUG =

#
# Extra flags for testing
#
# ==============================================================================
#

CFLAGS_TEST = \
	--coverage \
	-fno-inline \
	-I../$(SRC_DIR) \
	-DQEV_ENABLE_MOCK \
	-DPORT=$(shell echo $$(((($$$$ % (32766 - 1024)) + 1024) * 2))) \
	$(shell pkg-config --cflags $(LIBS_TEST))

LDFLAGS_TEST = \
	--coverage \
	$(shell pkg-config --libs $(LIBS_TEST))

#
# Extra flags for release
#
# ==============================================================================
#

CFLAGS_RELEASE = \
	-fPIE \
	-O3

LDFLAGS_RELEASE = \
	-pie

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
	$(SRC_DIR)/str.o \
	$(SRC_DIR)/sub.o

APPS = \
	$(SRC_DIR)/app_client_test.so

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
	test_protocol_http \
	test_protocol_raw \
	test_protocol_rfc6455 \
	test_protocol_util \
	test_str \
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
	@echo "    make bench       run any builtin benchmarks"
	@echo "    make clean       clean up everything"
	@echo "    make deb         make QuickIO debs for the current release"
	@echo "    make deb-stable  make QuickIO debs for stable"
	@echo "    make docs        build all documentation"
	@echo "    make docs-watch  rebuild all documentation any time something changes"
	@echo "    make install     install the binaries locally"
	@echo "    make run         run quickio in debug mode"
	@echo "    make test        run the test suite"
	@echo "    make uninstall   remove all installed files"

bench: _bench

clean:
	find -name '*.gcno' -exec rm {} \;
	find -name '*.gcda' -exec rm {} \;
	find -name '*.xml' -exec rm {} \;
	rm -f .build_*
	rm -f $(BINARY)
	rm -f $(OBJECTS)
	rm -f $(APPS) $(TEST_APPS)
	$(MAKE) -C $(QEV_DIR) clean
	rm -f $(patsubst %,$(TEST_DIR)/%,$(TESTS) $(BENCHMARKS))
	rm -f test/*.sock

install: _release

run: _debug
	./$(BINARY) -f quickio.ini

test: _test
	./lib/quick-event/ext/gcovr \
		--root=. \
		--exclude=test \
		--exclude=lib \
		--exclude='.*\.h'

#
# Used internally for changing build type
#
# ==============================================================================
#

.build_%:
	$(MAKE) clean
	touch $@

_debug: export CFLAGS += $(CFLAGS_DEBUG)
_debug: export LDFLAGS += $(LDLAGS_DEBUG)
_debug: .build_debug
	MAX_CLIENTS=65536 \
	PUBLIC_ADDRESS=localhost \
	BIND_PORT=8080 \
	BIND_PORT_SSL=4433 \
	BIND_PATH=/tmp/quickio.sock \
	SUPPORT_FLASH=false \
		./quickio.ini.sh > quickio.ini
	$(MAKE) $(BINARY) $(APPS)

_test: .build_test
	$(MAKE) $(TESTS)

_release _bench: export CFLAGS += $(CFLAGS_RELEASE)
_release _bench: export LDFLAGS += $(LDLAGS_RELEASE)
_release _bench: .build_release
	MAX_CLIENTS=4194304 \
	BIND_PORT=80 \
	BIND_PORT_SSL=443 \
	LOG_FILE=/var/log/quickio.log \
	SUPPORT_FLASH=true \
	USER=quickio \
		./quickio.ini.sh > quickio.ini
	$(MAKE) $(BINARY) $(APPS)

#
# Rules to build all the files
#
# ==============================================================================
#

$(BINARY): $(BINARY_OBJECTS) $(LIBS_QEV)
	@echo '-------- Linking quickio --------'
	@$(CC) $^ -o $@ $(LIBS_QEV) $(LDFLAGS)

.PHONY: $(TESTS)
$(TESTS): % : $(TEST_APPS) $(TEST_DIR)/%
	@cd $(TEST_DIR) && ./$@

.PHONY: $(BENCHMARKS)
$(BENCHMARKS): % : $(TEST_APPS) $(TEST_DIR)/%
	@cd $(TEST_DIR) && ./$@

$(SRC_DIR)/protocols_http_iframe.c: $(SRC_DIR)/protocols_http_iframe.html
	@echo '-------- Generating $@ --------'
	@java -jar $(HTML_COMPRESSOR) --compress-js $< > $@.html
	@xxd -i $@.html > $@

$(TEST_DIR)/test_%: export CFLAGS += $(CFLAGS_TEST)
$(TEST_DIR)/bench_%: export CFLAGS += $(CFLAGS_RELEASE)
$(TEST_DIR)/%: export LDFLAGS += $(LDFLAGS_TEST)
$(TEST_DIR)/%: $(TEST_DIR)/%.c $(TEST_DIR)/test.c $(OBJECTS) $(LIBS_QEV_TEST)
	@echo '-------- Compiling $@ --------'
	@cd $(TEST_DIR) && $(CC) $(CFLAGS) $*.c test.c $(OBJECTS_TEST) -o $* ../$(LIBS_QEV_TEST) $(LDFLAGS)

$(SRC_DIR)/protocols_http.o: $(SRC_DIR)/protocols_http_iframe.c
%.o: %.c
	@echo '-------- Compiling $@ --------'
	@$(CC) -c $(CFLAGS) $< -o $@

%.so: %.c
	@echo '-------- Compiling app $@ --------'
	@cd $(shell dirname $@) && $(CC) -shared -fPIC $(CFLAGS) $(<F) -o $(@F)

$(LIBS_QEV) $(LIBS_QEV_TEST): $(QEV_DIR)/% :
	@cd $(QEV_DIR) && $(MAKE) $*
