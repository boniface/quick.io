from nose.tools import *

from . import get_socket, get_ws

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
"""

# A perfectly-formatted ready message from the server
READY = "\x81\05ready"

def test_handshake():
	"""Tests the rfc6455 handshake"""
	s = get_socket()
	
	s.send(HANDSHAKE)
	
	assert s.recv(4096).startswith(HANDSHAKE_RESPONSE)

def test_ready():
	"""Verify the "ready" message from the server"""
	s = get_socket()
	
	s.send(HANDSHAKE)
	
	data = ""
	for i in range(2):
		data += s.recv(4096)
		
		if READY in data:
			assert READY in data
			return
	
	assert False

def test_continuation():
	"""Test continuation frames"""
	assert True

def test_ping():
	"""Tests the ping control packet"""
	ws = get_ws()
	ws.send('test')