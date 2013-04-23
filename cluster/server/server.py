import json
import sys
import time

import gevent
from gevent.server import StreamServer

from config import config, keys as config_keys
from errors import QioException, QioInvalidHandshake
from qio import QioServer, QioServers

HANDSHAKE = 'QIO: hi'
WHO_ARE_YOU = 'whoareyou:'

# All of the QIO servers connected
qioservers = QioServers()

class ClusterServer(StreamServer):
	def __init__(self, *args, **kwargs):
		super(ClusterServer, self).__init__(*args, spawn=1000, **kwargs)
	
	def handshake(self, client):
		if client.readline() != HANDSHAKE:
			raise QioInvalidHandshake()
	
	def conn_new(self, client):
		client.send(WHO_ARE_YOU)
	
	def balance(self):
		""" Polls the cluster status to check if balancing is needed """
		
		last_balance = time.time()
		while True:
			qioservers.update()
			
			if time.time() - last_balance > config.balance_interval:
				last_balance = time.time()
				qioservers.balance()
			
			gevent.sleep(config.poll_interval)
	
	def read(self, client):
		""" Handles reading messages from a single connection """
		
		while True:
			msg = client.read_msg()
			handler = 'handle_%s' % msg[0]
			if hasattr(self, handler):
				data = json.loads(msg[1]) if msg[1] else None
				send_back = getattr(self, handler)(client, data)
				if send_back != None:
					client.send('%s' % json.dumps(send_back))
	
	def handle(self, socket, address):
		""" Handles a new connection """
		
		try:
			c = QioServer(socket, address)
			self.handshake(c)
			self.conn_new(c)
			self.read(c)
		except QioException as e:
			print 'Exception: ' + repr(e)
		except Exception as e:
			raise
		finally:
			c.close()
	
	def handle_iam(self, client, msg):
		qioservers.new_connection(client, msg['address'], msg['display_name'])
	
	def handle_stats(self, client, msg):
		client.update_stats(msg)
	
	def handle_balance_complete(self, client, msg):
		client.balance_complete(client)