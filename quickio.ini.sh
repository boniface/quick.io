#!/bin/bash
cat <<EOF
[quick-event]

#
# Size for buffers obtained from the pool (optimal: 2^n - 1).
#
# buffer-size = 4095

#
# Maximum number of times a buffer may be used before being recycled
#
# buffer-max-uses = 32768

#
# What this host is called. It's typically safe just to use the default.
# This is used for naming the host in nagios, graphite, etc.
#
# hostname =

#
# The number of threads to keep active for the job pool.
#
# job-threads = 8

#
# Where the server logs should be written
#
log-file = $LOG_FILE

#
# The maximum number of clients that are allowed to connect. Should be a power
# of 2.
#
max-clients = $MAX_CLIENTS

#
# How often stats should be flushed to their sinks (in seconds).
#
# stats-interval = 10

#
# Address of the graphite server
#
# sink-graphite-address =

#
# Port on which graphite's newline-based processor is running.
#
# sink-graphite-port = 2003

#
# Command that runs nsca_send, with all needed options
#
# sink-nagios-nsca-command =

#
# How many threads QEV should run to service clients
#
# threads = 4

#
# The number of milliseconds a client is closed after a timeout is requested
#
# timeout = 10000

#
# The total number of bytes that may be used for client read and write buffers
#
# userspace-buff-max = 536870912

#
# How fairly userspace bytes should be given to users (0-100)
#
# userspace-buff-fairness = 35

include = $INCLUDE

[quick-event-monitor]

#
# This section contains all stats that can be monitored.
#
# Values are formatted as:
#   1) crit<10, warn<20
#   2) crit>20, warn>10
#   3) crit>40
#   4) warn<50

#
# The number of HTTP handshakes coming in
#
# quickio.protocol.rfc6455.handshakes.http =

#
# The number of invalid QuickIO handshakes (after the HTTP handshake, if there
# wasn't a /qio/ohai sent).
#
# quickio.protocol.rfc6455.handshakes.qio_invalid =

#
# The number of valid QuickIO handshakes
#
# quickio.protocol.rfc6455.handshakes.qio =

#
# The number of HTTP requests that did not contain the correct HTTP upgrade
# headers.
#
# quickio.protocol.rfc6455.handshakes.http_invalid =

#
# When an event is received, how long it takes to route the event directly
# through the server.
#
# quickio.protocol.rfc6455.route.stddev =
# quickio.protocol.rfc6455.route.sum =
# quickio.protocol.rfc6455.route.min =
# quickio.protocol.rfc6455.route.count =
# quickio.protocol.rfc6455.route.mean =
# quickio.protocol.rfc6455.route.max =

#
# The number of handshakes over the raw protocol
#
# quickio.protocol.raw.handshakes =

#
# When an event is recieved, how long it takes to route it directly through
# the server.
#
# quickio.protocol.raw.route.stddev =
# quickio.protocol.raw.route.sum =
# quickio.protocol.raw.route.min =
# quickio.protocol.raw.route.count =
# quickio.protocol.raw.route.mean =
# quickio.protocol.raw.route.max =

#
# Number of idiots still having to use the flash fallback for websockets
#
# quickio.protocol.flash.handshakes =

#
# Number of new subs created as a response to client subscriptions.
#
# quickio.subs.added =

#
# Number of subs that lost all subscribers and were therefore deleted.
#
# quickio.subs.removed =

#
# Total number of subs on the server at this instant.
#
# quickio.subs.total =

#
# Number of clients that connected
#
# quickio.qev.clients.opens =

#
# Amount of data held in userspace buffers from clients
#
# quickio.qev.clients.userspace_bytes =

#
# The number of clients that timed out
#
# quickio.qev.clients.timeouts =

#
# Total number of coients connected.
#
# quickio.qev.clients.connected =

#
# Number of clients that closed
#
# quickio.qev.clients.closes =

#
# How many times the client lock spun in the interval.
# quickio.qev.lock-spins.client =

#
# How many times, for all r/w locks, the reader portion spun waiting
# for a writer to finish.
#
# quickio.qev.lock-spins.read =

#
# How many times, for all r/w locks, the writer portion spun waiting for
# readers to finish.
#
# quickio.qev.lock-spins.write =

#
# How many jobs were submitted to the job pool
#
# quickio.qev.jobs =

