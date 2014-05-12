Client Architecture
*******************

Clients are typically very simple and provide a few simple functions for larger applications to communicate with the server.

Principles of Clients
=====================

Clients are built around a few, simple principles:

* If a client requests a callback, it will ALWAYS receive one.
* If the server requests a callback, it is polite to send one.
* Clients can be killed at any moment by the server, but they are allowed to reconnect and reestablish themselves.
* Clients can be told to move to a different server; they must follow this instruction.
* Clients are responsible for maintaining their own connection to the server.

Implementing a Client
=====================

The best way to get started implementing a client is to get familiar with an existing client. Either way, we're going to run through everything that a client needs to implement to be up-to-par with all the other clients.

Client Events
-------------

A client must handle the following events from the server:

==================== =============================
 Event                 Description
==================== =============================
/qio/heartbeat        Fired from the server every 60 seconds for some protocols (WebSocket, Raw). Typically, the client should just ignore the message, reset its last receive time, and move on. If the server requests a callback, however, the client MUST fire it as soon as possible.

/qio/callback/{id}    Fired when the server is sending a callback to the client; the id parameter is the callback id. This should call the registered callback function registered at this id.

/qio/move             Fired when the server is forcing the client to balance to another QuickIO server. The data in the event is the server to move to. This should be as transparent as a reconnect.

==================== =============================

The following events are special events that are fired to alert the client of changes in connection state. These events MUST NEVER be sent to the server in a /qio/on event.

==================== =============================
 Event                 Description
==================== =============================
/open                 Fire when a new connection to the server is established and the handshake has been completed.

/close                Fire whenever a connection to the server is lost or closed (even by a client), but only if /open has been fired.

/error                Fire when there is an error connecting to the server, when the connection is lost, or any other error you see fit. The client will only treat this as informational and use it for logging and debugging. Any data included in this event is platform-dependent, but do you best to be thorough.
==================== =============================

Client Functions
----------------

A client should implement the following functions:

==================== =============================
Function             Description
==================== =============================
(constructor)         Creates an object (or control structure) for handling all QuickIO requests.

on                    Subscribe to an event on the server. This MUST update all internal structures to reflect that the client is now subscribed to the event and will be until `off` is called.

one                   Subscribe to the event, receive one event, and unsubscribe

off(null)             Unsubscribe from an event on the server

off(cb)               Remove a single callback from a subscription, calling unsubscribe on the server if there are no more callbacks.

send                  Send an event to the server.

close                 Immediately close the connection to the server. This may be called in an /open callback, in a /close callback, or at any time by the client.

reconnect             For clients that exist in multi-threaded environments, this is used to trigger a connection after /open and /close have been subscribed to. It also drops any active connection and reconnects to the cluster immediately.

==================== =============================

Handling Sending Events
-----------------------

While connected to a server, the client MUST send events as quickly as possible to the server. It should perform the following tasks:

1. Clean the event path such that it only contains [\A-\Z\a-\z\0-\9_-/], starts with a slash, and has no trailing slash.
2. Serialize any data to JSON immediately, or simply send a JSON `null` if there is no data.
3. Assign the callback an ID from an unsigned 64bit integer that is atomically incremented such that the first ID issued is 1, second is 2, and so on, and store that ID and callback in some quick-lookup table for future reference.
4. Format the event as follows: <event path>:<callback ID, or 0 if no callback>=<serialized JSON string>, for something like: /event/path:123={"json": "data"}
5. Write the event to the underlying protocol and flush it out to the server

When not connected to the server, the client MUST queue up events until it reconnects. The following rules apply to queued events:

1. Callbacks that are being sent to the server MAY NEVER be queued, and -1 "disconnected" MUST be fired on them immediately since callbacks become invalid once the connection with the server has been lost.
2. Subscription events may never be queued (/qio/on may never be put into the queue). The state of subscriptions is always known, and can be managed on reconnect, so queuing them up just results in extra memory usage.
3. All other events MUST be persisted until connected
4. Any data to be sent with the event MUST be serialized into a string before being put into the queue so that the object that represents it maybe mutated after the call.
5. Callback IDs may only be issued when connected, so just save a reference to the callback until connected.

Connecting to the Cluster
-------------------------

