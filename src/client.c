/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

gint32* client_sub_get(struct client *client G_GNUC_UNUSED)
{
	// @todo actually implement that heuristic :-P
	// @todo with expandable qev_lists + max subscribe stats/heuristic
	return g_slice_alloc(sizeof(gint32));
}

void client_sub_put(struct client *client G_GNUC_UNUSED, gint32 *idx)
{
	g_slice_free1(sizeof(*idx), idx);
}
