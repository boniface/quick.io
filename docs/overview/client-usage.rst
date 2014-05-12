Programming with a QuickIO Client
=================================

When first getting started with a QuickIO client, it's necessary to understand QuickIO's event model. If you've ever programmed with jQuery (or the DOM in a browser), worked with a UI, or used any publish-subscribe system, you will feel right at home with QuickIO. Originally based on the WebSocket standard, QuickIO provides a server-to-client, and vice versa, event framework for subscribing to events on the server, triggering events on a server, and responding to all events a server might trigger. The simplest way to learn to use the client is to jump in, so let's get started.

What is an event?
-----------------

Everything that the server sends and receives is treated as an event. An event can be something as simple as sending server time to a client to anything as complex as user authentication, handshakes, and callbacks. In reality, an event is just a single, complete message from the server that is addressed to a specific path, includes a callback ID, and contains some data to give to the path.

If you've ever used a URL before, the event paths will look familiar to you: `/some/event/path`. Notice the following properties about the event path:

1. Path elements are separated by slashes
2. There is no trailing slash
3. There is a slash at the beginning

Every single event path follows this pattern. In order to keep things simple, events are limited to the following ASCII characters: A-Z, a-z, 0-9, -, _, /. All other characters will be forcibly removed by the server and ignored, such that the following two event paths are identical::

	/app/user/new
	/app/@user/(new)>>>>

But that is only the first part of an event; the second part of an event is the callback ID. When sending an event to the server or client, either may request a callback be sent with some additional data. For example, say the client client wants to ask the server where in the world it is: rather than subscribing to an event and then asking the server to send data to that event, which would be incredibly complicated and prone to errors, the client may send an event to the server, request a callback on that event, and while maintaining its scope, get that data. For example, if you want the server's location, you might do the following:

.. code-block:: javascript

	send('/server/where-are-you', function(...) {})

The actual events sent between server and client would look like::

	/server/where-are-you:1=null
	/qio/callback/1:0={"code": 200, "data": "in the cloud"}

Notice how, after the colon following the event, the client sent a 1: this is a 64bit unsigned integer that increments with each callback. So in this case, the client requested that the server send an event to callback ID 1 with the data it requested from /server/where-are-you. The server responded with the JSON-encoded string {"code": 200, "data": "in the cloud"}, meaning that everything went OK (code 200), and that it's "in the cloud", as indicated by "data". Also notice that the client sent back a 0 for its callback ID, meaning that it did not want a callback.

In the above example, we came to the final point about events: all data exchanged between client and server `must` be encoded as JSON. No exceptions. When the server sends a callback to a client, it will include a status code (typically taking after HTTP status codes, though custom applications may set their own codes), data, and en error message (in the field `err_msg`) if something went wrong. Clients may send arbitrary JSON-encoded data back to the server without error codes and messages; any error codes and messages must be implemented on an application level and are not part of the protocol.

When using a client, you typically don't need to worry about event formatting and maintaining connections, subscriptions, or anything of the sort: the client will manage all of that for you transparently, and you may simply get your stuff done.

Sending Events
--------------

In some event systems, this is called "triggering an event", but the difference with QuickIO is that the event will be triggered `on the server`, not on the client. You cannot actually trigger events on the client; only events from the server are triggered on the client. Above was hinted as how to send events, now let's take a look at how to do this from a client perspective.

.. code-block:: javascript

	// Don't request a callback
	send('/some/event', {"tons": ["of json", "data"], "with numbers": 123123});

	// Request a callback, but send no data
	send('/server/where-are-you', function(data, cb, code) {
		if (code == 200) {
			// `data` contains the server's location
			cb("im not in the cloud");
		} else {
			// Something went wrong with the event, handle it
		}
	});

Let's go through what is happening with these events:

1. The first line is sending an event to the server at the path `/some/event`, including a large JSON payload, and not requesting a callback. This type of event might or might not be delivered to the server, and it's not a big deal if it isn't.
2. This event requests a callback from the server, mainly because it's requesting the server's location. As you can see, it sends no data to the server, which is completely acceptable, but it counts on the server sending back data. In the callback, notice that it checks the error code for the callback, making sure that it's 200 before doing anything else. If it's not 200, it should make an attempt to handle any errors. Errors may be anything from "client disconnected" (code==-1), to "event path not found" (code==404), to any application-specific code.

