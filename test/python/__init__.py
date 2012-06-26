import fcntl
import os
import random
from subprocess import Popen, PIPE
import sys
import time

port = None
server = None

def setup():
	global port, server
	
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

def teardown():
	global server
	server.kill()
	server = None