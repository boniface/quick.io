/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include <poll.h>
#include "test.h"

#define INIT_HEADERS \
	"POST /?sid=%s&connect=true HTTP/1.1\r\n" \
	"Content-Length: 21\r\n\r\n" \
	"/qio/hostname:1=null\n"

#define MSG_HEADERS \
	"POST /?sid=%s HTTP/1.1\r\n" \
	"Content-Length: %lu\r\n\r\n" \
	"%s"

const gchar *uuid_chars = "abcdef123456790";

struct httpc {
	/**
	 * The socket currently polling on the server
	 */
	qev_fd_t polling;

	/**
	 * The idle socket, both on client and server
	 */
	qev_fd_t waiting;

	/**
	 * Buffered messages from the server
	 */
	GAsyncQueue *aq;

	/**
	 * Session Id
	 */
	gchar *sid;

	/**
	 * The thread the client is running in
	 */
	GThread *th;

	/**
	 * If the thread should keep running
	 */
	gboolean th_run;

	/**
	 * Mutex for sending
	 */
	GMutex lock;
};

static void* _httpc_thread(void *hc);
static void _next(struct httpc *hc, const gchar *msg);

static void _uuid(gchar sid[33])
{
	guint i;
	for (i = 0; i < 32; i++) {
		sid[i] = uuid_chars[g_random_int_range(0, strlen(uuid_chars))];
	}

	sid[32] = '\0';
}

static struct httpc* _httpc_new()
{
	gint err;
	gchar sid[33];
	GString *buff = qev_buffer_get();
	struct httpc *hc = g_slice_alloc0(sizeof(*hc));

	_uuid(sid);

	hc->polling = test_socket();
	hc->waiting = test_socket();
	hc->aq = g_async_queue_new();
	hc->sid = g_strdup(sid);
	hc->th_run = TRUE;
	g_mutex_init(&hc->lock);

	hc->th = g_thread_new("httpc", _httpc_thread, hc);

