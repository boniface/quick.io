import sys
import time

import gevent

import balance
from config import config
from errors import QioInvalidMessage

curr_id = 0

def get_id():
	global curr_id
	curr_id += 1
	return curr_id - 1

class QioSocket(object):
	def __init__(self, socket, address):
		self._address = address
		self._socket = socket
		self._reader = socket.makefile()
	
	def close(self):
		self._socket.close()
		self._reader.close()
	
	def readline(self):
		return self._reader.readline().strip()
	
	def read_msg(self):
		msg = self.readline()
		print msg
		if not msg:
			raise QioInvalidMessage()
		
		cmd = msg.strip().split(':', 1)
		if len(cmd) != 2:
			raise QioInvalidMessage()
		
		return cmd
	
	def send(self, cmd):
		self._socket.send("%s\n" % cmd)

class QioServer(QioSocket):
	is_updated = False
	
	_id = 0
	_parent = None
	
	CMD_STATS = 'sendstats:'
	CMD_BALANCE = 'balance:%d %s'
	
	def setup(self, parent, public_address, display_name):
		self.id = get_id()
		self.clients = 0
		self.display_name = display_name
		self.public_address = public_address
		self._parent = parent
	
	def close(self):
		""" The connection to the server was lost """
		
		super(QioServer, self).close()
		if self._parent:
			self._parent.server_disconnect(self)
	
	def update(self):
		""" Ask the server to send its stats back """
		self.is_updated = False
		self.send(self.CMD_STATS)
	
	def update_stats(self, stats):
		""" Response from the server: update the server's stats """
		
		self.is_updated = True
		self.clients = stats.get('clients', 0)
	
	def _balance_move(self, server, count):
		""" So that test cases can override balance operations """
		self.send(self.CMD_BALANCE % (count, server.public_address))
	
	def balance_move(self, servers):
		""" Tell the server to move a given number of users to the given addresses """
		
		# With the processes, bring them down to the right levels
		for s in servers:
			count = min((s[1], self.clients))
			self._balance_move(s[0], count)

class QioServers(object):
	_server_class = QioServer
	
	def __init__(self):
		self._servers = {}
		self._balance = balance.Balancer(self)
	
	def server_disconnect(self, server):
		self._servers.pop(server.public_address, None)
	
	def new_connection(self, server, public_address, display_name):
		server.setup(self, public_address, display_name)
		self._servers[public_address] = server
	
	def num_clients(self, only_updated=False):
		""" The number of clients in the entire cluster """
		clients = 0
		for s in self:
			if not only_updated or s.is_updated:
				clients += s.clients
		return clients
	
	def num_servers(self, only_updated=False):
		if only_updated:
			cnt = 0
			for s in self:
				if s.is_updated:
					cnt += 1
			return cnt
		else:
			return len(self)
	
	def update(self):
		""" Asks all the servers to send their current load stats """
		
		if not len(self):
			return
		
		for s in self:
			s.update()
		
		# Wait for all the servers to send their stats
		for i in range(5):
			gevent.sleep(1)
			all_updated = True
			for s in self:
				if not s.is_updated:
					all_updated = False
					break
			
			if all_updated:
				break
	
	def balance(self):
		""" Runs a balance check on the cluster """
		
		self._balance.check()
	
	def __iter__(self):
		for addr, s in self._servers.iteritems():
			yield s
	
	def iter_updated(self):
		for s in self:
			if s.is_updated:
				yield s
		
	def __len__(self):
		return len(self._servers)