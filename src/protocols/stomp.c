/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

enum protocol_handles protocol_stomp_handles(struct client *client G_GNUC_UNUSED, void **data G_GNUC_UNUSED)
{
	return PROT_NO;
}

enum protocol_status protocol_stomp_handshake(
	struct client *client G_GNUC_UNUSED,
	void *data G_GNUC_UNUSED)
{
	return PROT_OK;
}

enum protocol_status protocol_stomp_route(struct client *client G_GNUC_UNUSED)
{
	return PROT_OK;
}

GString* protocol_stomp_frame(const gchar *data G_GNUC_UNUSED, const guint64 len G_GNUC_UNUSED)
{
	return NULL;
}
