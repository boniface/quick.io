import socket
import time

from nose.tools import *

def test_bad_client():
	"""Send junk to the server and make sure server terminates connection"""
	from . import sock
	
	#make the client misbehave
	sock.send("test")
	
	#wait for the connection to terminate
	time.sleep(1.1)
	
	with assert_raises(socket.error):
		#write to raise an error
		sock.send('closed')
		sock.send('closed')