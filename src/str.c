/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 * @copyright is_utf8 2013 Palard Julien
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Modified from: https://github.com/JulienPalard/is_utf8
 */
gboolean qio_str_is_utf8(const guchar *s, const gsize len)
{
	gsize i = 0;
	gsize cont_bytes = 0;

	while (i < len) {
		if (s[i] <= 0x7F) {
			cont_bytes = 0;
		} else if (s[i] >= 0xC0 /*11000000*/ && s[i] <= 0xDF /*11011111*/) {
			cont_bytes = 1;
		} else if (s[i] >= 0xE0 /*11100000*/ && s[i] <= 0xEF /*11101111*/) {
			cont_bytes = 2;
		} else if (s[i] >= 0xF0 /*11110000*/ && s[i] <= 0xF4 /* RFC 3629 */) {
			cont_bytes = 3;
		} else{
			return FALSE;
		}

		i++;

		while (i < len && cont_bytes > 0 && s[i] >= 0x80 && s[i] <= 0xBF) {
			i++;
			cont_bytes--;
		}

		if (cont_bytes != 0) {
			return FALSE;
		}
	}

	return TRUE;
}
