from datetime import datetime, timedelta
import math
import json
from nose.tools import *
import os
import signal
import subprocess
import time
import unittest

import gevent
import gevent.pool
from gevent import socket

from config import config
from qio import QioServer, QioServers, QioSocket

def push_to_threshold(servers):
	def thresh():
		should_have = sum([s.clients for s in servers]) / len(servers)
		return should_have, int(round(should_have * config.balance_threshold))
	
	distance, t = thresh()
	servers[0].clients += t
	
	while True:
		should_have, t = thresh()
		load = servers[0].clients - should_have
		
		if load == t:
			return
		elif load > t:
			servers[0].clients -= distance
		else:
			servers[0].clients += distance
		
		if distance > 2:
			distance /= 2

class QioServerTester(QioServer):
	_is_balancing = False
	
	def __init__(self):
		pass
			
	def balance_is_running(self):
		return self._is_balancing
	
	def balance_complete(self):
		self._is_balancing = False
	
	def _balance_move(self, server, count):
		self._is_balancing = True
		self.clients -= count
		server.clients += count

class QioServersTester(QioServers):
	def balance_complete(self):
		for s in self:
			s.balance_complete()
	
	def balance_is_running(self):
		for addr, s in self._servers.iteritems():
			if s.balance_is_running():
				return True
		return False

class TestSingleServer(unittest.TestCase):
	def setUp(self):
		self.qservers = qs = QioServersTester()
		
		p = QioServerTester()
		qs.new_connection(p, 'test1', 'test1')
		p.clients = 5000
		p.is_updated = True
		
	def test(self):
		self.qservers._balance.check()
		assert_false(self.qservers.balance_is_running())
		
		for s in self.qservers:
			assert_equal(s.clients, 5000)

class TestTwoServers(unittest.TestCase):
	def setUp(self):
		self.qservers = qs = QioServersTester()
		self.servers = []
		
		p = QioServerTester()
		qs.new_connection(p, 'test1', 'test1')
		p.clients = 5000
		p.is_updated = True
		self.servers.append(p)
		
		p = QioServerTester()
		qs.new_connection(p, 'test2', 'test2')
		p.clients = 5000
		p.is_updated = True
		self.servers.append(p)
	
	def test_sane(self):
		self.qservers._balance.check()
		assert_false(self.qservers.balance_is_running())
		
		for s in self.qservers:
			assert_equal(s.clients, 5000)
	
	def test_min_clients(self):
		self.servers[0].clients = 3000
		self.servers[1].clients = 1000
		
		self.qservers._balance.check()
		assert_false(self.qservers.balance_is_running())
	
	def test_unbalanced_0(self):
		self.servers[0].clients *= 2
		start_clients = self.qservers.num_clients()
		
		self.qservers._balance.check()
		
		assert_true(self.qservers.balance_is_running())
		assert_equal(start_clients, self.qservers.num_clients())
		
		should_be = self.servers[0].clients
		for p in self.servers:
			assert_almost_equal(p.clients, should_be, delta=1)
	
	def test_unbalanced_1(self):
		self.servers[0].clients *= 2
		start_clients = self.qservers.num_clients()
		self.qservers._balance.check()
		
		assert_true(self.qservers.balance_is_running())
		assert_equal(start_clients, self.qservers.num_clients())
		
		should_be = self.servers[0].clients
		for p in self.servers:
			assert_almost_equal(p.clients, should_be, delta=1)
	
	def test_unbalanced_just_under(self):
		push_to_threshold(self.servers)
		self.servers[0].clients -= 1
		
		under = self.servers[0].clients
		
		self.qservers._balance.check()
		
		assert_equal(self.servers[0].clients, under)
		assert_equal(self.servers[1].clients, 5000)
	
	def test_unbalanced_just_over(self):
		push_to_threshold(self.servers)
		
		self.qservers._balance.check()
		
		should_be = self.servers[0].clients
		for p in self.servers:
			assert_almost_equal(p.clients, should_be, delta=1)

