#include "quickio.h"
#include <fcntl.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCK_PATH "/tmp/quickio.bench_routing.sock"

#define CONFIG_FILE "/tmp/quickio.bench_routing.ini"
#define CONFIG \
	"[quick-event]\n" \
	"threads = 1\n" \
	"timeout = 100000\n" \
	"[quick.io]\n" \
	"public-address = localhost\n" \
	"bind-path = " SOCK_PATH

#define RAW_HANDSHAKE "/qio/ohai"

#define RFC6455_HEADERS \
	"GET / HTTP/1.1\r\n" \
	"Host: localhost\r\n" \
	"Sec-WebSocket-Key: JF+JVs2N4NAX39FAAkkdIA==\r\n" \
	"Sec-WebSocket-Protocol: quickio\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Sec-WebSocket-Version: 13\r\n\r\n"

#define RFC6455_HEADERS_RESPONSE \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Protocol: quickio\r\n" \
	"Cache-Control: private, max-age=0\r\n" \
	"Expires: -1\r\n" \
	"Sec-WebSocket-Accept: Nf+/kB4wxkn+6EPeanngB3VZNwU=\r\n\r\n"

#define RFC6455_QIO_HANDSHAKE "\x81\x89""abcd""N""\x13""\n""\x0b""N""\r""\x0b""\x05""\x08"
#define RFC6455_QIO_HANDSHAKE_RESPONSE "\x81\x09/qio/ohai"

void _qev_mem_pressure_return(const GString *buff);

static GString *_buff = NULL;

static qev_fd_t _raw_sock = -1;
static qev_fd_t _rfc6455_sock = -1;
static qev_fd_t _http_sock = -1;

static struct client *_surrogate = NULL;

static gboolean _base_routing(void *nothing G_GNUC_UNUSED)
{
	qev_buffer_clear(_buff);
	g_string_append(_buff, "/qio/ping:0=null");
	protocol_raw_handle(_surrogate, _buff->str);

	return TRUE;
}

static gboolean _network_raw_single(void *nothing G_GNUC_UNUSED)
{
	gint err;
	gchar buff[128];

	err = send(_raw_sock, "\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:1=null", 24, 0);
	if (err != 24) {
		PERROR("Failed to send on raw socket");
		return FALSE;
	}

	recv(_raw_sock, buff, sizeof(buff), 0);

	return TRUE;
}

static gboolean _immediate_raw_single(void *nothing G_GNUC_UNUSED)
{
	qev_buffer_clear(_buff);
	g_string_append_len(_buff, "\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:1=null", 24);
	_surrogate->protocol.prot = protocol_raw;
	protocols_route(_surrogate);

	if (_surrogate->qev_client._wbuff) {
		_qev_mem_pressure_return(_surrogate->qev_client._wbuff);
		qev_buffer_put0(&_surrogate->qev_client._wbuff);
	}

	return TRUE;
}

static gboolean _network_raw_double(void *nothing G_GNUC_UNUSED)
{
	gint err;
	gchar buff[256];

	err = send(_raw_sock,
		"\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:1=null"
		"\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:1=null", 48, 0);
	if (err != 48) {
		PERROR("Failed to send on raw socket");
		return FALSE;
	}

	recv(_raw_sock, buff, sizeof(buff), 0);

	return TRUE;
}

static gboolean _immediate_raw_double(void *nothing G_GNUC_UNUSED)
{
	qev_buffer_clear(_buff);
	g_string_append_len(_buff,
		"\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:1=null"
		"\x00\x00\x00\x00\x00\x00\x00\x10/qio/ping:1=null", 48);
	_surrogate->protocol.prot = protocol_raw;
	protocols_route(_surrogate);

	_qev_mem_pressure_return(_surrogate->qev_client._wbuff);
	qev_buffer_put0(&_surrogate->qev_client._wbuff);

	return TRUE;
}

static gboolean _network_rfc6455_single(void *nothing G_GNUC_UNUSED)
{
	gint err;
	gchar buff[128];

	err = send(_rfc6455_sock, "\x81\x90\x00\x00\x00\x00/qio/ping:1=null", 22, 0);
	if (err != 22) {
		PERROR("Failed to send on raw socket");
		return FALSE;
	}

	recv(_rfc6455_sock, buff, sizeof(buff), 0);

	return TRUE;
}

