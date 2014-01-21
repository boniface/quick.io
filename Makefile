.PHONY: clean docs test

#
# Hide annoying messages
#
MAKEFLAGS += --no-print-directory

#
# If the test cases should be run in valgrind. Comment this out to disable
#
# USE_VALGRIND = 1

export CC = clang

LIB_DIR = lib
SRC_DIR = src
TEST_DIR = test
TEST_APPS_DIR = $(TEST_DIR)/apps
QEV_DIR = $(CURDIR)/lib/quick-event

BINARY = quickio

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

BIN_OBJECTS = \
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
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-std=gnu99 \
	-DQIO_SERVER \
	-I$(CURDIR)/$(LIB_DIR) \
	-I$(CURDIR)/$(SRC_DIR) \
	$(shell pkg-config --cflags $(LIBS))

CFLAGS_OBJ = \
	$(CFLAGS) \
	-c

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

CFLAGS_PROD = \
	-O3

LDFLAGS = \
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

LDFLAGS_PROD = \
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

all:
	@echo "Choose one of the following:"
	@echo "    make run - run quickio in debug mode"
	@echo "    make clean - clean up everything"

run: CFLAGS += $(CFLAGS_DEBUG)
run: LDFLAGS += $(LDFLAGS_DEBUG)
run: $(BINARY)
	$(MEMTEST) ./$(BINARY)

prod: CFLAGS += $(CFLAGS_PROD)
prod: LDFLAGS += $(LDFLAGS_PROD)
prod: $(BINARY)
prod:
	./$(BINARY)

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

clean:
	find -name '*.gcno' -exec rm {} \;
	find -name '*.gcda' -exec rm {} \;
	find -name '*.xml' -exec rm {} \;
	find test -name 'test_*.ini' -exec rm {} \;
	rm -f $(BIN_OBJECTS)
	rm -f $(TEST_APPS)
	$(MAKE) -C lib/quick-event/ clean
	rm -f $(patsubst %,$(TEST_DIR)/%,$(TESTS) $(BENCHMARKS))
	rm -f $(BINARY)
	$(MAKE) -C docs clean

$(BINARY): $(BIN_OBJECTS) $(LIBQEV)
	@echo '-------- Compiling quickio --------'
	@$(CC) $^ -o $@ $(LDFLAGS)

.PHONY: $(TESTS)
$(TESTS): % : $(TEST_APPS) $(TEST_DIR)/%
	@cd $(TEST_DIR) && $(MEMTEST) ./$@

.PHONY: $(BENCHMARKS)
$(BENCHMARKS): % : $(TEST_APPS) $(TEST_DIR)/%
	@cd $(TEST_DIR) && ./$@

%.o: %.c $(HEADERS)
	@echo '-------- Compiling $@ --------'
	@$(CC) $(CFLAGS_OBJ) $< -o $@

$(TEST_APPS_DIR)/%.so: $(TEST_APPS_DIR)/%.c $(HEADERS)
	@echo '-------- Compiling app $@ --------'
	@cd $(TEST_APPS_DIR) && $(CC) -shared -fPIC $(CFLAGS) $*.c -o $*.so

$(TEST_DIR)/test_%: CFLAGS += $(CFLAGS_TEST)
$(TEST_DIR)/bench_%: CFLAGS += $(CFLAGS_PROD)
$(TEST_DIR)/%: LDFLAGS += $(LDFLAGS_TEST)
$(TEST_DIR)/%: $(TEST_DIR)/%.c $(TEST_DIR)/test.c $(OBJECTS) $(LIBQEV_TEST)
	@echo '-------- Compiling $@ --------'
	@cd $(TEST_DIR) && $(CC) $(CFLAGS) $*.c test.c $(OBJECTS_TEST) -o $* $(LDFLAGS)

$(LIBQEV) $(LIBQEV_TEST): $(QEV_DIR)/%:
	cd $(QEV_DIR) && $(MAKE) $*
