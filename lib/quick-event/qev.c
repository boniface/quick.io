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
 * OpenSSL is thread safe only if we give it locks
 */
static GMutex *_ssl_locks;

/**
 * Local DH params that can be reused
 */
#define QEV_DH_SIZES_X(size) static DH * QEV_DH_NAME(size) = NULL;
	QEV_DH_SIZES
#undef QEV_DH_SIZES_X

/**
 * SSL handshakes can take multiple read/write cycles
 */
static inline int _qev_ssl_handshake(SSL *ctx, qev_flags_t *flags) {
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
			g_log(QEV_DOMAIN, G_LOG_LEVEL_WARNING, "SSL_accept failed: %s", ERR_error_string(err, NULL));
			return -1;
		}
	}
	
	return 0;
}

/**
 * OpenSSL callback for getting the DH params for the given size.
 */
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
 * Setup elliptic curve Diffie–Hellman
 *
 * @return If everything was setup correctly
 */
static gboolean _qev_setup_ecdh(SSL_CTX *ctx) {
	EC_KEY *ecdh;
	
	if ((ecdh = EC_KEY_new_by_curve_name(QEV_ECDH_CURVE)) == NULL) {
		return FALSE;
	}
	
	SSL_CTX_set_tmp_ecdh(ctx, ecdh);
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);
	EC_KEY_free(ecdh);
	
	return TRUE;
}

/**
 * Set the public Diffie-Hellman params
 *
 * @return If everything was setup correctly
 */
static gboolean _qev_setup_dh(SSL_CTX *ctx) {
	#define QEV_DH_SIZES_X(size) \
		if ((_qev_dh ## size = DH_new()) == NULL) { \
			return FALSE; \
		} \
		_qev_dh ## size->p = BN_bin2bn(_qev_dh ## size ## _p, sizeof(_qev_dh ## size ## _p), NULL); \
		_qev_dh ## size->g = BN_bin2bn(_qev_dh ## size ## _g, sizeof(_qev_dh ## size ## _g), NULL);
		
		QEV_DH_SIZES
	#undef QEV_DH_SIZES_X
	
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
	SSL_CTX_set_tmp_dh_callback(ctx, _qev_ssl_get_tmpdh);
	
	return TRUE;
}

static void _ssl_lock_fn(int mode, int lock_num, const char *file, int line) {
	if (mode & CRYPTO_LOCK) {
		g_mutex_lock((_ssl_locks + lock_num));
	} else {
		g_mutex_unlock((_ssl_locks + lock_num));
	}
}

static void _qev_client_free(void *c) {
	QEV_CLIENT_T *client = c;
	if (QEV_CSLOT(client, _flags) & QEV_CMASK_SSL) {
		int mode = SSL_get_shutdown(QEV_CSLOT(client, ssl_ctx));
		mode |= SSL_RECEIVED_SHUTDOWN | SSL_SENT_SHUTDOWN;
		
		SSL_set_shutdown(QEV_CSLOT(client, ssl_ctx), mode);
		SSL_set_quiet_shutdown(QEV_CSLOT(client, ssl_ctx), 1);
		SSL_shutdown(QEV_CSLOT(client, ssl_ctx));
		
		SSL_free(QEV_CSLOT(client, ssl_ctx));
	}
	
	QEV_CLIENT_CLOSE_FN(client);
	
	#ifndef QEV_CLIENT_NEVER_FREE
		g_slice_free1(sizeof(*client), client);
	#endif
}

int qev_init() {
	qev_time = g_get_real_time() / 1000000;
	_closed = qev_wqueue_init(_qev_client_free);
	return qev_sys_init();
}

int qev_listen(const char *ip_address, const uint16_t port) {
	return qev_sys_listen(ip_address, port, NULL);
}

int qev_listen_ssl(const char *ip_address, const uint16_t port, const char *cert_path, const char *key_path) {
	QEV_CLIENT_T *client;
	qev_socket_t sock = qev_sys_listen(ip_address, port, &client);
	
	if (sock == -1) {
		return -1;
	}
	
	static int inited = 0;
	if (!inited) {
		SSL_load_error_strings();
		SSL_library_init();
		
		_ssl_locks = g_malloc(CRYPTO_num_locks() * sizeof(*_ssl_locks));
		for (int i = 0; i < CRYPTO_num_locks(); i++) {
			g_mutex_init((_ssl_locks + i));
		}
		
		CRYPTO_set_locking_callback(_ssl_lock_fn);
		
		inited = 1;
	}
	
	// Provides more options using SSLv23 (SSLv2 disabled below)
	// If just left as SSLv3, Flash can't connect.  Hooray.
	SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
	
	if (ctx == NULL) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen_ssl(): Could not create SSL context");
		return -1;
	}
	
	if (!SSL_CTX_use_certificate_chain_file(ctx, cert_path)) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen_ssl(): Could not load certificate");
		return -1;
	}
	
	if (!SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM)) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen_ssl(): Could not load private key");
		return -1;
	}
	
	if (!SSL_CTX_check_private_key(ctx)) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen_ssl(): Could not validate private key");
		return -1;
	}
	
	if (!SSL_CTX_set_cipher_list(ctx, QEV_SSL_CIPHERS)) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen_ssl(): Could not set SSL ciphers");
		return -1;
	}
	
	if (!_qev_setup_dh(ctx)) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen_ssl(): Could not setup Diffie-Hellman params");
		return -1;
	}
	
	if (!_qev_setup_ecdh(ctx)) {
		g_log(QEV_DOMAIN, G_LOG_LEVEL_CRITICAL, "qev_listen_ssl(): Could not setup ECDH params");
		return -1;
	}
	
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
	SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	
	QEV_CSLOT(client, ssl_ctx) = ctx;
	QEV_CSLOT(client, _flags) |= QEV_CMASK_SSL;
	
	return 0;
}

