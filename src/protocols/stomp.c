/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

void protocol_stomp_init()
{

}

enum protocol_handles protocol_stomp_handles(struct client *client G_GNUC_UNUSED, void **data G_GNUC_UNUSED)
{
	return PROT_NO;
}

enum protocol_status protocol_stomp_handshake(
	struct client *client G_GNUC_UNUSED,
	void *data G_GNUC_UNUSED)
{
	return PROT_FATAL;
}

enum protocol_status protocol_stomp_route(struct client *client G_GNUC_UNUSED)
{
	return PROT_FATAL;
}

void protocol_stomp_heartbeat(struct client *client G_GNUC_UNUSED, struct heartbeat *hb G_GNUC_UNUSED)
{

}

GString* protocol_stomp_frame(
	const gchar *ev_path G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t server_cb G_GNUC_UNUSED,
	const gchar *json G_GNUC_UNUSED)
{
	return NULL;
}
