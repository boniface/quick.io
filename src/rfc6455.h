#include <glib.h>
#include <libsoup/soup.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

gboolean rfc6455_handles(SoupMessageHeaders*);
gboolean rfc6455_handshake(const int, SoupMessageHeaders*);