#include "qio.h"

#include "qsys/epoll.c"

/**
 * The socket that we perform all operations on.
 */
static qsys_socket _socket;

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

gboolean qsys_timer_set(client_t *client, guint16 sec, guint16 ms) {
	// Don't create a new timer if there is one already running
	if (client->timer > 0) {
		return TRUE;
	}
	
	if (sec == 0 && ms == 0) {
		sec = option_timeout();
	}
	
	if (!_qsys_timer_set(client, sec, ms)) {
		DEBUG("Could not set timer");
		return FALSE;
	}
	
	return TRUE;
}

void qsys_timer_clear(client_t *client) {
	if (client->timer > 0) {
		_qsys_timer_clear(client);
	}
}