/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#include "test.h"

#ifndef PORT
	#define PORT 41238
#endif

/**
 * Location of this test's config file
 */
#define CONFIG_FILE "test_" G_STRINGIFY(PORT) ".ini"

/**
 * Default configuration for tests
 */
#define CONFIG \
	"[quick-event]\n" \
	"poll-wait = 1\n" \
	"tcp-nodelay = true\n" \
	"timeout = %d\n" \
	"[quick.io]\n" \
	"public-address = localhost\n" \
	"bind-address = localhost\n" \
	"bind-port = %d\n" \
	"bind-address-ssl = localhost\n" \
	"bind-port-ssl = %d\n" \
	"bind-path = quickio.%d.sock\n" \
	"ssl-key-path-0 = ../lib/quick-event/certs/rsa/test.key\n" \
	"ssl-cert-path-0 = ../lib/quick-event/certs/rsa/test.crt\n" \
	"ssl-key-path-1 = ../lib/quick-event/certs/ecdsa/test.key\n" \
	"ssl-cert-path-1 = ../lib/quick-event/certs/ecdsa/test.crt\n" \
	"support-flash = false\n" \
	"user = %s\n" \
	"[quick.io-apps]\n" \
	"/test = ./app_test_sane\n" \
	"[test_app_sane]\n" \
	"sane-value = 172346\n"

struct _wait_for_buff {
	gboolean good;
	guint len;
};

/**
 * Check doesn't make its own copy of this guy...
 */
static gchar *_xml_file = NULL;

static gboolean _do_heartbeat = FALSE;

static void _wait_for_buff_cb(struct client *client, void *wfb_)
{
	struct _wait_for_buff *wfb = wfb_;

	QEV_WAIT_FOR(client->qev_client._read_operations == 0);
	QEV_WAIT_FOR(client->qev_client.rbuff != NULL);
	QEV_WAIT_FOR(client->qev_client.rbuff->len >= wfb->len);

	wfb->good = TRUE;
}

static void _get_client_cb(struct client *client, void *ptr_)
{
	struct client **ptr = ptr_;
	if (*ptr == NULL) {
		*ptr = client;
	}
}

/**
 * Heartbeats need to run from the QEV event loop: qev_foreach() only runs
 * unlocked from QEV threads, and since we need to test removal of clients from
 * heartbeats, we have to run there.
 */
static void _heartbeat_timer()
{
	if (_do_heartbeat) {
		periodic_run();
		_do_heartbeat = FALSE;
	}
}

void test_new(
	const gchar *suite_name,
	SRunner **sr,
	Suite **s)
{
	_xml_file = g_strdup_printf("test_%s.xml", suite_name);

	*s = suite_create(suite_name);
	*sr = srunner_create(*s);
	srunner_set_xml(*sr, _xml_file);
}

void test_config()
{
	gboolean configed;
	GString *c = qev_buffer_get();

	g_string_printf(c, CONFIG,
			100,
			PORT, PORT + 1, PORT, getlogin());
	configed = g_file_set_contents(CONFIG_FILE, c->str, -1, NULL);
	ASSERT(configed, "Could not create test config file");

	qev_buffer_put(c);
}

void test_config_rm()
{
	unlink(CONFIG_FILE);
}

gboolean test_do(SRunner *sr)
{
	gint failures;

	test_config();

	srunner_run_all(sr, CK_NORMAL);
	failures = srunner_ntests_failed(sr);
	srunner_free(sr);

	unlink(CONFIG_FILE);
	test_config_rm();

	return failures > 0;
}

void test_setup()
{
	gchar *args[] = {"test", "--config-file=" CONFIG_FILE};
	qio_main(2, args);
	qev_timer(_heartbeat_timer, 0, 1);
}

void test_setup_with_config(gchar *config)
{
	gchar *args[] = {"test", "--config-file=" CONFIG_FILE, "-f", config};
	qio_main(4, args);
	qev_timer(_heartbeat_timer, 0, 1);
}

void test_teardown()
{
	qev_mock_done();
	qev_exit();
}

