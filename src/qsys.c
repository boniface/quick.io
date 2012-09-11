#include "qio.h"

#include "qsys/epoll.c"

void qsys_main_loop() {
	if (_qsys_init(option_bind_address(), option_port())) {
		DEBUGF("Sys inited, listening for connections on %d", option_port());
	} else {
		ERROR("_qsys_init() failed");
		return;
	}
	
	if (_qsys_init_maintenance()) {
		DEBUG("Maintenance timer inited");
	} else {
		ERROR("Coud not init maintenance timer");
		return;
	}
	
	while (TRUE) {
		_qsys_dispatch();
	}
}

gssize qsys_read(client_t *client, gchar *buff, gsize buff_size) {
	return _qsys_read(client->socket, buff, buff_size);
}

gssize qsys_write(client_t *client, gchar *buff, gsize buff_size) {
	return _qsys_write(client->socket, buff, buff_size);
}

void qsys_close(client_t *client) {
	_qsys_close(client->socket);
}

#ifdef TESTING
#include "../test/test_qsys.c"
#endif