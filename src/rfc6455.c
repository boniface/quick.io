#include "debug.h"
#include "rfc6455.h"

// From: http://tools.ietf.org/html/rfc6455#section-1.3
const char HASH_KEY[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const int HASH_KEY_LEN = sizeof(HASH_KEY) - 1; //-1 to kill the null terminator

// Headers used in rfc6455
const char VERSION_KEY[] = "Sec-WebSocket-Version";
const char CHALLENGE_KEY[] = "Sec-WebSocket-Key";

// Response headers
char HEADERS[] = "HTTP/1.1 101 Switching Protocols\r\n"
	"Upgrade: websocket\r\n"
	"Connection: Upgrade\r\n"
	"Access-Control-Allow-Origin: *\r\n"
	"Sec-WebSocket-Accept: %s\r\n"
	"Sec-WebSocket-Protocol: chat\r\n\r\n";
const int HEADERS_LEN = sizeof(HEADERS);

gboolean rfc6455_handles(SoupMessageHeaders *req_headers) {
	const char *id = soup_message_headers_get_one(req_headers, VERSION_KEY);
	return id != NULL && strncmp(id, "13", 2) == 0;
}

gboolean rfc6455_handshake(const int sock, SoupMessageHeaders *req_headers) {
	const char *key = soup_message_headers_get_one(req_headers, CHALLENGE_KEY);
	
	if (key == NULL) {
		return FALSE;
	}
	
	//build up the concated key, ready for hashing for the return header
	//-1 to kill the null terminator from HASH_KEY
	guchar out[strlen(key) + HASH_KEY_LEN];
	strcpy((char*)out, key);
	strcat((char*)out, HASH_KEY);
	
	gsize size = strlen((char*)out);
	GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA1);
	
	//do some hard-core hashing, then free it all
	g_checksum_update(sum, out, size);
	g_checksum_get_digest(sum, out, &size);
	gchar *b64 = g_base64_encode(out, size);
	g_checksum_free(sum);
	
	size = HEADERS_LEN + strlen(b64);
	char headers[size];
	
	snprintf(headers, size, HEADERS, b64);
	DEBUG(headers);
	write(sock, headers, strlen(headers));
	
	g_free(b64);
	
	return TRUE;
}