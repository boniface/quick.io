UTILS_CFLAGS = -fPIC -c -I$(ROOT)/src -fvisibility=hidden
UTILS_DIR = $(ROOT)/lib/utils

$(BUILD_DIR)/http_parser.o: $(UTILS_DIR)/http_parser.c $(UTILS_DIR)/http_parser.h
	@echo '-------- Compiling http_parser.o --------'
	@$(CC) -c $(CFLAGS) $(UTILS_CFLAGS) $< -o $@

$(BUILD_DIR)/reconnecting_socket.o: $(UTILS_DIR)/reconnecting_socket.c $(UTILS_DIR)/reconnecting_socket.h
	@echo '-------- Compiling reconnecting_socket.o --------'
	@$(CC) -c $(CFLAGS) $(UTILS_CFLAGS) $< -o $@
