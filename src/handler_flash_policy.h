/**
 * Handles sending the flash socket policy file back on flash connections.
 * You're welcome, Internet Explorer.
 *
 * @file handler_flash_policy.h
 */

#pragma once
#include "qio.h"

/**
 * What the client sends when it wants our policy.
 */
#define H_FLASH_POLICY_REQUEST "<policy-file-request/>"

/**
 * Tell the client that we accept all the things!
 */
#define H_FLASH_POLICY_RESPONSE \
	"<cross-domain-policy>" \
		"<allow-access-from domain=\"*\" to-ports=\"*\" />" \
	"</cross-domain-policy>"

/**
 * If, given the headers, we support this client.
 *
 * @param req The buffer that flash decides to send...it's stupid.
 *
 * @return If this handler can handle this client.
 */
gboolean h_flash_policy_handles(gchar *req);

/**
 * Handshake with the client.
 *
 * @param client The client to handshake with
 *
 * @return If the handshake succeeded.
 */
status_t h_flash_policy_handshake(client_t *client);

#ifdef TESTING
#include "../test/test_handler_flash_policy.h"
#endif