#include "quickio.h"

#define CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890*&"
#define DECODE (CHARS CHARS CHARS CHARS)

static GString *_buff = NULL;
static GString *_buff_out = NULL;


static gchar _mask[4] = "abcd";

static union {
	gchar c[8];
	guint64 i64;
} _mask64;

static union {
	guint32 i64[2];
	__int128_t i128;
} _mask128;

static gboolean _base(void *nothing G_GNUC_UNUSED)
{
	guint64 i;

	for (i = 0; i < _buff->len; i++) {
		_buff_out->str[i] = _buff->str[i] ^ _mask[i & 3];
	}

	return TRUE;
}

static gboolean _64bit(void *nothing G_GNUC_UNUSED)
{
	guint64 i;
	guint64 max = _buff->len - (_buff->len & (sizeof(guint64) - 1));

	for (i = 0; i < max; i += sizeof(guint64)) {
		guint64 from = *((guint64*)(_buff->str + i));
		*((guint64*)(_buff_out->str + i)) = from ^ _mask64.i64;
	}

	return TRUE;
}

static gboolean _128bit(void *nothing G_GNUC_UNUSED)
{
	guint64 i;
	guint64 max = _buff->len - (_buff->len & (sizeof(__uint128_t) - 1));

	for (i = 0; i < max; i += sizeof(__uint128_t)) {
		__uint128_t from = *((__uint128_t*)(_buff->str + i));
		*((__uint128_t*)(_buff_out->str + i)) = from ^ _mask128.i128;
	}

	return TRUE;
}

int main()
{
	_buff = qev_buffer_get();
	_buff_out = qev_buffer_get();

	g_string_append(_buff, DECODE);
	qev_buffer_ensure(_buff_out, _buff->len);

	memcpy(_mask64.c, "abcdefgh", sizeof(_mask64.c));
	_mask128.i64[0] = _mask64.i64;
	_mask128.i64[1] = _mask64.i64;

	qev_bench_t *bench = qev_bench_new("ws_decode", "bench_ws_decode.xml");
	qev_bench_fn(bench, "base", _base, NULL, 2000000);
	qev_bench_fn(bench, "64bit", _64bit, NULL, 2000000);
	qev_bench_fn(bench, "128bit", _128bit, NULL, 2000000);
	qev_bench_free(bench);

	qev_buffer_put0(&_buff);
	qev_buffer_put0(&_buff_out);

	return 0;
}
