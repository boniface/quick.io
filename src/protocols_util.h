/**
 * Support functions for various protocol needs.
 * @file util.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#pragma once
#include "quickio.h"

/**
 * For holding HTTP headers.
 */
struct protocol_headers {
	/**
	 * The number of headers parsed. This is set by protocol_util_headers().
	 */
	guint used;

	/**
	 * All of the known headers.
	 */
	struct {
		gchar *key;
		gchar *val;
	} header[16];
};

/**
 * From the given buffer, parse out HTTP headers, ignoring the first line
 * (which is typically the GET, POST, etc line).
 *
 * @note
 *     Values might have trailing white space, it's not removed for you.
 *
 * @param head
 *     The string to parse the headers out of.
 * @param[out] headers
 *     Where the headers should be placed.
 */
void protocol_util_headers(
	gchar *head,
	struct protocol_headers *headers);

/**
 * From the list of headers, get the specified header, ignoring case.
 *
 * @note
 *     Values might have trailing white space, it's not removed for you.
 *
 * @param headers
 *     All of the known headers.
 * @param key
 *     The header you're looking for.
 *
 * @return
 *     The value of the header; NULL if it doesn't exist.
 */
gchar* protocol_util_headers_get(
	const struct protocol_headers *headers,
	const gchar *key);
