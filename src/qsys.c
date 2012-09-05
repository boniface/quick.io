#include "qio.h"

#include "qsys/epoll.c"

/**
 * The socket that we perform all operations on.
 */
static qsys_socket _socket;

/**
 * Fake client: used for maintenance rounds.
 */


gpointer qsys_accept(gpointer not_used) {
	while (TRUE) {
		conns_client_new(_qsys_accept(_socket));
	}
	
	return NULL;
}

gboolean qsys_init() {
	if (_qsys_init()) {
		DEBUG("Sys inited");
	} else {
		ERROR("_qsys_init() failed");
		return FALSE;
	}
	
	if (_qsys_init_maintenance()) {
		DEBUG("Maintenance timer inited");
	} else {
		ERROR("Coud not init maintenance timer");
		return FALSE;
	}
	
	return TRUE;
}

gboolean qsys_listen() {
	_socket = _qsys_listen(option_bind_address(), option_port());
	return _socket != 0;
}

void qsys_dispatch() {
	_qsys_dispatch();
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