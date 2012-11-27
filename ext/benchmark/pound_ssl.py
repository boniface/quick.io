#!/usr/bin/env python2.7
import gevent.monkey; gevent.monkey.patch_all()

import gevent
from multiprocessing import Process
import socket
import ssl

reconnects = 0

HANDSHAKE = """GET /qio HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Origin: http://example.com
Sec-WebSocket-Version: 13

"""

def reconnects_count():
	last = 0
	while True:
		print 'Reconnects: %d (+%d)' % (reconnects, reconnects - last)
		last = reconnects
		gevent.sleep(1)

gevent.spawn(reconnects_count)

def pound():
	global reconnects
	while True:
		try:
			s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			ss = ssl.wrap_socket(s)

			ss.connect(('localhost', 4433))
			ss.write(HANDSHAKE)
			ss.read()
			
			reconnects += 1
		except:
			pass

def run():
	gevent.joinall([gevent.spawn(pound) for _ in range(100)])

for _ in range(3):
	Process(target=run).start()
run()