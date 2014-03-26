/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include <poll.h>
#include "test.h"

// START_TEST(test_rfc6455_handshake_http_invalid_headers)
// {
// 	const gchar *headers =
// 		"GET / HTTP/1.1\r\n"
// 		"herp derp\r\n\r\n";

// 	gint err;
// 	gchar buff[1024];
// 	qev_fd_t ts = test_socket();

// 	ck_assert_int_eq(send(ts, headers, strlen(headers), 0), strlen(headers));

// 	err = recv(ts, buff, sizeof(buff), 0);
// 	buff[err] = '\0';

// 	ck_assert_str_eq(buff,
// 		"HTTP/1.1 426 Upgrade Required\r\n"
// 		"Connection: keep-alive\r\n"
// 		"Keep-Alive: timeout=60\r\n"
// 		"Cache-Control: no-cache, no-store, must-revalidate\r\n"
// 		"Pragma: no-cache\r\n"
// 		"Expires: 0\r\n\r\n");

// 	test_client_dead(ts);
// 	close(ts);
// }
// END_TEST

#define INIT_HEADERS \
	"POST /?sid=%s&connect=true HTTP/1.1\r\n" \
	"Content-Length: 21\r\n\r\n" \
	"/qio/hostname:1=null\n"

#define MSG_HEADERS \
	"POST /?sid=%s HTTP/1.1\r\n" \
	"Content-Length: %lu\r\n\r\n" \
	"%s"

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

static struct httpc* _httpc_new()
{
	gint err;
	uuid_t uuid;
	gchar sid[37];
	GString *buff = qev_buffer_get();
	struct httpc *hc = g_slice_alloc0(sizeof(*hc));

	uuid_generate_random(uuid);
	uuid_unparse(uuid, sid);

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
		gchar *body;
		guint64 head_len;
		guint64 body_len;
		struct protocol_headers headers;

		body = strstr(buff->str, "\r\n\r\n");
		ASSERT(body != NULL, "No body found in response: %s", buff->str);
		body[3] = '\0';
		body += 4;

		head_len = strlen(buff->str) + 1;

		protocol_util_headers(buff->str, &headers);
		cl = protocol_util_headers_get(&headers, "Content-Length");
		body_len = g_ascii_strtoull(cl, NULL, 10);

		ASSERT(buff->len >= (head_len + body_len),
			"Incomplete read from server: exepected at least %lu, got %lu",
			head_len + body_len, buff->len);

		ASSERT(strstr(buff->str, "200 OK") != NULL,
			"Didn't get 200, got: %s", buff->str);

