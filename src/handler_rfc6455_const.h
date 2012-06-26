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

// Opcodes that can be used
#define OP_CONTINUATION 0b00000000
#define OP_TEXT 0b00000001
#define OP_BINARY 0b00000010
#define OP_PING 0b00001001
#define OP_PONG 0b00001010

#define MASK_BYTE 0b10000000
#define FIRST_BYTE_TEXT MASK_BYTE | OP_TEXT