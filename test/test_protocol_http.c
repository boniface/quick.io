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
	"Content-Length: 0\r\n\r\n"

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

static struct httpc *_hc = NULL;

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

	g_string_printf(buff, INIT_HEADERS, hc->sid);
	err = send(hc->polling, buff->str, buff->len, 0);
	ck_assert_int_eq(err, buff->len);

	hc->th = g_thread_new("httpc", _httpc_thread, hc);

	qev_buffer_put(buff);

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

static void _setup()
{
	test_setup();
	_hc = _httpc_new();
}

static void _teardown()
{
	_httpc_free(_hc);
	_hc = NULL;

	test_teardown();
}

static void _drain(struct httpc *hc, qev_fd_t sock)
{
	GString *buff = qev_buffer_get();

	qev_buffer_ensure(buff, buff->len + 2048);

	while (TRUE) {
		gssize to_read = buff->allocated_len - buff->len;
		gint err = recv(sock, buff->str + buff->len, to_read, 0);

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

	msg = msg ? : "";
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
		struct pollfd p[] = {
			{	.fd = hc->polling,
				.events = POLLIN,
			},
			{	.fd = hc->waiting,
				.events = POLLIN,
			},
		};

		poll(p, G_N_ELEMENTS(p), 1);
		for (i = 0; i < G_N_ELEMENTS(p); i++) {
			if (p[i].revents == 0) {
				continue;
			}

			_drain(hc, p[i].fd);
		}
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
	_send(_hc, "/qio/on:1=\"/test\"\n");
	_next(_hc, "/qio/callback/1:0={\"code\":404,\"data\":null,\"err_msg\":null}");
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
	tcase_set_timeout(tcase, 10);
	tcase_add_checked_fixture(tcase, _setup, _teardown);
	tcase_add_test(tcase, test_http_sane);

	// @todo implement HTTP/1.0 support
	// tcase_add_test(tcase, test_http_10);

	return test_do(sr);
}
