The Run Down
************

This page contains the quick-and-dirty introduction to QuickIO. This is a high-level overview, shedding the weight of code descriptions, algorithms, and other fun stuff, and instead using JavaScript examples to highlight points.

The Event Model
===============

Everything that the server sends and receives is treated as an event. An event can be something as simple as sending server time to a client to anything as complex as user authentication, handshakes, and callbacks.

Event paths take the form of `/some/path/to/an/event` such that the they are namespaced, allowing multiple different applications to work independently of each other on the same server. Event paths may only consist of the following characters::

	-/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz

Any other character in the path will be summarily removed and ignored.  In other words, the following two paths will be identical once event path validation finishes::

	/app/user/new
	/app/@user/(new)>>>>

Event Format
------------

Data sent to and from the server is composed of 4 different parts::

	event_path:callback_id=json_data

1. event_path: just the path of the event, in its full glory
2. callback_id: an integer, greater than 0 (0 indicating no callback)
3. data: JSON data as a string

Event Example
=============

With all of this in mind, let's go through a deep example that demonstrates the power of this simple event format:

.. code-block:: javascript
	:linenos:

	qio.send('/app/login', {session_id: '1234abcd'}, function(data, cb, code, errMsg) {
		switch (code) {
			case 400:
				log('Login failed :(');
				break;

			case 200:
				qio.on('/app/update', function(data) {
					log('The app was updated on: ' + data.date);
				});

				cb({iam: 'Superman'}, function(data) {
					log('The server says his name is: ' + data.iam);
				});
				break;
		}
	});

Line 1
------

In this case, we're simply sending an event with some JSON data to /app/login; the actual data sent to the server will look like::

	/app/login:1={session_id: "1234abcd"}

This is just a simple, quick method of conveying everything about the event. Notice, too, that a function was passed in to the event. This function will be held by the client until the server responds with a callback. Once a callback is recieved, such as this one::

	/qio/callback/1:1={"code": 200, "data": null}

The function will be triggered on the client side. The function will be called with the following parameters:

* data = null (see the key in the JSON payload)
* cb = some function (since the server also requested a callback with ID 1)
* code = 200 (see the key in the JSON payload)
* errMsg = null as there was no error given

Also, as we'll see later on at Line 12, the server requested a callback from us (the 1 before the `=`), so we should be polite and honor that.

At this point, we're at Line 2.

Line 2
------

Based on code, we're going to decide where to jump next; since the code was 200, we're jumping to line 7.

Line 7
------

Since everything was good, we're going to ask the application to inform us every time it is updated. That is, we're subscribing to the event `/app/update`, and whenever that event fires, the function that is passed in will be called. The corresponding event that might be sent to the serer is::

	/qio/on:0="/app/update"

Line 12
-------

Remember that the server requested a callback? Well, we're going to respond to that callback by sending it our name. But wait, we're also passing a callback to the callback! Don't fret, think of it like this: the server sent us an `event` that requested a callback, so now we're just sending an event that happens to be a callback to the server, and with that event, we're requesting a callback::

	/qio/callback/1:2={"iam": "Superman"}

Line 13
-------

After receiving our message and seeing that we're Superman, the server will respond with its name. Let's try not to be dissapointed::

	/qio/callback/2:0={"code": 200, "data": {"iam": 'Howard'}}

Apparently, the server's name is Howard, and it's so ashamed of itself that it didn't even request a callback, so that conversation is over.

Line 9
------

Wait a second, at one point, we subscribed to app updates. And it looks like the server just had an update; this is what that event looks like::

	/app/update:0={"date": "Jan 1, 1970"}

Notice how this event doesn't include a code or any other fields: those are specific to callbacks. All of the JSON data in this example is passed directly into the callback.

Output
------

So after all that, what will actually be logged? Well, very simply, not much::

	The server says his name is: Howard
	The app was updated on: Jan 1, 1970

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

Callbacks are probably the trickiest event type. Whereas broadcast events go to everyone, and a general event goes to a single client, a callback is an event that is sent in *response* to another event. That is, the client sent the server an event, and the server is responding with data *to that event*, such that the two events are linked together. It is possible to have chains of events going back and forth between server and client where each is required to maintain enough state to be able to carry the conversation to an end.

Callbacks are a bit tricky in their implementation details, but rest assured: whenever there is a callback, the server *WILL* issue a callback, and the client should do the same.
