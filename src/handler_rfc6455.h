#pragma once
#include <libsoup/soup.h>

#include "socket.h"

gboolean rfc6455_handles(SoupMessageHeaders*);
gboolean rfc6455_handshake(client_t*, SoupMessageHeaders*);

/**
 * The return of the function must be free()'d
 */
char* rfc6455_prepare_frame(char*, int*);