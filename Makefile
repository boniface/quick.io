.PHONY: test

#
# Hide annoying messages
#
MAKEFLAGS += -rR --no-print-directory

CC = clang

TEST_DIR = test
LIB_DIR = lib
SRC_DIR = src

BINARY = quickio

OBJECTS = \
	$(LIB_DIR)/http-parser/http_parser.o \
	$(SRC_DIR)/config.o \
	$(SRC_DIR)/events.o \
	$(SRC_DIR)/protocols.o \
	$(SRC_DIR)/protocols/flash.o \
	$(SRC_DIR)/protocols/rfc6455.o \
	$(SRC_DIR)/protocols/stomp.o \
	$(SRC_DIR)/quickio.o \
	$(SRC_DIR)/qev.o

OBJECTS_TEST = \
	$(patsubst %,../%,$(OBJECTS))

TESTS = \
	test_events

LIBS = glib-2.0 openssl
LIBS_TEST = check

CFLAGS = \
	-g \
	--coverage \
	-Wall \
	-Wextra \
	-Wsign-compare \
	-Wtype-limits \
	-Wuninitialized \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-D_FORTIFY_SOURCE=2 \
	-std=gnu99 \
	-I$(SRC_DIR) \
	$(shell pkg-config --cflags $(LIBS)) \
	-fno-inline \
	-DQEV_LOG_DEBUG

CFLAGS_TEST = \
	-I../$(SRC_DIR) \
	-DQIO_TESTING \
	$(shell pkg-config --cflags $(LIBS_TEST)) \

LDFLAGS = \
	-g \
	-rdynamic \
	-fno-default-inline \
	--coverage \
	-lm \
	$(shell pkg-config --libs $(LIBS))

LDFLAGS_TEST = \
	$(shell pkg-config --libs $(LIBS_TEST))

all:
	@echo "Choose one of the following:"
	@echo "    make run - run quickio in debug mode"
	@echo "    make clean - clean up everything"

run: $(BINARY)
	./$(BINARY)

test: $(TESTS)

clean:
	find -name '*.gcno' -exec rm {} \;
	find -name '*.gcda' -exec rm {} \;
	find $(TEST_DIR) -name '*.xml' -exec rm {} \;
	rm -f $(OBJECTS)
	rm -f $(patsubst %,$(TEST_DIR)/%,$(TESTS))
	rm -f $(BINARY)

$(BINARY): $(OBJECTS)
	@echo '-------- Compiling quickio --------'
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(TESTS):
test_%: CFLAGS += $(CFLAGS_TEST)
test_%: LDFLAGS += $(LDFLAGS_TEST)
test_%: $(TEST_DIR)/test_%.c $(TEST_DIR)/test.c $(OBJECTS)
	@echo '-------- Compiling $@ --------'
	cd $(TEST_DIR) && $(CC) $(CFLAGS) $@.c test.c $(OBJECTS_TEST) -o $@ $(LDFLAGS)
	@cd $(TEST_DIR) && ./$@
