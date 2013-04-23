class Config(object):
	poll_interval = 5
	
	balance_cooldown = 120
	balance_interval = 120
	balance_min_clients = 5000
	
	_balance_threshold = .2
	
	@property
	def balance_threshold(self):
		return self._balance_threshold
	
	@balance_threshold.setter
	def balance_threshold(self, val):
		self._balance_threshold = float(val) / 100

def setup(opts):
	for k in keys:
		if k in opts:
			setattr(config, k, int(opts[k]))

config = Config()
keys = filter(lambda k: not k.startswith('_'), dir(config))