/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

void events_route(
	struct client *client,
	const gchar *event_path,
	const guint64 client_callback,
	const gchar *json)
{
	DEBUG("Got event: client=%p, event_path=%s, callback=%lu, json=%s",
			client, event_path, client_callback, json);
}

void events_init()
{

}
