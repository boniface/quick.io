import fcntl
import os
import random
from select import select
import socket
from subprocess import Popen, PIPE
import sys
import time

import websocket

# The setup() and teardown() to be executed in every process
_multiprocess_can_split_ = True

port = None
server = None

# A socket connected to the running server
sock = None
ws = None

def get_socket():
	global sock
	
	if sock:
		sock.close()
		
	sock = socket.create_connection(('127.0.0.1', port))
	
	return sock

def get_ws():
	global ws
	
	if ws:
		ws.close()
	
	ws = websocket.create_connection('ws://127.0.0.1:%s/' % port)
	
	return ws

def setup():
	global port, server
	
	port = str(random.randint(5000, 15000))
	
	cmd = [
		'../../app/server',
		'--port=' + port,
		'--timeout=1'
	]
	
	server = Popen(cmd, bufsize=1, stdout=PIPE)
	fcntl.fcntl(server.stdout.fileno(), fcntl.F_SETFL, os.O_NONBLOCK)
	
	while True:
		try:
			if 'READY' in server.stdout.read():
				break
		except IOError:
			pass
		
		time.sleep(.1)

def teardown():
	global server, sock, ws
	
	if sock:
		sock.close()
		sock = None
	
	if ws:
		ws.close()
		ws = None
	
	server.kill()
	server = None