	g_string_printf(buff, INIT_HEADERS, hc->sid);
	err = send(hc->polling, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	qev_buffer_put(buff);

	_next(hc, "/qio/callback/1:0={\"code\":200,\"data\":\"localhost\"}");

	return hc;
}

static void _httpc_free(struct httpc *hc)
{
	hc->th_run = FALSE;
	g_thread_join(hc->th);
	hc->th = NULL;

	close(hc->polling);
	hc->polling = -1;

	close(hc->waiting);
	hc->waiting = -1;

	if (g_async_queue_length(hc->aq) != 0) {
		guint i;
		guint len = g_async_queue_length(hc->aq);
		GString *buff = qev_buffer_get();

		for (i = 0; i < len; i++) {
			gchar *msg = g_async_queue_pop(hc->aq);
			g_string_append_printf(buff, "\t* %s\n", msg);
		}

		CRITICAL("Async Queue was not emptied; %u messages remain:\n%s",
			len, buff->str);

		qev_buffer_put(buff);

		ck_abort();
	}

	g_async_queue_unref(hc->aq);
	hc->aq = NULL;

	g_free(hc->sid);
	hc->sid = NULL;

	g_mutex_clear(&hc->lock);

	g_slice_free1(sizeof(*hc), hc);
}

static void _drain(struct httpc *hc, qev_fd_t sock)
{
	GString *buff = qev_buffer_get();

	qev_buffer_ensure(buff, buff->len + 2048);

	while (TRUE) {
		gssize to_read = buff->allocated_len - buff->len;
		gssize err = recv(sock, buff->str + buff->len, to_read, MSG_DONTWAIT);

		if (err > 0) {
			g_string_set_size(buff, buff->len + err);
		}

		if (err != to_read) {
			break;
		}
	}

	while (buff->len > 0) {
		gchar *cl;
		guint64 head_len;
		guint64 body_len;
		struct qev_http_response headers;

		ck_assert(qev_http_response_parse(buff->str, &headers));
		cl = qev_http_response_header(&headers, "Content-Length");
		body_len = g_ascii_strtoull(cl, NULL, 10);
		head_len = headers.body - buff->str;

		ASSERT(buff->len >= (head_len + body_len),
			"Incomplete read from server: exepected at least %lu, got %lu",
			head_len + body_len, buff->len);

		ASSERT(headers.status == 200,
			"Didn't get 200, got: %lu", headers.status);

		if (body_len > 0) {
			gchar replacement = headers.body[body_len];
			headers.body[body_len] = '\0';

			gchar **msgs = g_strsplit(headers.body, "\n", -1);
			gchar **tmp = msgs;

			while (*tmp != NULL) {
				if (strlen(*tmp) > 0 && g_strcmp0(*tmp, "/qio/heartbeat:0=null") != 0) {
					g_async_queue_push(hc->aq, *tmp);
				} else {
					g_free(*tmp);
				}
				tmp++;
			}

			g_free(msgs);

			headers.body[body_len] = replacement;
		}

		g_string_erase(buff, 0, head_len + body_len);
	}

	qev_buffer_put(buff);
}

static void _send(struct httpc *hc, const gchar *msg)
{
	gint err;
	qev_fd_t tmp;
	GString *buff = qev_buffer_get();

	g_mutex_lock(&hc->lock);

	g_string_printf(buff, MSG_HEADERS, hc->sid, strlen(msg), msg);

	err = send(hc->waiting, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	tmp = hc->waiting;
	hc->waiting = hc->polling;
	hc->polling = tmp;

	g_mutex_unlock(&hc->lock);

	qev_buffer_put(buff);
}

static void* _httpc_thread(void *hc_)
{
	guint i;
	struct httpc *hc = hc_;

	while (hc->th_run) {
		qev_fd_t fds[] = {
			hc->polling,
			hc->waiting,
		};

		for (i = 0; i < G_N_ELEMENTS(fds); i++) {
			_drain(hc, fds[i]);
		}

		g_usleep(100);
	}

	return NULL;
}

static gchar* _next_msg(struct httpc *hc)
{
	return g_async_queue_pop(hc->aq);
}

static void _next(struct httpc *hc, const gchar *msg)
{
	gchar *next = _next_msg(hc);
	ck_assert_str_eq(msg, next);
	g_free(next);
}

static void _assert_status_code(qev_fd_t s, const guint status)
{
	gint err;
	GString *resp = qev_buffer_get();
	gchar buff[1024];

	g_string_printf(resp, "HTTP/1.0 %u", status);

	err = recv(s, buff, sizeof(buff), 0);
	buff[err] = '\0';

	ck_assert(g_str_has_prefix(buff, resp->str));

	qev_buffer_put(resp);
}

START_TEST(test_http_sane)
{
	struct httpc *hc = _httpc_new();

	_send(hc, "/qio/on:1=\"/test/good\"\n");
	_next(hc, "/qio/callback/1:0={\"code\":200,\"data\":null}");
	_send(hc, "/qio/off:2=\"/test/good\"");
	_next(hc, "/qio/callback/2:0={\"code\":200,\"data\":null}");

	_httpc_free(hc);
}
END_TEST

START_TEST(test_http_replace_poller)
{
	guint i;
	struct httpc *hc = _httpc_new();

	for (i = 0; i < 10; i++) {
		_send(hc, "/qio/ping:0=null");
	}

	_send(hc, "/qio/ping:1=null");
	_next(hc, "/qio/callback/1:0={\"code\":200,\"data\":null}");

	_httpc_free(hc);
}
END_TEST

static void _http_heartbeat_foreach(struct client *client, void *clients_)
{
	static guint i = 0;
	struct client **clients = clients_;
	clients[i++] = qev_ref(client);
}

static struct client* _http_heartbeat_get_poller(struct client *clients[3])
{
	guint i;
	struct client *poller = NULL;

	while (poller == NULL) {
		g_usleep(100);

		for (i = 1; i < 3; i++) {
			if (clients[i]->http.client != NULL) {
				poller = clients[i];
				break;
			}
		}
	}

	return poller;
}

START_TEST(test_http_heartbeat)
{
	guint i;
	guint clientsi;
	struct client *poller;
	struct client *all_clients[3];
	struct client *clients[2];
	struct client *surrogate = NULL;
	struct httpc *hc = _httpc_new();

	// Flush out all the connections so they're active and associated with HTTP
	for (i = 0; i < 10; i++) {
		_send(hc, "/qio/ping:0=null");
	}

	qev_foreach(_http_heartbeat_foreach, 1, all_clients);

	for (clientsi = i = 0; i < G_N_ELEMENTS(all_clients); i++) {
		if (qev_is_surrogate(all_clients[i])) {
			ck_assert(surrogate == NULL);
			surrogate = all_clients[i];
		} else {
			clients[clientsi++] = all_clients[i];
		}
	}

	ck_assert(clientsi == G_N_ELEMENTS(clients));

	for (i = 0; i < G_N_ELEMENTS(clients); i++) {
		QEV_WAIT_FOR(clients[i]->protocol.prot == protocol_http);
		QEV_WAIT_FOR(clients[i]->qev_client.rbuff == NULL);
	}

	test_heartbeat();

	for (i = 0; i < G_N_ELEMENTS(all_clients); i++) {
		ck_assert(!all_clients[i]->qev_client._flags.closed);
	}

	poller = _http_heartbeat_get_poller(clients);

	/*
	 * Surrogates should be left alone whenever they have a client attached
	 */
	surrogate->last_send = qev_monotonic - QEV_SEC_TO_USEC(120);
	test_heartbeat();
	ck_assert_ptr_eq(surrogate->http.client, poller);

	for (i = 0; i < G_N_ELEMENTS(all_clients); i++) {
		ck_assert(!all_clients[i]->qev_client._flags.closed);
	}

	/*
	 * Pollers should be flushed after polling for too long
	 */
	poller->last_send = qev_monotonic - QEV_SEC_TO_USEC(120);
	test_heartbeat();
	ck_assert_ptr_eq(poller->http.client, NULL);
	ck_assert_ptr_eq(surrogate->http.client, NULL);

	/*
	 * Surrogates should be closed after not having a client attached for
	 * too long
	 */
	surrogate->last_send = qev_monotonic - QEV_SEC_TO_USEC(120);
	test_heartbeat();
	ck_assert(surrogate->qev_client._flags.closed);

	/*
	 * Pollers should be closed after no clients and no sending for too long
	 */
	for (i = 0; i < G_N_ELEMENTS(clients); i++) {
		ck_assert(!clients[i]->qev_client._flags.closed);
		ck_assert_ptr_eq(clients[i]->http.client, NULL);
		clients[i]->last_send = qev_monotonic - QEV_SEC_TO_USEC(240);
		test_heartbeat();
	}

	for (i = 0; i < G_N_ELEMENTS(all_clients); i++) {
		ck_assert(all_clients[i]->qev_client._flags.closed);
	}

	for (i = 0; i < G_N_ELEMENTS(all_clients); i++) {
		qev_unref0(&all_clients[i]);
	}

	_httpc_free(hc);
}
END_TEST

START_TEST(test_http_surrogate)
{
	const gchar *headers =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 0\n\n";
	const gchar *ping =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9 HTTP/1.1\n"
		"Content-Length: 16\n\n"
		"/qio/ping:1=null";

	gint err;
	struct client *surrogate;
	qev_fd_t s = test_socket();
	struct client *client = test_get_client_raw();

	ck_assert(!qev_is_surrogate(client));

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	QEV_WAIT_FOR(client->http.client != NULL);
	surrogate = test_get_surrogate();

	client->last_send = qev_monotonic - QEV_SEC_TO_USEC(60);

	test_heartbeat();

	ck_assert(client->http.client == NULL);
	_assert_status_code(s, 200);

	ck_assert_int_gt(surrogate->last_send, qev_monotonic - QEV_SEC_TO_USEC(1));

	err = send(s, ping, strlen(ping), 0);
	ck_assert_int_eq(err, strlen(ping));
	_assert_status_code(s, 200);

	close(s);
}
END_TEST

START_TEST(test_http_iframe)
{
	const gchar *header = "GET /iframe?instanceid=123 HTTP/1.1\n\n";

	gint err;
	gchar buff[0xffff];
	qev_fd_t s = test_socket();

	err = send(s, header, strlen(header), 0);
	ck_assert_int_eq(strlen(header), err);

	err = recv(s, buff, sizeof(buff), 0);
	ck_assert_int_gt(err, 0);
	buff[err] = '\0';

	ck_assert(strstr(buff, "<!DOCTYPE html>") != NULL);

	close(s);
}
END_TEST

START_TEST(test_http_long_uuid)
{
	const gchar *headers =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b91010101010101001&connect=true HTTP/1.1\n" \
		"Content-Length: 16\n\n" \
		"/qio/ping:1=null";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));
	_assert_status_code(s, 200);

	close(s);
}
END_TEST

START_TEST(test_http_disabled)
{
	const gchar *header = "GET /iframe HTTP/1.1\n\n";

	ck_assert(g_file_set_contents("test_http_disabled.ini",
		"[quick.io]\n"
		"public-address =", -1, NULL));
	test_setup_with_config("test_http_disabled.ini");
	unlink("test_http_disabled.ini");

	gint err;
	gchar buff[0xffff];
	qev_fd_t s = test_socket();

	err = send(s, header, strlen(header), 0);
	ck_assert_int_eq(strlen(header), err);

	err = recv(s, buff, sizeof(buff), 0);
	ck_assert_int_gt(err, 0);
	buff[err] = '\0';

	ck_assert(strstr(buff, "501 Not Implemented") != NULL);
	ck_assert(strstr(buff, "window.parent.postMessage") != NULL);

	close(s);
}
END_TEST

static void _edge_send_opening_headers(qev_fd_t s)
{
	const gchar *header =
		"POST / HTTP/1.1\n"
		"Content-Length: 0\n\n";

	gint err;

	err = send(s, header, strlen(header), 0);
	ck_assert_int_eq(strlen(header), err);

	_assert_status_code(s, 403);
}

START_TEST(test_http_edge_invalid_header_newlines)
{
	qev_fd_t s = test_socket();
	_edge_send_opening_headers(s);
	close(s);
}
END_TEST

START_TEST(test_http_edge_partial_headers_0)
{
	gint err;
	struct client *client;
	qev_fd_t s = test_socket();

	_edge_send_opening_headers(s);

	client = test_get_client();
	err = send(s, "POST", 4, 0);
	ck_assert_int_eq(err, 4);

	QEV_WAIT_FOR(client->timeout != NULL);

	err = send(s, " / HTTP/1.1\nContent-Length: 0\n\n", 31, 0);
	ck_assert_int_eq(err, 31);

	_assert_status_code(s, 403);

	close(s);
}
END_TEST

START_TEST(test_http_edge_partial_headers_1)
{
	gint err;
	qev_fd_t s = test_socket();
	struct client *client = test_get_client_raw();

	err = send(s, "POST", 4, 0);
	ck_assert_int_eq(err, 4);

	QEV_WAIT_FOR(client->timeout != NULL);

	err = send(s, " / HTTP/1.1\nContent-Length: 0\n\n", 31, 0);
	ck_assert_int_eq(err, 31);

	_assert_status_code(s, 403);

	close(s);
}
END_TEST

START_TEST(test_http_10)
{
	const gchar *headers =
		"POST / HTTP/1.0\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 403);
	test_client_dead(s);

	close(s);
}
END_TEST

START_TEST(test_http_10_keepalive)
{
	const gchar *headers =
		"POST / HTTP/1.0\n"
		"Connection: keep-alive\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();
	struct client *client = test_get_client_raw();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 403);

	ck_assert(!client->qev_client._flags.closed);

	close(s);
}
END_TEST

START_TEST(test_http_11_connection_close)
{
	const gchar *headers =
		"POST / HTTP/1.1\n"
		"Connection: close\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 403);
	test_client_dead(s);

	close(s);
}
END_TEST

START_TEST(test_http_partial_body)
{
	const gchar *headers =
		"POST / HTTP/1.1\n"
		"Content-Length: 16\n\n";
	const gchar *body = "/qio/ping:0=null";

	gint err;
	qev_fd_t s = test_socket();
	struct client *client = test_get_client_raw();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	QEV_WAIT_FOR(client->timeout != NULL);

	err = send(s, body, strlen(body), 0);
	ck_assert_int_eq(err, strlen(body));

	_assert_status_code(s, 403);
	ck_assert(!client->qev_client._flags.closed);

	close(s);
}
END_TEST

START_TEST(test_http_tons_of_newlines)
{
	struct httpc *hc = _httpc_new();

	_send(hc, "\n\n\n\n\n\n\n\n\n\n/qio/ping:1=null\n\n\n\n\n\n\n\n");
	_next(hc, "/qio/callback/1:0={\"code\":200,\"data\":null}");

	_httpc_free(hc);
}
END_TEST

START_TEST(test_http_oversized_request)
{
	const gchar *headers =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();
	struct client *client = test_get_client_raw();

	ck_assert(!qev_is_surrogate(client));

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	QEV_WAIT_FOR(client->http.flags.in_request);

	qev_close(client, QEV_CLOSE_OUT_OF_MEM);
	_assert_status_code(s, 413);

	close(s);
}
END_TEST

START_TEST(test_http_close_with_surrogate)
{
	const gchar *headers =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();
	struct client *client = test_get_client_raw();

	ck_assert(!qev_is_surrogate(client));

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	QEV_WAIT_FOR(client->http.client != NULL);
	QEV_WAIT_FOR(client->http.client->http.client != NULL);

	qev_close(client, HTTP_DONE);
	_assert_status_code(s, 403);

	close(s);
}
END_TEST

START_TEST(test_http_close_on_replace)
{
	const gchar *conn =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 0\n\n";
	const gchar *p =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9 HTTP/1.1\n"
		"Content-Length: 18\n\n"
		"/test/close:0=null";

	gint err;
	qev_fd_t s1 = test_socket();
	struct client *client = test_get_client_raw();
	qev_fd_t s2 = test_socket();

	err = send(s1, conn, strlen(conn), 0);
	ck_assert_int_eq(err, strlen(conn));

	QEV_WAIT_FOR(client->http.client != NULL);

	err = send(s2, p, strlen(p), 0);
	ck_assert_int_eq(err, strlen(p));

	_assert_status_code(s1, 403);
	_assert_status_code(s2, 403);

	close(s1);
	close(s2);
}
END_TEST

START_TEST(test_http_incoming_wait)
{
	struct httpc *hc = _httpc_new();
	struct client *surrogate = test_get_surrogate();
	struct protocol_frames pframes = protocol_http_frame("/test", "", 0, "null");

	_send(hc, "/qio/ping:0=null");
	QEV_WAIT_FOR(surrogate->http.client != NULL);
	surrogate->http.flags.incoming = TRUE;

	protocol_http_send(surrogate, &pframes);

	_send(hc, "/qio/ping:1=null");
	_next(hc, "/test:0=null");
	_next(hc, "/qio/callback/1:0={\"code\":200,\"data\":null}");

	qev_buffer_put(pframes.def);
	qev_buffer_put(pframes.raw);

	_httpc_free(hc);
}
END_TEST

START_TEST(test_http_incoming_no_wait)
{
	struct httpc *hc = _httpc_new();
	struct client *surrogate = test_get_surrogate();
	struct protocol_frames pframes = protocol_http_frame("/test", "", 0, "null");

	_send(hc, "/qio/ping:0=null");
	QEV_WAIT_FOR(surrogate->http.client != NULL);
	QEV_WAIT_FOR(!surrogate->http.flags.incoming);

	protocol_http_send(surrogate, &pframes);

	_next(hc, "/test:0=null");

	qev_buffer_put(pframes.def);
	qev_buffer_put(pframes.raw);

	_httpc_free(hc);
}
END_TEST

START_TEST(test_http_requests_on_same_socket)
{
	guint i;
	gint err;
	gchar sid[32];
	GString *buff = qev_buffer_get();
	GString *msg = qev_buffer_get();
	qev_fd_t s = test_socket();

	_uuid(sid);

	// Opening handshake
	g_string_printf(buff, INIT_HEADERS, sid);
	err = send(s, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	err = recv(s, buff->str, buff->allocated_len, 0);
	ck_assert(err > 0);
	g_string_set_size(buff, err);
	ck_assert(strstr(buff->str, "/qio/callback/1") != NULL);

	// First poll, sending nothing
	g_string_printf(buff, MSG_HEADERS, sid, msg->len, msg->str);
	err = send(s, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	// Send a ping with an expected callback: the first request should bring
	// back the ping's callback
	g_string_assign(msg, "/qio/ping:2=null");
	g_string_printf(buff, MSG_HEADERS, sid, msg->len, msg->str);
	err = send(s, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	for (i = 0; i < 2; i++) {
		err = recv(s, buff->str, buff->allocated_len, 0);
		ck_assert(err > 0);
		g_string_set_size(buff, err);
		if (strstr(buff->str, "/qio/callback/2") != NULL) {
			break;
		}
	}

	ck_assert(strstr(buff->str, "/qio/callback/2") != NULL);

	qev_buffer_put(msg);
	qev_buffer_put(buff);
	close(s);
}
END_TEST

START_TEST(test_http_error_uuid)
{
	const gchar *headers =
		"POST /?sid=abcd1234 HTTP/1.0\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 403);
	test_client_dead(s);

	close(s);
}
END_TEST

START_TEST(test_http_error_not_post)
{
	const gchar *headers =
		"GET / HTTP/1.1\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 405);

	close(s);
}
END_TEST

START_TEST(test_http_error_not_http)
{
	const gchar *headers = "GET\n\n";

	gint err;
	qev_fd_t s = test_socket();

	_edge_send_opening_headers(s);

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 400);

	test_client_dead(s);

	close(s);
}
END_TEST

START_TEST(test_http_error_invalid_content_length)
{
	const gchar *headers =
		"POST / HTTP/1.1\n"
		"Content-Length: af09\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 400);
	test_client_dead(s);

	close(s);
}
END_TEST

START_TEST(test_http_error_content_length_too_big)
{
	gint err;
	qev_fd_t s = test_socket();
	GString *buff = qev_buffer_get();

	g_string_printf(buff,
		"POST / HTTP/1.1\n"
		"Content-Length: 0\n\n"
		"POST / HTTP/1.1\n"
		"Content-Length: %lu\n\n",
		G_MAXUINT64);

	err = send(s, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	test_client_dead(s);

	qev_buffer_put(buff);
	close(s);
}
END_TEST

START_TEST(test_http_error_no_content_length)
{
	const gchar *headers = "POST / HTTP/1.1\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 411);
	test_client_dead(s);

	close(s);
}
END_TEST

START_TEST(test_http_error_invalid_upgrade)
{
	const gchar *headers =
		"GET / HTTP/1.1\r\n" \
		"Host: localhost\r\n" \
		"Sec-WebSocket-Key: JF+JVs2N4NAX39FAAkkdIA==\r\n" \
		"Upgrade: websocket\r\n" \
		"Connection: Upgrade\r\n\r\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_assert_status_code(s, 400);

	close(s);
}
END_TEST

START_TEST(test_http_error_invalid_events)
{
	const gchar *headers =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n" \
		"Content-Length: 19\n\n" \
		"im an event\n" \
		"/test:0";
	const gchar *headers_after =
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9 HTTP/1.1\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));
	_assert_status_code(s, 400);

	err = send(s, headers_after, strlen(headers_after), 0);
	ck_assert_int_eq(err, strlen(headers_after));
	_assert_status_code(s, 403);

	close(s);
}
END_TEST

START_TEST(test_http_error_invalid_uuid)
{
	const gchar *headers =
		"POST /?sid=0123&connect=true HTTP/1.1\n" \
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));
	_assert_status_code(s, 403);