#
# The delay between when an event was received from the OS and when
# quick-event was able to act on it.
#
# quickio.qev.platform.dispatch_delay.stddev =
# quickio.qev.platform.dispatch_delay.sum =
# quickio.qev.platform.dispatch_delay.min =
# quickio.qev.platform.dispatch_delay.count =
# quickio.qev.platform.dispatch_delay.mean =
# quickio.qev.platform.dispatch_delay.max =

#
# How long it took the application to handle a read event from a client.
# quickio.qev.platform.route_read.stddev =
# quickio.qev.platform.route_read.sum =
# quickio.qev.platform.route_read.min =
# quickio.qev.platform.route_read.count =
# quickio.qev.platform.route_read.mean =
# quickio.qev.platform.route_read.max =

#
# When a client sends a /qio/on, this is incremented as the client has that
# sub (pending approval).
#
# quickio.clients.subs.added =

#
# The number of subscriptions that were removed from clients (because client
# send a /qio/off, client closed, subscription was rejected, etc).
#
# quickio.clients.subs.removed =

#
# Total number of subscriptions that all clients have.
#
# quickio.clients.subs.total =

#
# How many callbacks the server evicted to make space for new callbacks.
# If this is too high, then clients are sending callbacks too slowly, the
# server is sending them too quickly, or something or the sort is happening.
#
# quickio.clients.callbacks.evicted =

#
# How many valid callbacks were fired
#
# quickio.clients.callbacks.fired =

#
# How many callbacks are pending on the server
# quickio.clients.callbacks.total =

#
# How many callbacks were created
# quickio.clients.callbacks.created =

#
# The number of unique broadcasts sent out
#
# quickio.evs.broadcasts.unique =

#
# Every time a client receives a broadcast, this is incremented (so per-client,
# per-broadcast).
#
# quickio.evs.broadcasts.messages =

#
# The number of events received from clients (includes callbacks)
#
# quickio.evs.received =

#
# How many events were sent out (excludes broadcasts and callbacks)
#
# quickio.evs.sent =

#
# How many callbacks were sent out
#
# quickio.evs.callbacks_sent =

[quick.io]

#
# The public address of the server. This value MUST be set, or clients that
# rely on HTTP fallback WILL NOT be able to connect.
#
# This value should be a DNS-resolvable name, not an IP address.
#
public-address = $PUBLIC_ADDRESS

#
# The address the main server should listen on
# Use 0.0.0.0 to listen everywhere.
# Leave blank to listen on no plain text port.
#
bind-address = 0.0.0.0

#
# The SSL address to listen on.
# Use 0.0.0.0 to listen everywhere.
# Leave blank not to use SSL.
#
# bind-address-ssl = 0.0.0.0

# The port that the server processes will start listening on.
#
bind-port = $BIND_PORT
bind-port-ssl = $BIND_PORT_SSL

#
# For debugging and stuff, sometimes you want a Unix Domain socket.
# Put the path for it here if you need it.
#
bind-path = $BIND_PATH

#
# If flash should be supported (listen on port 843 for policy requests).
#
support-flash = $SUPPORT_FLASH

#
# The paths to the server's SSL certificate chain and private key.
#
# *-0 is typically used for RSA
# *-1 is typically used for ECDSA
#
# ssl-cert-path-0 =
# ssl-key-path-0 =
# ssl-cert-path-1 =
# ssl-key-path-1 =

#
# The number of threads used to pump out broadcasts to clients
#
# broadcast-threads = 2

#
# How long a callback should be allowed to live on the server before being
# killed (measured in seconds)
#
# clients-cb-max-age = 15

#
# Maximum number of subscriptions that may exist on the server.
#
# clients-max-subs = 4194304

#
# How fairly subscriptions are allocated to clients. 100 indicates that
# subscriptions should be completely evenly distributed amongst clients
# (MAX(1, num_subscriptions / num_clients)), whereas 0 is first-come,
# first-serve. A value in the middle balances the two extremes.
#
# clients-subs-fairness = 80

#
# How often periodic tasks should be polled (heartbeats, callback cleanup, etc).
# Measured in seconds.
#
# periodic-interval = 10

#
# Number of threads used to run periodic tasks.
#
# periodic-threads = 8

#
# The minimum size for subscription arrays. Should be a power of 2. Any changes
# at runtime are only applied to new subscriptions.
#
# sub-min-size = 8192

#
# The user the server should run as
#
user = $USER

[quick.io-apps]

#
# Instructs QuickIO to load the apps specified. Values are given as:
#
# app_name = /path/to/app
#
EOF
