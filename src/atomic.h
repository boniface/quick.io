/**
 * Some more atomic operations that aren't implemented alsewhere.
 * @file atomic.h
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
 * For reference counting: if it's possible for a reference to go to
 * 0 while trying to grab a reference, this is what you want.
 *
 * @return
 *     TRUE if the reference was incremented
 * @return
 *     FALSE if the reference was 0 and could not be incremented
 */
static inline gboolean atomic_inc_not_zero(guint *to)
{
	__sync_synchronize();
	guint val = *to;

	while (TRUE) {
		guint old;

		if (G_UNLIKELY(val == 0)) {
			break;
		}

		old = __sync_val_compare_and_swap(to, val, val + 1);
		if (G_LIKELY(old == val)) {
			break;
		}

		val = old;
	}

	return val > 0;
}
