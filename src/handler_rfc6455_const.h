// From: http://tools.ietf.org/html/rfc6455#section-1.3
#define HASH_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define HASH_KEY_LEN sizeof(HASH_KEY) - 1 //-1 to kill the null terminator

// Headers used in rfc6455
#define VERSION_KEY "Sec-WebSocket-Version"
#define CHALLENGE_KEY "Sec-WebSocket-Key"

// Response headers
#define HEADERS "HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: %s\r\n" \
	"Sec-WebSocket-Protocol: chat\r\n\r\n"
#define HEADERS_LEN sizeof(HEADERS)

#define FIRST_BYTE 0b10000000
#define FIRST_BYTE_TEXT FIRST_BYTE | 0b00000001