static gboolean _immediate_rfc6455_single(void *nothing G_GNUC_UNUSED)
{
	qev_buffer_clear(_buff);
	g_string_append_len(_buff, "\x81\x90\x00\x00\x00\x00/qio/ping:1=null", 22);
	_surrogate->protocol.prot = protocol_rfc6455;
	protocols_route(_surrogate);

	_qev_mem_pressure_return(_surrogate->qev_client._wbuff);
	qev_buffer_put0(&_surrogate->qev_client._wbuff);

	return TRUE;
}

static gboolean _network_rfc6455_double(void *nothing G_GNUC_UNUSED)
{
	gint err;
	gchar buff[256];

	err = send(_rfc6455_sock,
		"\x81\x90\x00\x00\x00\x00/qio/ping:1=null"
		"\x81\x90\x00\x00\x00\x00/qio/ping:1=null", 44, 0);
	if (err != 44) {
		PERROR("Failed to send on raw socket");
		return FALSE;
	}

	recv(_rfc6455_sock, buff, sizeof(buff), 0);

	return TRUE;
}

static gboolean _immediate_rfc6455_double(void *nothing G_GNUC_UNUSED)
{
	qev_buffer_clear(_buff);
	g_string_append_len(_buff,
		"\x81\x90\x00\x00\x00\x00/qio/ping:1=null"
		"\x81\x90\x00\x00\x00\x00/qio/ping:1=null", 44);
	_surrogate->protocol.prot = protocol_rfc6455;
	protocols_route(_surrogate);

	_qev_mem_pressure_return(_surrogate->qev_client._wbuff);
	qev_buffer_put0(&_surrogate->qev_client._wbuff);

	return TRUE;
}

static gboolean _network_http_single(void *nothing G_GNUC_UNUSED)
{
	gint err;
	gchar buff[2048];

	err = send(_http_sock,
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 16\n\n"
		"/qio/ping:1=null", 102, 0);
	if (err != 102) {
		PERROR("Failed to send on raw socket");
		return FALSE;
	}

	recv(_http_sock, buff, sizeof(buff), 0);

	return TRUE;
}

static gboolean _immediate_http_single(void *nothing G_GNUC_UNUSED)
{
	qev_buffer_clear(_buff);
	g_string_append(_buff,
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 16\n\n"
		"/qio/ping:1=null");
	_surrogate->protocol.prot = protocol_http;
	protocols_route(_surrogate);

	_qev_mem_pressure_return(_surrogate->qev_client._wbuff);
	qev_buffer_put0(&_surrogate->qev_client._wbuff);

	return TRUE;
}

static gboolean _network_http_double(void *nothing G_GNUC_UNUSED)
{
	gint err;
	gchar buff[2048];

	err = send(_http_sock,
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 33\n\n"
		"/qio/ping:1=null\n"
		"/qio/ping:1=null", 119, 0);
	if (err != 119) {
		PERROR("Failed to send on raw socket");
		return FALSE;
	}

	recv(_http_sock, buff, sizeof(buff), 0);

	return TRUE;
}

static gboolean _immediate_http_double(void *nothing G_GNUC_UNUSED)
{
	qev_buffer_clear(_buff);
	g_string_append(_buff,
		"POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1\n"
		"Content-Length: 33\n\n"
		"/qio/ping:1=null\n"
		"/qio/ping:1=null");
	_surrogate->protocol.prot = protocol_http;
	protocols_route(_surrogate);

	_qev_mem_pressure_return(_surrogate->qev_client._wbuff);
	qev_buffer_put0(&_surrogate->qev_client._wbuff);

	return TRUE;
}

static void _client(qev_fd_t *sock)
{
	gint err;
	gint len;
	struct sockaddr_un un;

	*sock = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT(*sock > 0, "Could not create socket");

	un.sun_family = AF_UNIX;
	g_strlcpy(un.sun_path, SOCK_PATH, sizeof(un.sun_path));

	len = sizeof(un.sun_family) + strlen(un.sun_path);
	err = connect(*sock, (struct sockaddr*)&un, len);
	if (err != 0) {
		PERROR("client()->connect()");
		FATAL("Could not connect");
	}

}

