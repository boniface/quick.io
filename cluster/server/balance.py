import math
import time

from config import config

class Balancer(object):
	def __init__(self, servers):
		self._servers = servers
		self._start = 0

	def _check_servers(self):
		total_clients = self._servers.num_clients(only_updated=True)

		# Don't rebalance if not enough clients are connected
		if total_clients < config.balance_min_clients:
			return

		should_have = math.ceil(float(total_clients) / self._servers.num_servers(only_updated=True))
		threshold = math.floor(should_have * config.balance_threshold)

		overloaded = []
		underloaded = []

		for server in self._servers.iter_updated():
			load = server.clients - should_have

			if load >= threshold:
				overloaded.append((server, load))
			elif load < 0:
				underloaded.append([server, abs(load)])

		# Don't bother if none are running hot
		if not overloaded:
			return

		# Save when we're starting this rebalance
		self._start = time.time()

		# Move clients to the most underloaded servers first
		underloaded = sorted(underloaded, cmp=lambda x,y: cmp(x[1], y[1]))

		for server_load in overloaded:
			server = server_load[0]
			left_to_move = server_load[1]
			move_to = []

			while left_to_move and underloaded:
				to = underloaded[0]

				# The number of clients that can fit onto this server
				move = min((left_to_move, to[1]))
				left_to_move -= move

				# Update how many clients this server can now hold, just in case we didn't fill it
				to[1] -= move

				# If the server is now balanced, don't send him anything else
				if to[1] == 0:
					underloaded.pop(0)

				move_to.append([to[0], move])

			server.balance_move(move_to)

	def check(self):
		# No rebalance is allowed to run when we're in the cooldown period
		if self._start + config.balance_cooldown >= time.time():
			return

		# Nothing to reblance if only 1 server
		if len(self._servers) > 1:
			self._check_servers()