Each QuickIO cluster lives behind a single public address, typically a single DNS A record that points to all the servers. Each server, in turn, has a public address that can be requested at the event path /qio/hostname. To connect to a cluster, a client should pick one of the A records at random and try connecting; on failure, it may issue another DNS request for updated hosts or try the next one in its list. Each language has its own ways of doing this, and it's usually best to let the socket library try to figure everything out. If you are, on the other hand, using bare sockets in C or the standard library in Java, you're going to have to do this part yourself. Other languages seem to do it right.

Since many clients in the wild are behind proxies and "smart" HTTP firewalls that don't yet support WebSocket, it's necessary to support HTTP long polling in each client. The client must first attempt to establish a WebSocket connection with the cluster, and if that fails for any reason besides an unreachable network or similar condition, it must immediately fall back to HTTP long polling. If it can establish a connection with the server over HTTP, it must continue using HTTP long polling until either the client or server terminates the connection. Once terminated, if the client is going to attempt to reconnect, it must first attempt WebSocket again, just in case the client changes networks, and the new network supports WebSocket.

WebSocket Handshakes
^^^^^^^^^^^^^^^^^^^^

QuickIO speaks WebSocket as described in `RFC6455 <http://tools.ietf.org/html/rfc6455>`_. Your client MUST implement the handshake and framing parts of the spec in order to communicate properly with a QuickIO server. QuickIO does NOT implement binary, continuation, ping, or pong frames, so feel free to ignore those (though bear in mind that ping and pong might be implemented in the future if web browsers ever build a standard API for using them).

After going through the standard RFC6455 handshake and upgrade, it is necessary to send the QuickIO handshake as many proxies let all the proper headers through, so it appears that the upgrade succeeds. Without this handshake, it's impossible to determine if the client `can really` speak WebSocket. The handshake is very simple: using all RFC6455 framing conventions, simply send the following message::

	/qio/ohai

The server will immediately respond with "/qio/ohai", too, and at this point, all handshakes have finished, and the connection is considered opened. At this point, the client MUST fire an /open event.

HTTP Long Polling
^^^^^^^^^^^^^^^^^

If any part of the WebSocket handshake fails, aside from network issues, as mentioned, the client MUST try connection via HTTP. Since many HTTP clients and proxies will attempt to balance HTTP requests over many sockets, it's possible that they will attempt to send long polling requests to different QuickIO servers in the cluster as they are round-robined behind a single DNS record. In order to ensure that the client is speaking to the same server throughout the lifetime of its HTTP session, it must conduct the opening handshake as follows:

1. Issue an HTTP POST to the cluster address, for example quickio.example.com, with the body "/qio/hostname:1=null" and the following URL parameters:
	* sid: the session ID used to identify the client. This MUST be a 128 bit number represented as 32 hexadecimal characters. It can be easily generated with any UUID library that supports UUID type 4 by taking the UUID string and replacing all "-" with "".
	* connect: this value MUST be the string "true" to indicate that the client is new. This parameter only ever appears in the initial request.
2. If the server does not respond with a 200, the connection must be failed immediately, and the client must try reconnecting again.
3. If the server responded with a 200, the body of the response will contain the server's public address, formatted as an event response. All further communication with the server must use this address.
4. After receiving any response from the server, including after the handshake, the client must schedule another POST request to run after 0-2000 milliseconds, chose at random (typically Math.random() * 2000). This is necessary to make sure that there isn't a stampeding herd attacking the server after HTTP heartbeats.
5. A client may, at any time, issue a new POST request with newline-separated events, provided that the request is only sent after 0-2000 milliseconds, in the same way that requests are queued up after a poll finishes. Bodies of requests shall contain numerous newline-separated events, and they must be gathered into as few requests as possible (sending 2 requests at the same time is prohibited, the events must be gathered into a single request). Sending a request 0-2000 milliseconds after a previous request is acceptable as there is no way to preempt when events will be fired. The server will respond by completing any other requests from the client and holding onto the newest request until data is ready (the new request will become the long-polling request).
6. If the client, at any point, sees a non 200 response, it must fail the connection immediately and fire any /close event, as appropriate.

Since this can be a bit complicated, let's look at a sample HTTP conversation between the client and the server, unnecessary headers omitted:

