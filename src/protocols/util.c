/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
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

void protocol_util_headers(
	const GString *buff,
	struct protocol_headers *headers)
{
	guint i = 0;
	gchar *head = buff->str;

	/*
	 * By this point, GET should already have been verified. So just skip
	 * the first line and continue with parsing
	 */
	_skip(&head, DELIM_NEWLINE);
	while (i < G_N_ELEMENTS(headers->header)) {
		_skip_past_delim(&head, DELIM_SPACE);
		headers->header[i].key = _skip(&head, DELIM_COLON | DELIM_SPACE);
		headers->header[i].val = _skip(&head, DELIM_NEWLINE);
		if (headers->header[i].key == NULL || headers->header[i].val == NULL) {
			break;
		}
		i++;
	}

	headers->used = i;
}

gchar* protocol_util_headers_get(
	const struct protocol_headers *headers,
	const gchar *key)
{
	guint i;

	/*
	 * As stupid as this looks, it's faster in the best case, but obviously
	 * worse in the worst case. Considering that most connections are going
	 * to use case-sensitive headers, this is acceptable.
	 */
	for (i = 0; i < headers->used; i++) {
		if (strcmp(headers->header[i].key, key) == 0) {
			return headers->header[i].val;
		}
	}

	for (i = 0; i < headers->used; i++) {
		if (strcasecmp(headers->header[i].key, key) == 0) {
			return headers->header[i].val;
		}
	}

	return NULL;
}