class TestThreeServers(unittest.TestCase):
	def setUp(self):
		self.qservers = qs = QioServersTester()
		self.servers = []
		
		p = QioServerTester()
		qs.new_connection(p, 'test1', 'test1')
		p.clients = 5000
		p.is_updated = True
		self.servers.append(p)
		
		p = QioServerTester()
		qs.new_connection(p, 'test2', 'test2')
		p.clients = 5000
		p.is_updated = True
		self.servers.append(p)
		
		p = QioServerTester()
		qs.new_connection(p, 'test3', 'test3')
		p.clients = 5000
		p.is_updated = True
		self.servers.append(p)
	
	def test_sane(self):
		self.qservers._balance.check()
		assert_false(self.qservers.balance_is_running())
		
		for s in self.qservers:
			assert_equal(s.clients, 5000)
	
	def test_unbalanced_0(self):
		self.servers[0].clients *= 3
		start_clients = self.qservers.num_clients()
		
		self.qservers._balance.check()
		
		assert_true(self.qservers.balance_is_running())
		assert_equal(start_clients, self.qservers.num_clients())
		
		should_be = self.servers[0].clients
		for p in self.servers:
			assert_almost_equal(p.clients, should_be, delta=2)
	
	def test_unbalanced_just_under(self):
		push_to_threshold(self.servers)
		start_clients = self.qservers.num_clients()
		
		self.qservers._balance.check()
		
		assert_false(self.qservers.balance_is_running())
		assert_equal(start_clients, self.qservers.num_clients())
	
	def test_unbalanced_just_over(self):
		push_to_threshold(self.servers)
		self.servers[0].clients += 1
		start_clients = self.qservers.num_clients()
		
		self.qservers._balance.check()
		
		assert_true(self.qservers.balance_is_running())
		assert_equal(start_clients, self.qservers.num_clients())
		
		should_be = self.servers[0].clients
		for p in self.servers:
			assert_almost_equal(p.clients, should_be, delta=1)
	
	def test_cooldown(self):
		self.servers[0].clients *= 3
		self.qservers._balance.check()
		
		assert_true(self.qservers.balance_is_running())
		
		self.qservers.balance_complete()
		assert_false(self.qservers.balance_is_running())
		
		self.servers[0].clients *= 3
		self.qservers._balance.check()
		assert_false(self.qservers.balance_is_running())
		
	def test_cooldown_expire(self):
		self.servers[0].clients *= 3
		self.qservers._balance.check()
		
		assert_true(self.qservers.balance_is_running())
		self.qservers.balance_complete()
		
		self.servers[0].clients *= 3
		self.qservers._balance._start = 0
		self.qservers._balance.check()
		assert_true(self.qservers.balance_is_running())

class TestFourServers(unittest.TestCase):
	def setUp(self):
		self.qservers = qs = QioServersTester()
		self.servers = []
		
		p = QioServerTester()
		qs.new_connection(p, 'test1', 'test1')
		p.clients = 100
		p.is_updated = True
		self.servers.append(p)
		
		p = QioServerTester()
		qs.new_connection(p, 'test2', 'test2')
		p.clients = 100
		p.is_updated = True
		self.servers.append(p)
		
		p = QioServerTester()
		qs.new_connection(p, 'test3', 'test3')
		p.clients = 100
		p.is_updated = True
		self.servers.append(p)
		
		p = QioServerTester()
		qs.new_connection(p, 'test4', 'test4')
		p.clients = 100
		p.is_updated = True
		self.servers.append(p)
	
	def test_sane(self):
		self.qservers._balance.check()
		assert_false(self.qservers.balance_is_running())
		
		for s in self.qservers:
			assert_equal(s.clients, 100)
	
	def test_unbalanced(self):
		old_min = config.balance_min_clients
		config.balance_min_clients = 5
		self.servers[0].clients = 9
		self.servers[1].clients = 0
		self.servers[2].clients = 0
		self.servers[3].clients = 0
		
		self.qservers._balance.check()
		assert_true(self.qservers.balance_is_running())
		
		assert_equal(self.servers[0].clients, 3)
		assert_equal(self.servers[1].clients, 3)
		assert_equal(self.servers[2].clients, 3)
		assert_equal(self.servers[3].clients, 0)
		
		config.balance_min_clients = old_min
	
