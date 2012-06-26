import socket
import time

from nose.tools import *

from . import get_socket


def test_bad_client():
	"""Send junk to the server and make sure server terminates connection"""
	s = get_socket()
	
	#make the client misbehave
	s.send("test")
	
	#wait for the connection to terminate
	time.sleep(1.1)
	
	with assert_raises(socket.error):
		s.send('t')
		s.send('t')