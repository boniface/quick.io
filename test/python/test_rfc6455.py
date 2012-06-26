from nose.tools import *

HANDSHAKE = """GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Origin: http://example.com
Sec-WebSocket-Protocol: chat
Sec-WebSocket-Version: 13

"""

HANDSHAKE_RESPONSE = """HTTP/1.1 101 Switching Protocols\r
Upgrade: websocket\r
Connection: Upgrade\r
Access-Control-Allow-Origin: *\r
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r
Sec-WebSocket-Protocol: chat\r
"""

# A perfectly-formatted ready message from the server
READY = "\x81\05ready"

def test_handshake():
	"""Tests the rfc6455 handshake"""
	from . import sock
	
	sock.send(HANDSHAKE)
	
	assert sock.recv(4096).startswith(HANDSHAKE_RESPONSE)

def test_ready():
	"""Tests the "ready" message from the server"""
	from . import sock
	
	sock.send(HANDSHAKE)
	
	data = sock.recv(4096)
	data += sock.recv(4096)
	
	assert READY in data