		if (body_len > 0) {
			gchar replacement = body[body_len];
			body[body_len] = '\0';

			gchar **msgs = g_strsplit(body, "\n", -1);
			gchar **tmp = msgs;

			while (*tmp != NULL) {
				if (strlen(*tmp) > 0) {
					g_async_queue_push(hc->aq, *tmp);
				} else {
					g_free(*tmp);
				}
				tmp++;
			}

			g_free(msgs);

			body[body_len] = replacement;
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
	struct client *poller;
	struct client *clients[3];
	struct httpc *hc = _httpc_new();

	// Flush out all the connections so they're active and associated with HTTP
	for (i = 0; i < 10; i++) {
		_send(hc, "/qio/ping:0=null");
	}

	// Clients iterated in reverse: surrogate is always first
	qev_foreach(_http_heartbeat_foreach, 1, clients);
	ck_assert(qev_is_surrogate(clients[0]));
	ck_assert(!qev_is_surrogate(clients[1]));
	ck_assert(!qev_is_surrogate(clients[2]));

	for (i = 1; i < G_N_ELEMENTS(clients); i++) {
		QEV_WAIT_FOR(clients[i]->protocol.prot == protocol_http);
		QEV_WAIT_FOR(clients[i]->qev_client.rbuff == NULL);
	}

	test_heartbeat();

	for (i = 0; i < G_N_ELEMENTS(clients); i++) {
		ck_assert(!clients[i]->qev_client._flags.closed);
	}

	poller = _http_heartbeat_get_poller(clients);

	/*
	 * Surrogates should be left alone whenever they have a client attached
	 */
	clients[0]->last_send = qev_monotonic - QEV_SEC_TO_USEC(120);
	test_heartbeat();
	ck_assert_ptr_eq(clients[0]->http.client, poller);

	for (i = 0; i < G_N_ELEMENTS(clients); i++) {
		ck_assert(!clients[i]->qev_client._flags.closed);
	}

	/*
	 * Pollers should be flushed after polling for too long
	 */
	poller->last_send = qev_monotonic - QEV_SEC_TO_USEC(120);
	test_heartbeat();
	ck_assert_ptr_eq(poller->http.client, NULL);
	ck_assert_ptr_eq(clients[0]->http.client, NULL);

	/*
	 * Surrogates should be closed after not having a client attached for
	 * too long
	 */
	clients[0]->last_send = qev_monotonic - QEV_SEC_TO_USEC(120);
	test_heartbeat();
	ck_assert(clients[0]->qev_client._flags.closed);

	/*
	 * Pollers should be closed after no clients and no sending for too long
	 */
	for (i = 1; i < G_N_ELEMENTS(clients); i++) {
		ck_assert(!clients[i]->qev_client._flags.closed);
		ck_assert_ptr_eq(clients[i]->http.client, NULL);
		clients[i]->last_send = qev_monotonic - QEV_SEC_TO_USEC(240);
		test_heartbeat();
	}

	for (i = 0; i < G_N_ELEMENTS(clients); i++) {
		ck_assert(clients[i]->qev_client._flags.closed);
	}

	for (i = 0; i < G_N_ELEMENTS(clients); i++) {
		qev_unref0(&clients[i]);
	}

	_httpc_free(hc);
}
END_TEST

static void _edge_assert_status_code(qev_fd_t s, const guint status)
{
	gint err;
	GString *resp = qev_buffer_get();
	gchar buff[1024];

	g_string_printf(resp, "HTTP/1.0 %u", status);

	err = recv(s, buff, sizeof(buff), 0);
	err[buff] = '\0';

	if (!g_str_has_prefix(buff, resp->str)) {
		INFO("Got: %s", buff);
	}

	ck_assert(g_str_has_prefix(buff, resp->str));

	qev_buffer_put(resp);
}

static void _edge_send_opening_headers(qev_fd_t s)
{
	const gchar *header =
		"POST / HTTP/1.1\n"
		"Content-Length: 0\n\n";

	gint err;

	err = send(s, header, strlen(header), 0);
	ck_assert_int_eq(strlen(header), err);

	_edge_assert_status_code(s, 403);
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

	_edge_assert_status_code(s, 403);

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

	_edge_assert_status_code(s, 403);

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

	_edge_assert_status_code(s, 403);
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

	_edge_assert_status_code(s, 403);

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

	_edge_assert_status_code(s, 403);
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

	_edge_assert_status_code(s, 403);
	ck_assert(!client->qev_client._flags.closed);

	close(s);
}
END_TEST

START_TEST(test_http_oversized_request)
{
	const gchar *headers =
		"POST /?sid=16a0dd9a-4e55-4a9f-9452-0c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 0\n\n";

	gint err;
	qev_fd_t s = test_socket();
	struct client *client = test_get_client_raw();

	ck_assert(!qev_is_surrogate(client));

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	QEV_WAIT_FOR(client->http.flags.in_request);

	qev_close(client, QEV_CLOSE_READ_HIGH);
	_edge_assert_status_code(s, 413);

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

	_edge_assert_status_code(s, 403);
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

	_edge_assert_status_code(s, 405);

	close(s);
}
END_TEST

START_TEST(test_http_error_not_http)
{
	const gchar *headers = "I'm too drunk to HTTP. Just figure it out.\n\n";

	gint err;
	qev_fd_t s = test_socket();

	_edge_send_opening_headers(s);

	err = send(s, headers, strlen(headers), 0);
	ck_assert_int_eq(err, strlen(headers));

	_edge_assert_status_code(s, 400);

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

	_edge_assert_status_code(s, 400);
	test_client_dead(s);

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

	_edge_assert_status_code(s, 411);
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

	_edge_assert_status_code(s, 400);

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
	tcase_add_test(tcase, test_http_oversized_request);

	tcase = tcase_create("Errors");
	suite_add_tcase(s, tcase);
	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	tcase_add_test(tcase, test_http_error_uuid);
	tcase_add_test(tcase, test_http_error_not_post);
	tcase_add_test(tcase, test_http_error_not_http);
	tcase_add_test(tcase, test_http_error_invalid_content_length);
	tcase_add_test(tcase, test_http_error_no_content_length);
	tcase_add_test(tcase, test_http_error_invalid_upgrade);

	return test_do(sr);
}