void qev_run() {
	static int ticks = 0;
	
	int id = qev_wqueue_register(_closed);
	while (1) {
		qev_dispatch();
		qev_wqueue_tick(_closed, id);
		
		if (g_atomic_int_add(&ticks, 1) == 5) {
			ticks = 0;
			qev_time = g_get_real_time() / 1000000;
		}
		
		#ifdef QEV_TICK_FN
			QEV_TICK_FN();
		#endif
	}
}

void* qev_run_thread(void *arg) {
	qev_run();
	return NULL;
}

QEV_CLIENT_T* qev_client_create() {
	return g_slice_alloc0(sizeof(QEV_CLIENT_T));
}

void qev_client_read(QEV_CLIENT_T *client) {
	// In order to synchronize actions across threads, we keep a counter
	// of the number of actions on a client: if it is greater than 1, it means
	// another thread is working on it, if it is 0, then it means we
	// may work on it. In order to accomplish this without locking, we're 
	// using GCC's atomic functions.
	if (__sync_fetch_and_add(&QEV_CSLOT(client, _read_operations), 1) == 0) {
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
		} while (__sync_sub_and_fetch(&QEV_CSLOT(client, _read_operations), 1) > 0 && !(__sync_fetch_and_or(&QEV_CSLOT(client, _flags), 0) & QEV_CMASK_CLOSING));
	}
}

void qev_timer_fire(_timer_t *timer) {
	if (timer->flags & QEV_TIMER_EXCLUSIVE) {
		if (__sync_fetch_and_add(&timer->operations, 1) == 0) {
			do {
				timer->fn();
			} while (__sync_sub_and_fetch(&timer->operations, 1) > 0);
		}
	} else {
		timer->fn();
	}
}

void qev_close(QEV_CLIENT_T *client) {
	QEV_TEST_LOCK(qev_close_before);
	// Don't put the client into the list if he is already being closed
	if (!(__sync_fetch_and_or(&QEV_CSLOT(client, _flags), QEV_CMASK_CLOSING) & QEV_CMASK_CLOSING)) {
		#ifdef QEV_CLIENT_KILLED_FN
			QEV_CLIENT_KILLED_FN(client);
		#endif
		
		qev_sys_client_closed(client);
		qev_wqueue_add(_closed, client);
	}
}

void qev_client_lock(QEV_CLIENT_T *client) {
	while (!__sync_bool_compare_and_swap(&QEV_CSLOT(client, _lock), 0, 1)) {
		// Use yield: you're allowed to do sys calls on clients while locked,
		// so another syscall won't hurt too bad
		g_thread_yield();
		QEV_STATS_INC(qev_lock_client_spin);
	}
}

void qev_client_unlock(QEV_CLIENT_T *client) {
	__sync_bool_compare_and_swap(&QEV_CSLOT(client, _lock), 1, 0);
}

void qev_debug_flush() {
	qev_wqueue_debug_flush(_closed);
}

#ifdef __linux__
	#include "qev_linux.c"
#else
	#error No platform setup for quick-event
#endif

#include "qev_lock.c"
#include "qev_wqueue.c"