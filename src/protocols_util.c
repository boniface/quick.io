/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static gchar*_skip(char **start, const gchar *delims) {
	gchar *key;
	gchar *key_end;

	key = *start;
	key_end = key + strcspn(key, delims);
	*start = key_end + strspn(key_end, delims);

	*key_end = '\0';

	return key;
}

void protocol_util_headers(
	gchar *head,
	struct protocol_headers *headers)
{
	guint i = 0;

	/*
	 * By this point, GET should already have been verified. So just skip
	 * the first line and continue with parsing
	 */
	_skip(&head, "\r\n");
	while (i < G_N_ELEMENTS(headers->header)) {
		headers->header[i].key = _skip(&head, ": \t");
		headers->header[i].val = _skip(&head, "\r\n");
		if (*headers->header[i].key == '\0') {
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
