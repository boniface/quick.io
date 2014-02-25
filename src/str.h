/**
 * Provides a couple new string functions.
 * @file str.h
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
 * A case-insensitive hashing function, for hash tables.
 */
guint qio_stri_hash(const void *v);

/**
 * Case-insensitive string compare function, for hash tables.
 */
gboolean qio_stri_equal(const void *v1, const void *v2);