class Integration(unittest.TestCase):
	def setUp(self):
		self.proc = subprocess.Popen(
			['python', 'cluster/', 'test.ini'],
			stdout=subprocess.PIPE,
			env=dict(os.environ, PYTHONUNBUFFERED='true'),
		)
		
		self.proc.stdout.readline()
		
	def tearDown(self):
		os.kill(self.proc.pid, signal.SIGKILL)
	
	def test_single_server(self):
		cs = [Server('0')]
		p = gevent.pool.Pool(len(cs))
		
		cs[0].clients = 1000
		
		for c in cs:
			p.apply_async(c.run, [cs])
		
		gevent.sleep(2)
		
		for _ in range(2):
			for c in cs:
				assert_equal(c.clients, 1000)
			gevent.sleep(2)
		
		p.kill()
	
	def test_two_servers(self):
		cs = [Server('0'), Server('1')]
		p = gevent.pool.Pool(len(cs))
		
		cs[0].clients = 1000
		cs[1].clients = 1000
		
		for c in cs:
			p.apply_async(c.run, [cs])
		
		gevent.sleep(2)
		
		for _ in range(2):
			for c in cs:
				assert_equal(c.clients, 1000)
			gevent.sleep(2)
		
		p.kill()
	
	def test_two_unbalanced(self):
		cs = [Server('0'), Server('1')]
		p = gevent.pool.Pool(len(cs))
		
		cs[0].clients = 1000
		cs[1].clients = 2000
		
		for c in cs:
			p.apply_async(c.run, [cs])
		
		gevent.sleep(2)
		
		for _ in range(2):
			for c in cs:
				assert_equal(c.clients, 1500)
			gevent.sleep(2)
		
		p.kill()
	
	def test_four_unbalanced_0(self):
		cs = [Server('0'), Server('1'), Server('2'), Server('3')]
		p = gevent.pool.Pool(len(cs))
		
		cs[0].clients = 8
		
		for c in cs:
			p.apply_async(c.run, [cs])
		
		gevent.sleep(2)
		
		for _ in range(2):
			for c in cs:
				assert_equal(c.clients, 2)
			gevent.sleep(2)
		
		p.kill()
	
	def test_four_unbalanced_1(self):
		cs = [Server('0'), Server('1'), Server('2'), Server('3')]
		p = gevent.pool.Pool(len(cs))
		
		cs[0].clients = 9
		
		for c in cs:
			p.apply_async(c.run, [cs])
		
		gevent.sleep(2)
		
		for i in range(2):
			# Two rebalances happen (this is a stupid, greedy algorithm):
			#   The first gives 3 to each server, leaving 1 with 0
			#   The second moves some to the empty server
			for c in cs:
				assert_greater_equal(c.clients, i)
			gevent.sleep(2)
		
		p.kill()

class Server(QioSocket):
	def __init__(self, address):
		self.clients = 0
		s = socket.create_connection(('127.0.0.1', '9871'))
		super(Server, self).__init__(s, address)
	
	def run(self, servers):
		self.servers = servers
		self.send('QIO: hi')
		while True:
			msg = self.read_msg()
			handler = 'handle_%s' % msg[0]
			if hasattr(self, handler):
				try:
					data = json.loads(msg[1]) if msg[1] else None
				except ValueError:
					data = msg[1]
				
				getattr(self, handler)(data)
	
	def handle_whoareyou(self, data):
		self.send('iam:%s' % json.dumps({
			'address': self._address,
			'display_name': self._address,
		}))
	
	def handle_sendstats(self, data):
		self.send('stats:%s' % json.dumps({
			'clients': self.clients,
		}))
	
	def handle_balance(self, data):
		to = data.split(' ')
		self.servers[int(to[1])].clients += int(to[0])
		self.clients -= int(to[0])