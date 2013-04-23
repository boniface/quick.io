import gevent.monkey; gevent.monkey.patch_all()

import ConfigParser
import os
import signal
import sys

import server

if len(sys.argv) != 2:
	print 'Error: only one argument should be given: the config file'
	sys.exit(1)

def run(opts):
	import config
	config.setup(opts)
	
	cluster_addr = (opts.get('cluster_host', '0.0.0.0'), int(opts.get('cluster_port', 9871)))
	s = server.ClusterServer(cluster_addr)
	s.start()
	
	print 'Waiting for cluster connections on %s:%d' % cluster_addr
	
	s.balance()

config = ConfigParser.SafeConfigParser()
config.read(sys.argv[1])
opts = dict(config.items('cluster'))
run(opts)