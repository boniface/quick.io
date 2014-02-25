/**
 * Support functions for various protocol needs.
 * @file util.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once
#include "quickio.h"

/**
 * From the given buffer, parse out HTTP headers, ignoring the first line
 * (which is typically the GET, POST, etc line).
 *
 * @param buff
 *     The buffer to parse the headers out of.
 *
 * @return
 *     A thread-local hash table that MUST NOT be freed. Maps header to value
 *     in a case-insensitive manner. Keys become invalid once `buff` is altered.
 *     Any subsequent calls to this from the same thread return the same
 *     hash table, cleared out and populated with the headers.
 */
GHashTable* protocol_util_parse_headers(GString *buff);
