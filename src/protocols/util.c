/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * HTTP header delimiters
 */
#define DELIM_COLON 0x01
#define DELIM_SPACE 0x02
#define DELIM_NEWLINE 0x04

static gchar _http_delims[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	DELIM_SPACE, DELIM_NEWLINE, 0x00, 0x00, DELIM_NEWLINE, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, DELIM_SPACE, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, DELIM_COLON,
};

inline static void _skip_past_delim(gchar **curr, guchar delims)
{
	while (**curr != '\0' && _http_delims[(guchar)**curr] & delims) {
		(*curr)++;
	}
}

static gchar* _skip(gchar **start, guchar delims)
{
	gchar *val = *start;
	gchar *curr = val;

	while (*curr != '\0' && !(_http_delims[(guchar)*curr] & delims)) {
		curr++;
	}

	if (*curr == '\0') {
		return NULL;
	}

	*curr = '\0';
	curr++;

	_skip_past_delim(&curr, delims);

	*start = curr;

	return val;
}

GHashTable* protocol_util_parse_headers(GString *buff)
{
	static __thread GHashTable *headers;

	gchar *head = buff->str;

	if (G_UNLIKELY(headers == NULL)) {
		headers = g_hash_table_new(qio_stri_hash, qio_stri_equal);
		qev_cleanup_thlocal(headers, (qev_free_fn)g_hash_table_unref);
	}

	g_hash_table_remove_all(headers);

	/*
	 * By this point, GET should already have been verified. So just skip
	 * the first line and continue with parsing
	 */
	_skip(&head, DELIM_NEWLINE);
	_skip_past_delim(&head, DELIM_SPACE);
	while (TRUE) {
		gchar *key = _skip(&head, DELIM_COLON | DELIM_SPACE);
		gchar *val = _skip(&head, DELIM_SPACE | DELIM_NEWLINE);
		if (key == NULL || val == NULL) {
			break;
		}
		g_hash_table_insert(headers, key, val);
	}

	return headers;
}
