/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

enum protocol_handles protocol_stomp_handles(struct client *client, void **data)
{
	return PROT_NO;
}

enum protocol_status protocol_stomp_handshake(
	struct client *client,
	void *data)
{
	return PROT_OK;
}

enum protocol_status protocol_stomp_route(struct client *client)
{
	return PROT_OK;
}
