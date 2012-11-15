/**
 * Copyright 2012 Andrew Stone <andrew@clovar.com>
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "qev.h"
#include "qev_internal.h"
#include "qev_dh.h"

/**
 * The work queue for closed clients.
 */
static qev_wqueue_t *_closed;

/**
 * Local DH params that can be reused
 */
#define QEV_DH_SIZES_X(size) static DH * QEV_DH_NAME(size) = NULL;
	QEV_DH_SIZES
#undef QEV_DH_SIZES_X

/**
 * SSL handshakes can take multiple read/write cycles
 */
static inline int _qev_ssl_handshake(SSL *ctx, volatile qev_flags_t *flags) {
	int err;
	
	// Had to break this into multiple lines...it was unreadable as one
	// Still not too readable, but better. Ugh.
	if ((err = SSL_accept(ctx)) == 1) {
		// Handshake is done!
		__sync_and_and_fetch(flags, ~QEV_CMASK_SSL_HANDSHAKING);
	} else {
		__sync_or_and_fetch(flags, QEV_CMASK_SSL_HANDSHAKING);
		err = SSL_get_error(ctx, err);
		if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
			fprintf(stderr, "SSL_accept failed: %s\n", ERR_reason_error_string(err));
			return -1;
		}
	}
	
	return 0;
}

static DH* _qev_ssl_get_tmpdh(SSL *s, int is_export, int key_len) {
	switch (key_len) {
		#define QEV_DH_SIZES_X(size) \
			case size: \
				return QEV_DH_NAME(size);
		#undef QEV_DH_SIZES_X
	}
	
	return QEV_DH_NAME(QEV_DH_DEFAULT_SIZE);
}

/**
 * Generates new Diffie-Hellman primes for this instance
 *
 * @return 1 on success, 0 on failure.
 */
static int _qev_setup_dh(SSL_CTX *ctx) {
	#define QEV_DH_SIZES_X(size) \
		if ((_qev_dh ## size = DH_new()) == NULL) { \
			return FALSE; \
		} \
		_qev_dh ## size->p = BN_bin2bn(_qev_dh ## size ## _p, sizeof(_qev_dh ## size ## _p), NULL); \
		_qev_dh ## size->g = BN_bin2bn(_qev_dh ## size ## _g, sizeof(_qev_dh ## size ## _g), NULL);
		
		QEV_DH_SIZES
	#undef QEV_DH_SIZES_X
		
    SSL_CTX_set_tmp_dh_callback(ctx, _qev_ssl_get_tmpdh);
	
	return TRUE;
}

int qev_init() {
	_closed = qev_wqueue_init(qev_client_free);
	return qev_sys_init();
}

int qev_listen(char *ip_address, uint16_t port) {
	return qev_sys_listen(ip_address, port, NULL);
}

int qev_listen_ssl(char *ip_address, uint16_t port, char *cert_path, char *key_path) {
	QEV_CLIENT_T *client;
	qev_socket_t sock = qev_sys_listen(ip_address, port, &client);
	
	if (sock == -1) {
		return -1;
	}
	
	static int inited = 0;
	if (!inited) {
		if (!RAND_poll()) {
			fprintf(stderr, "qev_listen_ssl(): No entropy for encryption\n");
			return -1;
		}
		
		SSL_load_error_strings();
		SSL_library_init();
		
		inited = 1;
	}
	
	SSL_CTX *ctx = SSL_CTX_new(SSLv3_server_method());
	
	if (ctx == NULL) {
		fprintf(stderr, "qev_listen_ssl(): Could not create SSL context\n");
		return -1;
	}
	
	if (!SSL_CTX_use_certificate_chain_file(ctx, cert_path)) {
		fprintf(stderr, "qev_listen_ssl(): Could not load certificate\n");
		return -1;
	}
	
	if (!SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM)) {
		fprintf(stderr, "qev_listen_ssl(): Could not load private key\n");
		return -1;
	}
	
	if (!SSL_CTX_check_private_key(ctx)) {
		fprintf(stderr, "qev_listen_ssl(): Could not validate private key\n");
		return -1;
	}
	
	if (!_qev_setup_dh(ctx)) {
		fprintf(stderr, "qev_listen_ssl(): Could not setup Diffie-Hellman params\n");
		return -1;
	}
	
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	
	QEV_CSLOT(client, ssl_ctx) = ctx;
	QEV_CSLOT(client, _flags) |= QEV_CMASK_SSL;
	
	return 0;
}

void qev_run() {
	int id = qev_wqueue_register(_closed);
	while (1) {
		qev_dispatch();
		qev_wqueue_tick(_closed, id);
	}
}

void* qev_run_thread(void *arg) {
	qev_run();
	return NULL;
}

QEV_CLIENT_T* qev_create_client() {
	return g_slice_alloc0(sizeof(QEV_CLIENT_T));
}

void qev_client_read(QEV_CLIENT_T *client) {
	// In order to synchronize actions across threads, we keep a counter
	// of the number of actions on a client: if it is greater than 1, it means
	// another thread is working on it, if it is 0, then it means we
	// may work on it. In order to accomplish this without locking, we're 
	// using GCC's atomic functions.
	if (__sync_fetch_and_add(&QEV_CSLOT(client, _operations), 1) == 0) {
		QEV_TEST_LOCK(qev_client_read_in);
		do {
			if (QEV_CSLOT(client, _flags) & QEV_CMASK_SSL_HANDSHAKING) {
				if (_qev_ssl_handshake(QEV_CSLOT(client, ssl_ctx), &QEV_CSLOT(client, _flags)) != 0) {
					qev_close(client);
					
					// Don't decrement the counter: nothing should be done on this client anymore
					break;
				}
			} else {
				if (!QEV_CLIENT_READ_FN(client)) {
					qev_close(client);
					break;
				}
			}
		} while (__sync_sub_and_fetch(&QEV_CSLOT(client, _operations), 1) > 0 && !(QEV_CSLOT(client, _flags) & QEV_CMASK_CLOSING));
	}
}

void qev_close(QEV_CLIENT_T *client) {
	QEV_TEST_LOCK(qev_close_before);
	// Don't put the client into the list if he is already being closed
	if (!(__sync_fetch_and_or(&QEV_CSLOT(client, _flags), QEV_CMASK_CLOSING) & QEV_CMASK_CLOSING)) {
		qev_sys_client_closed(client);
		qev_wqueue_add(_closed, client);
	}
}

void qev_client_free(void *c) {
	QEV_CLIENT_T *client = c;
	if (QEV_CSLOT(client, _flags) & QEV_CMASK_SSL) {
		SSL_free(QEV_CSLOT(client, ssl_ctx));
	}
	
	QEV_CLIENT_CLOSE_FN(client);
	
	#ifndef QEV_CLIENT_NEVER_FREE
		g_slice_free1(sizeof(*client), client);
	#endif
}

void qev_debug_flush() {
	qev_wqueue_debug_flush(_closed);
}

#ifdef __linux__
	#include "qev_linux.c"
#else
	#error No platform setup for quick-event
#endif

#include "qev_wqueue.c"