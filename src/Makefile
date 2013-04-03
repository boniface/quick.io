include ../Makefile.mk

TARGET = $(BUILD_DIR)/quickio
OBJECTS += \
	$(patsubst %.c, $(BUILD_DIR)/%.o, $(wildcard *.c)) \
	$(patsubst %.o, $(BUILD_DIR)/%.o, $(DEPENDENCIES))

all: $(TARGET)

$(TARGET): $(BUILD_DIR) $(BUILD_DIR)/$(QIOINI_NAME) $(OBJECTS)
	@echo '-------- Compiling server --------'
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c %.h ../test/test_%.c
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS) $(CFLAGS_OBJ) $< -o $@

$(BUILD_DIR)/%.o: %.c %.h
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS) $(CFLAGS_OBJ) $< -o $@

$(BUILD_DIR)/%.o: ../test/%.c ../test/%.h
	@echo '-------- Compiling $< --------'
	@$(CC) $(CFLAGS) $(CFLAGS_OBJ) $< -o $@

$(BUILD_DIR)/valgrind.o: ../test/valgrind.c
	@echo '-------- Compiling valgrind.o --------'
	@$(CC) $(CFLAGS) $(CFLAGS_OBJ) $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(QIOINI_NAME):
	cp $(ROOT)/$(QIOINI) $@

include ../lib/utils/Makefile