.. code-block:: http

	POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9&connect=true HTTP/1.1
	Host: quickio.example.com
	Content-Type: text/plain
	Content-Length: 21

	/qio/hostname:1=null

.. code-block:: http

	HTTP/1.1 200 OK
	Content-Type: text/plain
	Content-Length: 62

	/qio/callback/1:0={"code":200,"data":"quickio129.example.com"}

.. code-block:: http

	POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9 HTTP/1.1
	Host: quickio129.example.com
	Content-Type: text/plain
	Content-Length: 0

.. code-block:: http

	POST /?sid=16a0dd9a4e554a9f94520c8bfa59e1b9 HTTP/1.1
	Host: quickio129.example.com
	Content-Type: text/plain
	Content-Length: 16

	/qio/ping:1=null

.. code-block:: http

	HTTP/1.1 200 OK
	Content-Type: text/plain
	Content-Length: 42

	/qio/callback/1:0={"code":200,"data":null}

At this point, there is 1 HTTP request pending at the server, and that will be used to send any new events back to the client. Once this request finishes, the client will send a new request after (Math.random() * 2000) milliseconds.

Connection Persistence
----------------------

The client MUST do its best to maintain a connection to the QuickIO cluster until it is told to stop. Between connection failures, it must backoff using the following algorithm, such that backoff increases between successive failures.

.. code-block:: javascript

	onDisconnect(function() {
		backoff = Math.min(25600, backoff * 2);
		reconnectAfter(backoff);
	});

	onConnectAndSuccessfulHandshake(function() {
		backoff = 100;
	});

Once a connection with the server has been re-established, and once all handshakes have been completed the client must do the following, `in this order`:

1. Go through all callbacks that exist and fire a -1 "disconnected" error on them, being sure not to trample any new callbacks that come in as a result of triggering the old ones.
2. Subscribe to all events that exist in the client by sending a new /qio/on event to the server for each event, listening for any errors while subscribing.
3. Go through any events that were accumulated while not connected and send them to the server.
4. Fire /open and reset the backoff timer

Handling Events from the Server
-------------------------------

During the lifetime of the client, it will receive a ton of events from the server. Handling them is really rather simple.

1. When an event comes through, break it into its parts (event path, cb id, and JSON data), and parse any JSON into an object.
2. If the server requested a callback, create a callback that can be passed to the event handler and called from there.
3. If the event path is a QuickIO event from the server (/qio/heartbeat, /qio/move, /qio/callback, etc), handle them as needed.
4. Otherwise, lookup the event in the subscriptions table and call each callback with the JSON data.

Callback Association with Connections
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Since callbacks are very tightly linked to the server and session they have on the server, they must be explicitly tied to a given connection, typically by giving the connection and ID and associating the callback with that ID. If at any point the application attempts to trigger a callback that is not tied to the current connection, the client must respond to the callback immediately with -1 "disconnected".

Client Heartbeats
-----------------

Your average user will most likely be someone connecting to your service from behind some NAT gateway: this presents some interesting problems. In order to make sure the client maintains a connection to the server at all times, even when there is no activity, application-level heartbeats are employed. Each protocol has a different way of handling heartbeats.

WebSocket Heartbeats
^^^^^^^^^^^^^^^^^^^^

By default, a client will receive at least one message every 60 seconds, be it in the form of a callback, broadcast event, or heartbeat.

Heartbeats are implemented such that, if a client hasn't been sent a message in around 60 seconds (this is variable to within -10 seconds, but a client will never go more than 60 seconds without an event from the server), it will receive a heartbeat.

The best method for implementing a heartbeat is:

1. The client should listen for a `/qio/heartbeat` event (but not subscribe).
2. Once the connection is open, the client should set a timer for 65 seconds, listening for messages from the server.
3. Every time the client receives a message from the server, it should reset its timer.
4. If there has been no activity within 65 seconds, assume the server is dead and reconnect.

HTTP Heartbeats
^^^^^^^^^^^^^^^

HTTP requests will be responded to once every 50 seconds in order to ensure that nasty proxies don't just time them out. The client must, however, set a timeout of 60 seconds on each request, just to give any response time to traverse the network, and some time to be slow. If the request ever times out, the client must assume the connection has been lost and reconnect.
