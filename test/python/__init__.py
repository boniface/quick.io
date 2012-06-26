import fcntl
import os
import random
import socket
from subprocess import Popen, PIPE
import sys
import time

# The setup() and teardown() to be executed in every process
_multiprocess_can_split_ = True

server = None

# A socket connected to the running server
sock = None

def setup():
	global server, sock
	
	port = str(random.randint(5000, 15000))
	
	cmd = [
		'../../src/server',
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
	
	sock = socket.create_connection(('127.0.0.1', port))

def teardown():
	global server, sock
	sock.close()
	server.kill()
	server = None