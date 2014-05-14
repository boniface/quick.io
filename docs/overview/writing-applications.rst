Writing Applications
********************

Applications are what *your* code runs in. An application should run in its own namespace and respect the internal namespace of the server. Applications may be written in any language that has C/C++ bindings.

Structure
=========

Applications are responsible for running any internal threads they might need, maintaining any connections to external services, and not bringing the server down when they fail. Applications will receive only two general callbacks from the server during their typical lifetime, and a possible third callback when running unit tests.

Initialization
--------------

The first callback to the application will be right after it's loaded into memory: it will be told to initialize itself. During this time, the application should listen for all events it's interested in, setup any internal structures, and run any threads necessary to do its job. Should the application fail to initialize, it should inform the server by returning FALSE, indicating it failed.

.. note:: Initialization is optional, but if you don't implement it, you can't really listen for events, and that's rather pointless.

Exiting
-------

The only other time the application will receive a general callback is when the server is exiting. At this point, the application MUST shut down its internal threads, close any external connections, and free any resources.

Registering Events
==================

Since the server is built entirely around events, it might be useful to know to know how to register them. The call is incredibly simple:

.. code-block:: c

	// EV_PREFIX is usually defined as some namespace for your application, like: #define EV_PREFIX "/my-app-prefix"
	struct event *myev = evs_add_handler(EV_PREFIX, "/my-event", _myev_handler, _myev_on, _myev_off, FALSE);

This function returns an `struct event`, which is used to broadcast events to all clients subscribed to the event.

`_myev_handler`
	a function that will be called when the event is triggered from a client; if this is `NULL`, then it is assumed that no handler is wanted.

`_myev_on`
	called when a client subscribes to an event. If this is NULL, it is assumed that any client will be allowed to subscribe. If it is a function, it may do any of the following:

1. Return EVS_STATUS_OK, indicating that the client should be subscribed immediately.
2. Return EVS_STATUS_HANDLED, indicating that no response should be sent back because verification of the event will be processed asynchronously, and a callback will be sent later.
3. Return EVS_STATUS_ERR, indicating that the client is not allowed to subscribe.

.. important:: This function MUST execute as fast as possible as it runs in one of the server's main threads. Any background operations, long processing, socket operations, or anything that could potentially block should be done in another thread. Return EVS_STATUS_HANDLED for these operations, and schedule them to run in another thread.

`_myev_off`
	called when a client unsubscribes from an event. This is just a notification that the client left, and it cannot be stopped.

`handle_children`
	a boolean value indicating if child events should also be handled by this event handler. If TRUE, the following events will all be handled by /my-app-prefix/my-event:

* /my-app-prefix/my-event
* /my-app-prefix/my-event/some/child
* /my-app-prefix/my-event/another/child

It is completely up to the event handler if it wants to handle events with these path segments, and it may reject any event with segments it does not like. Extra event segments go to the handler and callbacks in a variable called `ev_extra`, or located inside `struct evs_on_info` as ev_extra.

.. note::

	QuickIO reserves the `/qio` namespace for itself and its internal events. Applications MAY NEVER create events in this namespace.

Handling Events
---------------

When a client sends an event to the server, it is routed to the registered handler. The handler callback looks like:

.. code-block:: c

	typedef enum evs_status (*evs_handler_fn)(
		struct client *client,
		const gchar *ev_extra,
		const evs_cb_t client_cb,
		gchar *json);

This function is expected to return a status, as described above, and is given the following:

1. client: The client that sent the event
2. ev_extra: Any extra path segments sent from the client
3. client_cb: Where any callback from the event should be sent
4. json: A JSON string of data from the client. Check out `qev_json <../doxygen/json_8h.html>`_ for reading it.


Sending Callbacks
=================

So what if you have an event subscription callback that does asynchronous verification of events, and you want to send the proper callbacks? There are a set of callback functions to help you out here.

.. code-block:: c

	evs_on_cb()
	evs_on_info_copy()
	evs_on_info_free()

`Check them out <../doxygen/evs_8h.html>`_.

Creating Server Callbacks
=========================

Client callbacks are simple enough to understand, but what about when you want to be able to have a callback on the server? The easiest way to see these is to look at some code:

.. code-block:: c

	// Whatever data needs to be passed to callback handler
	struct callback_data {
		int id;
		gchar *name;
		float percent;
	};

	enum status _callback(
		struct client *client,
		const void *data,
		const evs_cb_t client_cb,
		gchar *json)
	{
		return CLIENT_GOOD;
	}

	void _free(void *data)
	{
		struct callback_data *d = data;
		g_free(d->name);
		g_slice_free1(sizeof(*d), d);
	}

	static enum evs_status _handler(
		struct client *client,
		const gchar *ev_extra G_GNUC_UNUSED,
		const evs_cb_t client_cb,
		gchar *json)
	{
		struct callback_data *d = g_slice_alloc0(sizeof(*d));
		d->name = g_strdup("test");

		evs_cb_with_cb(client, client_cb, json, _callback, d, _free);

		return EVS_STATS_HANDLED;
	}

	... snip ...

	evs_add_handler(EV_PREFIX, "/event", _hander, evs_no_on, NULL, FALSE);

	... snip ...

The event flow will be as follows:

1. An event comes in and is sent to _handler()
2. Handler sends a callback to the client, passing it the function to be called, the data to be given to the function, and a free function. The ownership of the data passes to QuickIO, and it will free it as necessary.
3. The client receives the event and triggers the server callback
4. The _callback() function is called, and it responds exactly like _handler does
5. Once _callback() is done, the data is free'd

.. note:: A server callback might *never* be called: a client has a limited number of server callbacks it can have registered simultaneously, so if it exceeds the number its allowed, then old callbacks will be culled to make room for the new. Chances are this will never happen, but it is a possibility.

Example
=======

In this example, we (fakely) track the population of the world, sending out an updated population event every time a baby is born.

You'll probably want to check out the `Doxygen API docs just to see what is possible <../doxygen>`_

Code
----

Enough talk, let's see what we can do here!

.. literalinclude:: app_skeleton.c
	:language: c
	:linenos:
