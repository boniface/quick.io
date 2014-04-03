/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

struct _intervals {
	struct protocol_heartbeat hb;
	gint64 cb;
};

static void _foreach_cb(struct client *client, void *is_)
{
	struct _intervals *is = is_;

	protocols_heartbeat(client, &is->hb);
	client_cb_prune(client, is->cb);
}

void periodic_run()
{
	struct _intervals is = {
		.hb = protocols_heartbeat_get_intervals(),
		.cb = client_cb_prune_get_before(),
	};


	qev_foreach(_foreach_cb, cfg_periodic_threads, &is);
}

void periodic_init()
{
	qev_timer(periodic_run, cfg_periodic_interval, 0);
}
