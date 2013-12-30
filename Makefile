.PHONY: clean test

#
# Hide annoying messages
#
MAKEFLAGS += --no-print-directory

#
# If the test cases should be run in valgrind. Comment this out to disable
#
# USE_VALGRIND = 1

CC = gcc

LIB_DIR = lib
SRC_DIR = src
TEST_DIR = test
TEST_APPS_DIR = $(TEST_DIR)/apps

BINARY = quickio

ifdef USE_VALGRIND
	MEMTEST = \
		G_SLICE=always-malloc \
		G_DEBUG=gc-friendly \
		valgrind \
			--quiet \
			--suppressions=../lib/quick-event/test/valgrind.supp \
			--tool=memcheck \
			--leak-check=full \
			--leak-resolution=high \
			--num-callers=20 \
			--track-origins=yes
else
	MEMTEST =
endif

HEADERS = \
	$(shell find $(SRC_DIR) -name '*.h') \
	include/quickio_app.h

OBJECTS = \
	$(LIB_DIR)/http-parser/http_parser.o \
	$(SRC_DIR)/apps.o \
	$(SRC_DIR)/apps_export.o \
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

BIN_OBJECTS = \
	$(OBJECTS) \
	$(SRC_DIR)/main.o

OBJECTS_TEST = \
	$(patsubst %,../%,$(OBJECTS))

TESTS = \
	test_apps \
	test_client \
	test_evs \
	test_protocol_flash \
	test_protocol_raw \
	test_protocol_rfc6455

TEST_APPS = \
	$(TEST_APPS_DIR)/test_app_sane.so

BENCHMARKS = \
	bench_evs_query

LIBS = glib-2.0 gmodule-2.0 openssl
LIBS_TEST = check
LIBQEV = lib/quick-event/libqev.a

CFLAGS = \
	-Wall \
	-Wextra \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-std=gnu99 \
	-I$(SRC_DIR) \
	$(shell pkg-config --cflags $(LIBS))

CFLAGS_OBJ = \
	$(CFLAGS) \
	-c \
	-fvisibility=hidden

CFLAGS_TEST = \
	-g \
	--coverage \
	-fno-inline \
	-I../$(SRC_DIR) \
	-DPORT=$(shell echo $$(((($$$$ % (32766 - 1024)) + 1024) * 2))) \
	$(shell pkg-config --cflags $(LIBS_TEST))

CFLAGS_DEBUG = \
	-g \
	-fno-inline \
	-DQEV_LOG_DEBUG

CFLAGS_BENCH = \
	-O3

LDFLAGS = \
	$(CURDIR)/$(LIBQEV) \
	-lm \
	$(shell pkg-config --libs $(LIBS))

LDFLAGS_DEBUG = \
	-g \
	-rdynamic \
	-fno-default-inline

LDFLAGS_TEST = \
	-g \
	-rdynamic \
	--coverage \
	-fno-default-inline \
	$(shell pkg-config --libs $(LIBS_TEST))

all:
	@echo "Choose one of the following:"
	@echo "    make run - run quickio in debug mode"
	@echo "    make clean - clean up everything"

run: CFLAGS += $(CFLAGS_DEBUG)
run: LDFLAGS += $(LDFLAGS_DEBUG)
run: $(BINARY)
	$(MEMTEST) ./$(BINARY)

test: $(TESTS)
	./lib/quick-event/ext/gcovr \
		--root=. \
		--exclude=test \
		--exclude=lib \
		--exclude='.*\.h'

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
$(TEST_DIR)/bench_%: CFLAGS += $(CFLAGS_BENCH)
$(TEST_DIR)/%: LDFLAGS += $(LDFLAGS_TEST)
$(TEST_DIR)/%: $(TEST_DIR)/%.c $(TEST_DIR)/test.c $(OBJECTS) $(LIBQEV)
	@echo '-------- Compiling $@ --------'
	@cd $(TEST_DIR) && $(CC) $(CFLAGS) $*.c test.c $(OBJECTS_TEST) -o $* $(LDFLAGS)

$(LIBQEV):
	cd lib/quick-event && $(MAKE)