Event Delivering and Persistence
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When network conditions become rough, an event might or might not be delivered, and it's really important to understand how events are persisted and delivered. If a client is connected, chances are the events will be delivered to the server without any problems. If an event is sent to the server without a callback, then there is no way for the client to know if the server received the event. In some cases this is acceptable, but in some cases, you need to know that the server got the event. In these cases, you should ask the server for a callback, and you will know either that it got the event (it responds with some code), or that the connection to the server was lost, and you should try sending the event again. If the client is disconnected when you send an event, the event will be queued up until a connection has been re-established, and then it will be sent. This queuing only works for top-level events, meaning that any attempt to send a callback when disconnected will result in an immediate error callback.

Sending a callback after losing a connection and reconnecting will also result in an immediate error callback: callbacks `cannot` be persisted across connections. But why? Well, there are a few reasons: when the client reconnects, it might connect to a different server in the cluster, so it can't assume that the new server knows anything about it. It's also impossible to manage callback state without explicit knowledge of what's being implemented. For example, consider the following:

.. code-block:: javascript

	send("/i-need-help", function(data, code, cb) {
		if (code != 200) {
			// What should be done here?
			return;
		}

		if (data == "what can I do for you?") {
			cb("where can I get something to eat?", function(data, code) {
				// Data contains where to go eat, or maybe there's an error code!
			});
		} else if (data == "i don't care") {
			cb("that makes me sad");
		}
	})

In this example, it's not plausible for the server and client to maintain the state of the callbacks, especially cross-server, possibly even cross-datacenter, especially since a client might vanish at any point in this conversation. So it's up to the client to ensure that anything it needs is delivered, or respond appropriately to the error codes. The server will do its best to fulfill all client requests, but it can only go so far.

Receiving Events
----------------

The other part of handling events is being able to receive and route them properly. In order to receive events in the client, you'll be subscribing to events and reacting to them. To do this, you very simply do the following:

.. code-block:: javascript

	on('/event/i/care/about', function(data) {
		// Handle the event in whatever way the application needs to
	});

That's right: you just subscribe to the event using an on() function, and whenever the server triggers the event, your function will be called. This should feel very similar to every other event system out there.

When you no longer care about an event, you have a few options for telling the server:

.. code-block:: javascript

	// Remove all listeners
	off('/event/i/care/about');

	// Remove only 1 listener
	off('/event/i/care/about', cb);

The first line removes all callbacks from the event and informs the server it no longer cares about the event. The second line removes only a single callback from the event, identified by the variable `cb`, and will only unsubscribe on the server if there are no other listeners for that event.

If you only want to hear about the event once, there is also a useful function for that:

.. code-block:: javascript

	one('/single/event', function(data) {
		// Handle the event
	});

When using one(), the event will be immediately unsubscribed from after the first event.

In these examples, we've just ignored error handling. For the most part, once you're subscribed to an event, you won't get any errors. During subscription, however, it's possible, so let's check out how to handle it.

.. code-block:: javascript

	on('/possible/subscription/error'
		function(data) {
			// When everything goes well
		},
		function(code, errMsg) {
			// When there's a subscription error
		});

In this case, when there is a subscription error, your error callback will be called immediately, and the subscription will be purged from the client. As usual, the application may choose how to respond to any subscription errors.

Special Events
--------------

Clients provide a few useful events for responding to connection and state changes. They follow, with their descriptions:

============ =================
 Event Path   When it's fired
============ =================
/open         When a connection has been established to a QuickIO server, and the client is ready to send events to the server.

/close        Whenever a connection has closed after being opened.

/error        Whenever there is some sort of error in the client that the client wants to report. This event is typically only informative and is used for debugging and logging and is heavily platform-dependent. This event may also fire numerous times while the client attempts to establish a connection to the server.

============ =================

.. note::

	QuickIO reserves the `/qio` namespace for itself and its internal events. You'll never need to interact with anything in that namespace, so it's pretty safe to ignore.
