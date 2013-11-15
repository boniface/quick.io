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
	$(SRC_DIR)/protocols.o \
	$(SRC_DIR)/protocols/flash.o \
	$(SRC_DIR)/protocols/rfc6455.o \
	$(SRC_DIR)/protocols/stomp.o \
	$(SRC_DIR)/quickio.o \
	$(SRC_DIR)/qev.o

LIBS = glib-2.0 openssl

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
	-fno-inline

LDFLAGS = \
	-g \
	-rdynamic \
	-fno-default-inline \
	--coverage \
	-lm \
	$(shell pkg-config --libs $(LIBS))

all:
	@echo "Choose one of the following:"
	@echo "    make run - run quickio in debug mode"
	@echo "    make clean - clean up everything"

run: $(BINARY)
	./$(BINARY)

clean:
	find -name '*.gcno' -exec rm {} \;
	find -name '*.gcda' -exec rm {} \;
	rm -f $(OBJECTS)
	rm -f $(BINARY)

$(BINARY): $(OBJECTS)
	@echo '-------- Compiling quickio --------'
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
