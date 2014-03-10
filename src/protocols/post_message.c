/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Return an uncached response: don't allow intermediaries that might lose
 * their route to the server to cache anything.
 */
#define RESPONSE \
	"HTTP/1.1 200 OK\r\n" \
	"Connection: close\r\n" \
	"Content-Type: text/html; charset=UTF-8\r\n" \
	HTTP_NOCACHE "\r\n" \
	"<!DOCTYPE html><head><script>window.parent.postMessage('QuickIO Online', '*');</script></head><body></body>"

static qev_stats_counter_t *_stat_hits;

void protocol_post_message_init()
{
	_stat_hits = qev_stats_counter("protocol.post_message", "hits", TRUE);
}

enum protocol_handles protocol_post_message_handles(struct client *client)
{
	GString *rbuff = client->qev_client.rbuff;

	if (g_str_has_prefix(rbuff->str, "GET /post_message")) {
		return PROT_YES;
	}

	return PROT_NO;
}

enum protocol_status protocol_post_message_handshake(struct client *client)
{
	qev_stats_counter_inc(_stat_hits);
	qev_buffer_clear(client->qev_client.rbuff);
	qev_write(client, RESPONSE, sizeof(RESPONSE) - 1);
	qev_close(client, QIO_CLOSE_DONE);
	return PROT_FATAL;
}
