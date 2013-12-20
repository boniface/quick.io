/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "test.h"

/**
 * Location of this test's config file
 */
#define CONFIG_FILE "test_" G_STRINGIFY(PORT) ".ini"

/**
 * The port that SSL is listening on
 */
#define SSL_PORT (PORT + 1)

/**
 * Default configuration for tests
 */
#define CONFIG \
 	"[quick-event]\n" \
 	"[quickio]\n" \
 	"bind-address = localhost\n" \
 	"bind-port = %d\n" \
 	"bind-address-ssl = localhost\n" \
 	"bind-port-ssl = %d\n" \
 	"ssl-key-path-0 = ../lib/quick-event/certs/rsa/test.key\n" \
 	"ssl-cert-path-0 = ../lib/quick-event/certs/rsa/test.crt\n" \
 	"ssl-key-path-1 = ../lib/quick-event/certs/ecdsa/test.key\n" \
 	"ssl-cert-path-1 = ../lib/quick-event/certs/ecdsa/test.crt\n" \
 	"[quick.io-apps]\n" \
 	"/test = ./apps/test_app_sane\n"

struct test_client {
	gboolean is_ssl;

	union {
		SSL *ssl;
		qev_fd_t fd;
	} conn;
};

/**
 * Check doesn't make its own copy of this guy...
 */
static gchar *_xml_file = NULL;

static qev_fd_t _create_socket(const guint port_)
{
	gint err;
	qev_fd_t sock;
	gchar port[6];
	struct addrinfo hints;
	struct addrinfo *res = NULL;

	snprintf(port, sizeof(port), "%u", port_);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ck_assert(getaddrinfo("localhost", port, &hints, &res) == 0);

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	ck_assert(sock != -1);

	err = connect(sock, res->ai_addr, res->ai_addrlen);
	if (err != 0) {
		PERROR("_create_socket()");
		ck_abort();
	}

	freeaddrinfo(res);

	return sock;
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

gboolean test_do(SRunner *sr)
{
	gint failures;
	gboolean configed;

	GString *c = g_string_sized_new(sizeof(CONFIG));
	g_string_printf(c, CONFIG, PORT, SSL_PORT);
	configed = g_file_set_contents(CONFIG_FILE, c->str, -1, NULL);
	g_string_free(c, TRUE);

	if (!configed) {
		CRITICAL("Could not create test config file");
		return 1;
	}

	srunner_run_all(sr, CK_NORMAL);
	failures = srunner_ntests_failed(sr);
	srunner_free(sr);

	unlink(CONFIG_FILE);
	g_free(_xml_file);

	return failures > 0;
}

void test_setup()
{
	gchar *args[] = {"test", "--config-file=" CONFIG_FILE};
	qio_main(2, args);
}

void test_teardown()
{
	qev_exit();
}

TCase* test_add(Suite *s, const gchar *name, ...)
{
	va_list args;
	TFun tfun;
	TCase *tcase = tcase_create(name);

	tcase_add_checked_fixture(tcase, test_setup, test_teardown);
	suite_add_tcase(s, tcase);

	va_start(args, name);

	while (TRUE) {
		tfun = va_arg(args, TFun);

		if (tfun == NULL) {
			break;
		}

		tcase_add_test(tcase, tfun);
	}

	va_end(args);

	return tcase;
}

struct test_client* test_client()
{
	gchar buff[12];
	struct test_client *tclient = g_slice_alloc0(sizeof(*tclient));
	tclient->is_ssl = FALSE;

	tclient->conn.fd = _create_socket(PORT);
	ck_assert(send(tclient->conn.fd, "/qio/ohai", 9, MSG_NOSIGNAL) == 9);
	ck_assert(recv(tclient->conn.fd, buff, sizeof(buff), 0) == 9);

	buff[9] = '\0';
	ck_assert(g_strcmp0(buff, "/qio/ohai") == 0);

	return tclient;
}

void test_send(
	test_client_t *tclient,
	const gchar *data)
{
	test_send_len(tclient, data, strlen(data));
}

void test_send_len(
	test_client_t *tclient,
	const gchar *data,
	const guint64 len)
{
	gint err;
	guint64 wlen = len;
	gchar size[sizeof(guint64)];

	if (len == 0) {
		wlen = strlen(data);
	}

	*((guint64*)size) = GUINT64_TO_BE(wlen);

	if (tclient->is_ssl) {
		err = -1;
	} else {
		err = send(tclient->conn.fd, size, sizeof(size), MSG_NOSIGNAL);
		ck_assert(err == sizeof(size));
		err = send(tclient->conn.fd, data, wlen, MSG_NOSIGNAL);
	}

	ck_assert(err == (gint64)wlen);
}

guint64 test_recv(
	test_client_t *tclient,
	gchar *data,
	const guint64 len)
{
	guint64 err;
	guint64 r = 0;
	guint64 rlen = 0;
	gchar size[sizeof(guint64)];
	data[0] = '\0';

	if (tclient->is_ssl) {
		err = -1;
	} else {
		err = recv(tclient->conn.fd, size, sizeof(size), MSG_WAITALL);
	}

	ck_assert(err == sizeof(size));
	rlen = GUINT64_FROM_BE(*((guint64*)size));

	ck_assert_msg(rlen < len, "Buffer not large enough to read response");

	while (r < rlen) {
		if (tclient->is_ssl) {
			err = -1;
		} else {
			err = recv(tclient->conn.fd, data + r, rlen - r, 0);
		}

		ck_assert(err > 0);
		r += err;
	}

	data[rlen] = '\0';

	return rlen;
}
void test_msg(test_client_t *tclient, const gchar *data)
{
	gchar buff[strlen(data) * 2];

	test_recv(tclient, buff, sizeof(buff));
	ck_assert_str_eq(buff, data);
}

void test_cb(test_client_t *tclient, const gchar *msg, const gchar *data)
{
	test_send(tclient, msg);
	test_msg(tclient, data);
}

void test_close(struct test_client *tclient)
{
	if (tclient->is_ssl) {
		// @todo
	} else {
		close(tclient->conn.fd);
	}

	g_slice_free1(sizeof(*tclient), tclient);
}