static void _init_raw_client()
{
	gchar buff[32];

	_client(&_raw_sock);
	ASSERT(send(_raw_sock, RAW_HANDSHAKE, strlen(RAW_HANDSHAKE), 0) == strlen(RAW_HANDSHAKE),
		"Raw handshake failed");
	ASSERT(recv(_raw_sock, buff, sizeof(buff), 0) == strlen(RAW_HANDSHAKE),
		"Raw handshake recv failed");
}

static void _init_rfc6455_client()
{
	gchar buff[1024];

	_client(&_rfc6455_sock);
	ASSERT(send(_rfc6455_sock, RFC6455_HEADERS, strlen(RFC6455_HEADERS), 0) == strlen(RFC6455_HEADERS),
		"RFC6455 handshake failed");
	ASSERT(recv(_rfc6455_sock, buff, sizeof(buff), 0) == strlen(RFC6455_HEADERS_RESPONSE),
		"RFC6455 handshake recv failed");
	ASSERT(send(_rfc6455_sock, RFC6455_QIO_HANDSHAKE, strlen(RFC6455_QIO_HANDSHAKE), 0) == strlen(RFC6455_QIO_HANDSHAKE),
		"RFC6455 QIO handshake failed");
	ASSERT(recv(_rfc6455_sock, buff, sizeof(buff), 0) == strlen(RFC6455_QIO_HANDSHAKE_RESPONSE),
		"RFC6455 QIO handshake recv failed");
}

static void _init_http_client()
{
	_client(&_http_sock);
}

int main()
{
	qev_bench_t *bench;
	gchar *argv[] = {
		"bench_routing",
		"--config-file=" CONFIG_FILE
	};

	ASSERT(g_file_set_contents(CONFIG_FILE, CONFIG, -1, NULL),
		"Could not setup config file");

	qev_pool_register_thread();
	qio_main(G_N_ELEMENTS(argv), argv);

	_buff = qev_buffer_get();
	_surrogate = qev_surrogate_new();
	_surrogate->protocol.handshaked = TRUE;
	_surrogate->qev_client.rbuff = _buff;

	_init_raw_client();
	_init_rfc6455_client();
	_init_http_client();

	bench = qev_bench_new("routing", "bench_routing.xml");
	qev_bench_fn_for(bench, "base_routing", _base_routing, NULL, 1000);
	qev_bench_fn_for(bench, "raw.single.network", _network_raw_single, NULL, 1000);
	qev_bench_fn_for(bench, "raw.single.immediate", _immediate_raw_single, NULL, 1000);
	qev_bench_fn_for(bench, "raw.double.network", _network_raw_double, NULL, 1000);
	qev_bench_fn_for(bench, "raw.double.immediate", _immediate_raw_double, NULL, 1000);
	qev_bench_fn_for(bench, "rfc6455.single.network", _network_rfc6455_single, NULL, 1000);
	qev_bench_fn_for(bench, "rfc6455.single.immediate", _immediate_rfc6455_single, NULL, 1000);
	qev_bench_fn_for(bench, "rfc6455.double.network", _network_rfc6455_double, NULL, 1000);
	qev_bench_fn_for(bench, "rfc6455.double.immediate", _immediate_rfc6455_double, NULL, 1000);
	qev_bench_fn_for(bench, "http.single.network", _network_http_single, NULL, 1000);
	qev_bench_fn_for(bench, "http.single.immediate", _immediate_http_single, NULL, 1000);
	qev_bench_fn_for(bench, "http.double.network", _network_http_double, NULL, 1000);
	qev_bench_fn_for(bench, "http.double.immediate", _immediate_http_double, NULL, 1000);
	qev_bench_free(bench);

	qev_close(_surrogate, 0);
	close(_raw_sock);
	close(_rfc6455_sock);
	close(_http_sock);

	unlink(CONFIG_FILE);

	qev_exit();

	return 0;
}
