The Run Down
************

This page contains the quick-and-dirty introduction to Quick.IO. This is a high-level overview, shedding the weight of code descriptions, algorithms, and other fun stuff, and instead using JavaScript examples to highlight points.

The Event Model
===============

Everything that the server sends and receives is treated as an event. An event can be something as simple as sending server time to a client to anything as complex as user authentication, handshakes, and callbacks.

Events take the form of `/some/path/to/an/event` such that the they are namespaced, allowing multiple different applications to work independently of each other on the same server. With just this simple event representation, the following becomes immediately possible:

.. code-block:: javascript
	
	qio.send('/app/login', {session_id: '1234abcd'}, function(data, cb) {
		if (!data.ok) {
			alert('Login failed :(');
		}
		
		qio.on('/app/update', function(data) {
			alert('The app was updated on: ' + data.date);
		});
		
		cb({iam: 'Andrew'}, function(data) {
			alert('The server says his name is: ' + data.iam);
		});
	});

The actual data sent in this exchange would look something like the following::
	
	/app/login:1:json={session_id: '1234abcd'}
	/qio/callback/1:1:json={ok: true}
	/qio/sub:0:plain=/app/update
	/qio/callback/1:2:json={iam: 'Andrew'}
	/qio/callback/2:0:json={iam: 'Howard'}
	/app/update:0:json={date: 'Jan 1, 1970'}

Event Format
------------

Data sent to and from the server is composed of 4 different parts::
	
	event_path:callback_id:data_type=data

1. event_path: just the path of the event, in its full glory
2. callback_id: an integer, greater than 0 (0 indicating no callback)
3. data_type: json, plain
4. data: the string representation of the data

Server Events
-------------

The server registers a few utility event handlers to make writing your application easier.

.. _server_events:

==================== =============================== ========= ==========================================
Event                Data                            Data Type Description
==================== =============================== ========= ==========================================
/qio/sub             The event path to subscribe     Plain     Subscribes the client to the event.
                     the client to.

/qio/unsub           The event path to unsubscribe   Plain     Removes a subscription from the client.
                     the client from.

/qio/ping            Anything                        Any       Any data sent to the server will be
                                                               returned as the data in the callback.

/qio/noop            Does nothing.                   None      No data will be returned in the
                                                               callback. 

/qio/callback        The data the client/server sent Any       The callback mechanism for the server,
                     along with the callback.                  attached to a variable-child event.
==================== =============================== ========= ==========================================

Event Subscriptions
===================

Events can be sent back and forth between client and server on a one-to-one basis, but the real fun comes in broadcasting. When you have hundreds of thousands of clients that need to get the exact same message, you want to use subscriptions and broadcasting. In the example earlier, the client did:

.. code-block:: javascript
	
	...snip...
	qio.on('/app/update', function(data) {
		alert('The app was updated on: ' + data.date);
	});
	...snip...

This registered an event listener on the server, waiting for the event `/app/update`. On the server, a list of clients subscriptions is maintained, so when you want to send an event to tons of waiting clients, send away!

.. tip:: Client subscriptions can be validated, so it's possible to create authenticated, private subscriptions.
.. note:: Broadcast events are the only event type that cannot have callbacks. Think of it: thousands of clients sending callbacks to every server message. Yikes.

Event Unsubscriptions
=====================

Of course, not everyone wants to listen for an event forever, so clients are allowed to stop listening for events:

.. code-block:: javascript
	
	qio.off('/app/update');

Once all clients have unsubscribed from an event, it will be cleaned up on the server, and life there will continue as normal.

Event Callbacks
===============

Callbacks are probably the trickiest event type. Whereas broadcast events go to everyone, and a general event goes to a single client, a callback is an event that is sent in *response* to another event. That is, for example, the client sent the server an event, and the server is responding with data *to that event*, such that the two events are linked together. It is possible to have chains of events going back and forth between server and client where each is required to maintain enough state to be able to carry the conversation to an end.

Callbacks are a bit tricky in their implementation details, but rest assured: whenever there is a callback, the server *WILL* issue a callback, and the client should do the same.