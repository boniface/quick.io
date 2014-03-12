/**
 * Implements some misc string utilities.
 * @file str.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once
#include "quickio.h"

/**
 * Check if a string is properly utf8 encoded. This is roughly 2x faster
 * than g_utf8_validate().
 *
 * @note
 *     From https://github.com/JulienPalard/is_utf8
 *
 * @param s
 *     The string to validate
 * @param len
 *     The length of the string
 *
 * @return
 *     If the string is encoded properly.
 */
gboolean qio_str_is_utf8(const guchar *s, const gsize len);
