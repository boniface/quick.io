import socket
import time

from nose.tools import *

def test_bad_client():
	"""Send junk to the server and make sure server terminates connection"""
	from . import port
	
	s = socket.create_connection(('127.0.0.1', port))
	
	#make the client misbehave
	s.send("test")
	
	#wait for the connection to terminate
	time.sleep(1.1)
	
	with assert_raises(socket.error):
		#write to raise an error
		s.send('closed')
		s.send('closed')