qev_fd_t test_socket()
{
	gint err;
	qev_fd_t sock;
	gchar port[6];
	gint on = 1;
	struct addrinfo *res = NULL;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	snprintf(port, sizeof(port), "%u", PORT);

	ck_assert(getaddrinfo("localhost", port, &hints, &res) == 0);

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	ck_assert(sock != -1);

	err = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on));
	ck_assert(err == 0);

	err = connect(sock, res->ai_addr, res->ai_addrlen);
	if (err != 0) {
		PERROR("_create_socket()");
		ck_abort();
	}

	freeaddrinfo(res);

	return sock;
}

qev_fd_t test_client()
{
	gint err;
	gchar buff[12];
	qev_fd_t tc;

	tc = test_socket();
	err = send(tc, "/qio/ohai", 9, MSG_NOSIGNAL);
	ck_assert(err == 9);
	err = recv(tc, buff, sizeof(buff), 0);
	ck_assert_int_eq(err, 9);

	buff[9] = '\0';
	ck_assert(g_strcmp0(buff, "/qio/ohai") == 0);

	return tc;
}

void test_send(
	qev_fd_t tc,
	const gchar *data)
{
	test_send_len(tc, data, strlen(data));
}

void test_send_len(
	qev_fd_t tc,
	const gchar *data,
	const guint64 len)
{
	gint err;
	guint64 wlen = len;
	union {
		guint64 i;
		gchar c[sizeof(guint64)];
	} size;

	size.i = GUINT64_TO_BE(wlen);

	err = send(tc, size.c, sizeof(size), MSG_NOSIGNAL);
	ck_assert_int_eq(err, sizeof(size));
	err = send(tc, data, wlen, MSG_NOSIGNAL);
	ck_assert_int_eq(err, wlen);
}

guint64 test_recv(
	qev_fd_t tc,
	gchar *data,
	const guint64 len)
{
	guint64 err;
	guint64 r = 0;
	union {
		guint64 i;
		gchar c[sizeof(guint64)];
	} size;
	data[0] = '\0';

	err = recv(tc, size.c, sizeof(size), MSG_WAITALL);
	ck_assert(err == sizeof(size));
	size.i = GUINT64_FROM_BE(size.i);

	ck_assert_msg(size.i < len, "Buffer not large enough to read response");

	while (r < size.i) {
		err = recv(tc, data + r, size.i - r, 0);
		ck_assert(err > 0);
		r += err;
	}

	data[size.i] = '\0';

	return size.i;
}
void test_msg(qev_fd_t tc, const gchar *data)
{
	gchar buff[32768];
	test_recv(tc, buff, sizeof(buff));
	ck_assert_str_eq(data, buff);
}

void test_cb(qev_fd_t tc, const gchar *msg, const gchar *data)
{
	test_send(tc, msg);
	test_msg(tc, data);
}

void test_ping(qev_fd_t tc)
{
	test_cb(tc,
		"/qio/ping:1=null",
		"/qio/callback/1:0={\"code\":200,\"data\":null}");
}

void test_wait_for_buff(const guint len)
{
	struct _wait_for_buff wfb = {
		.good = FALSE,
		.len = len,
	};

	while (!wfb.good) {
		qev_foreach(_wait_for_buff_cb, 1, &wfb);
	}
}

struct client* test_get_client()
{
	struct client *client = test_get_client_raw();

	QEV_WAIT_FOR(client->protocol.handshaked);

	return client;
}

struct client* test_get_client_raw()
{
	struct client *client = NULL;

	while (client == NULL) {
		g_usleep(100);
		qev_foreach(_get_client_cb, 1, &client);
	}

	return client;
}

void test_heartbeat()
{
	_do_heartbeat = TRUE;
	QEV_WAIT_FOR(_do_heartbeat == FALSE);
}

void test_client_dead(qev_fd_t tc)
{
	guint i;
	gint err;
	gchar buff[128];

	for (i = 0; i < 4096; i++) {
		err = recv(tc, buff, sizeof(buff), MSG_DONTWAIT);

		if (err == 0) {
			return;
		}

		g_usleep(1);
	}

	ck_abort_msg("Client was not killed by server.");
}
