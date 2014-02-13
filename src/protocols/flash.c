/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * What the client sends when it wants our policy.
 */
#define FLASH_POLICY_REQUEST "<policy-file-request/>"

/**
 * Tell the client that we accept all the things!
 */
#define FLASH_POLICY_RESPONSE \
	"<cross-domain-policy>" \
		"<allow-access-from domain=\"*\" to-ports=\"*\" />" \
	"</cross-domain-policy>"

static qev_stats_counter_t *_stat_handshakes;

void protocol_flash_init()
{
	_stat_handshakes = qev_stats_counter("protocol.flash", "handshakes", TRUE);
}

enum protocol_handles protocol_flash_handles(
	struct client *client,
	void **data G_GNUC_UNUSED)
{
	GString *rbuff = client->qev_client.rbuff;

	if (g_strcmp0(rbuff->str, FLASH_POLICY_REQUEST) == 0) {
		return PROT_YES;
	}

	if (*rbuff->str == '<') {
		return PROT_MAYBE;
	}

	return PROT_NO;
}

enum protocol_status protocol_flash_handshake(
	struct client *client,
	void *data G_GNUC_UNUSED)
{
	/*
	 * While I absolutely abhor closing a client in a handshake, it's pretty
	 * much what flash requires. So let's just make it happen.
	 * UGHHHHHH
	 */

	qev_write(client, FLASH_POLICY_RESPONSE, sizeof(FLASH_POLICY_RESPONSE) - 1);
	qev_close(client, QIO_CLOSE_DONE);

	qev_stats_counter_inc(_stat_handshakes);

	return PROT_FATAL;
}
