/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

gboolean qev_on_before_run()
{
	evs_init();
	protocols_init();

	/*
	 * Initialize last: don't let apps take over internal events.
	 */
	apps_init();

	return TRUE;
}

void qev_on_open(struct client *client, const gchar *ip G_GNUC_UNUSED)
{
	qev_timeout(client, &client->timeout);
}

void qev_on_close(struct client *client, guint reason)
{
	qev_timeout_clear(&client->timeout);
	protocols_closed(client, reason);
	evs_client_close(client);
}

void qev_on_read(struct client *client)
{
	protocols_route(client);
}

void qev_on_udp(
	const gchar *msg G_GNUC_UNUSED,
	const guint len G_GNUC_UNUSED,
	const gchar *ip G_GNUC_UNUSED)
{
	// UDP is unused
}

void qev_on_tick()
{
	evs_broadcast_tick();
}

void qev_on_exit()
{

}

struct client* qev_client_new() {
	struct client *client = g_slice_alloc0(sizeof(struct client));

	client->subs = g_hash_table_new(NULL, NULL);

	return client;
}

void qev_client_free(struct client *client) {
	g_hash_table_unref(client->subs);
	g_slice_free1(sizeof(*client), client);
}
