#!/usr/bin/env python2.7
import gevent.monkey; gevent.monkey.patch_all()

import gevent
import socket
import ssl

clients = 0
sockets = []

HANDSHAKE = """GET /qio HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Origin: http://example.com
Sec-WebSocket-Version: 13

"""

def count():
	while True:
		print 'Clients: %d' % clients
		gevent.sleep(1)

gevent.spawn(count)

while True:
	try:
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		ss = ssl.wrap_socket(s)

		ss.connect(('localhost', 4433))
		ss.write(HANDSHAKE)
		ss.read()
		
		sockets.append(ss)
		
		clients += 1
	except:
		pass
	
	# gevent.sleep(.1)