	close(s);
}
END_TEST

START_TEST(test_http_error_invalid_uuid_format)
{
	const gchar *headers =
		"POST /?sid=550e8400-e29b-41d4-a716-446655440000&connect=true HTTP/1.1\n" \
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));
	_assert_status_code(s, 403);

	close(s);
}
END_TEST

int main()
{
	SRunner *sr;
	Suite *s;
	TCase *tcase;
	test_new("protocol_http", &sr, &s);

	tcase = tcase_create("Sane");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_http_sane);
	tcase_add_test(tcase, test_http_replace_poller);
	tcase_add_test(tcase, test_http_heartbeat);
	tcase_add_test(tcase, test_http_surrogate);
	tcase_add_test(tcase, test_http_iframe);
	tcase_add_test(tcase, test_http_long_uuid);

	tcase = tcase_create("Disabled");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, NULL, test_teardown);
	tcase_add_test(tcase, test_http_disabled);

	tcase = tcase_create("Edges");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_http_edge_invalid_header_newlines);
	tcase_add_test(tcase, test_http_edge_partial_headers_0);
	tcase_add_test(tcase, test_http_edge_partial_headers_1);
	tcase_add_test(tcase, test_http_10);
	tcase_add_test(tcase, test_http_10_keepalive);
	tcase_add_test(tcase, test_http_11_connection_close);
	tcase_add_test(tcase, test_http_partial_body);
	tcase_add_test(tcase, test_http_tons_of_newlines);
	tcase_add_test(tcase, test_http_oversized_request);
	tcase_add_test(tcase, test_http_close_with_surrogate);
	tcase_add_test(tcase, test_http_close_on_replace);
	tcase_add_test(tcase, test_http_incoming_wait);
	tcase_add_test(tcase, test_http_incoming_no_wait);
	tcase_add_test(tcase, test_http_requests_on_same_socket);

	tcase = tcase_create("Errors");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_http_error_uuid);
	tcase_add_test(tcase, test_http_error_not_post);
	tcase_add_test(tcase, test_http_error_not_http);
	tcase_add_test(tcase, test_http_error_invalid_content_length);
	tcase_add_test(tcase, test_http_error_content_length_too_big);
	tcase_add_test(tcase, test_http_error_no_content_length);
	tcase_add_test(tcase, test_http_error_invalid_upgrade);
	tcase_add_test(tcase, test_http_error_invalid_events);
	tcase_add_test(tcase, test_http_error_invalid_uuid);
	tcase_add_test(tcase, test_http_error_invalid_uuid_format);

	return test_do(sr);
}
