include ../../Makefile.mk

BUILD_DIR_APPS := $(BUILD_DIR)/apps

APPS_CFLAGS += -fPIE
OBJECTS = $(patsubst %.c, $(BUILD_DIR_APPS)/%.so, $(wildcard *.c))

all: $(BUILD_DIR_APPS) $(OBJECTS)

$(BUILD_DIR_APPS)/%.so: %.c
	@echo '-------- Compiling $< --------'
	@# So that coverage reports are generated into the proper ../build_*/ directory
	@$(CC) -c $(CFLAGS) $(APPS_CFLAGS) $< -o $@-c
	@$(CC) -shared $(CFLAGS) $(APPS_CFLAGS) $@-c -o $@

$(BUILD_DIR_APPS):
	mkdir -p $(BUILD_DIR_APPS)
