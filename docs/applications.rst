Applications
************

Applications are what *your* code runs in. An application most likely runs in its own namespace, though this is transparent to it, and it has a set of events that is handles. They may be written in any language that has C/C++ bindings.

Structure
=========

Applications are run in a separate thread from the main server, but where they end and the main thread begins can be fuzzy. Here are two rules to keep in mind:

1. If it's an event handler, callback, subscription notification, or general app event, it's from the main thread.

2. If it's anything else, it's in the app thread.

The application thread goes through phases in its lifetime: initialization and running.

Initialization
--------------

The thread will first be spooled up during server initialization: during this time, the thread will receive a call to `app_init()`. This function should setup all the application's internal structures, request its configuration be loaded, validate any configuration, and, if anything fails, `return FALSE`, thus causing the server to fail loading. It might also be helpful (for debugging) to spit out a message with `ERROR()` before failing. If everything goes well, however, the application should `return TRUE`, indicating that it is ready to run.

.. note:: `app_init()` is an optional function; if it's not implemented, it will be assumed that the application is set to run.

Running
-------

After all of the applications have been initialized, the server will begin running and accepting connections, and the applications will each receive a call to `app_run()`. This is where the application should run its main loop, do all its work, process events, and so forth.

.. tip:: To synchronize events coming from the main thread to the application, `GLib Asynchronous Queues <http://developer.gnome.org/glib/2.32/glib-Asynchronous-Queues.html>`_ are awesome.

The application thread is expected to run for the duration of server lifetime, so it should never exit. If it does, however, encounter a fatal error, it should `return FALSE`, and the server will be brought down with the thread. If the application chooses to run for a short time and exit, then it should `return TRUE`, indicating that it meant to leave the `app_run()` function, and that the server should keep running.

.. note:: `app_run()` is an optional function; if it is not implemented, it will be assumed that the application just listens for events and runs in the main thread.

Registering Events
==================

Since the server is built entirely around events, it might be useful to know to register them. The call is incredibly simple (from the `app_init()` or `app_run()` functions):

.. code-block:: c
	
	on("/path/to/event", handler_function, on_subscribe_callback, on_unsubscribe_callback, should_handle_children)

This function returns an `event_handler_t`, which is used to broadcast events to all clients subscribed to the event. 

.. important:: You will NEVER receive the event name again in the application; it will be referred to exclusively by the pointer to the handler. As applications can be namespaced without their knowledge, passing event paths back to the applications would create way too many problems.

The `handler_function` parameter is a function that will be called when the event is triggered from a client; if this is `NULL`, then it is assumed that no handler is wanted.

`on_subscribe_callback` is called when a client subscribes to an event. If this is NULL, it is assumed that any client will be allowed to subscribe. If it is a function, it may do any of the following:

1. Return CLIENT_GOOD, indicating that the client should be subscribed immediately.
2. Return CLIENT_ERROR, indicating that an error should be sent back to the client. This is indistinguishable from the error sent back when the event does not exist.
3. Return CLIENT_ASYNC, indicating that no response should be sent back because verification of the event will be processed asynchronously. When issuing this value, it becomes the responsibility of the application to issue the callback to the client. Remember: clients MUST always receive a callback if they ask for one.

`on_unsubscribe_callback` is called when a client unsubscribes from an event. This is just a notification that the client left, and it cannot be stopped.

`should_handle_children` is a boolean value indicating if child events should also be handled by this event handler. For example:

.. code-block:: c
	
	evs_server_on("/test/event", NULL, NULL, NULL, TRUE)
	evs_server_on("/test/alone", NULL, NULL, NULL, FALSE)

The event handler at `/test/event` will handle children events, in the form of:

* /test/event/1
* /test/event/fun
* /test/event/many/crazy/path/segments

It is completely up to the event handler if it wants to handle events with these path segments, and it may reject any event with segments it does not like. Extra event segments go to the handler and callbacks in a variable called `extra`.

The event handler at `/test/alone` will ONLY accept events to `/test/alone`; all others will be rejected.

Sending Callbacks
=================

So what if you have an event subscription callback that does asynchronous verification of events, and you want to send the proper callbacks? There are a set of callback functions to help you out here.

.. code-block:: c
	
	evs_client_app_sub_cb()
	evs_client_send_callback()
	evs_client_send_error_callback()

Check out `the Application Functions in Doxygen <./doxygen/group__AppFunctions.html>`_ for more information.

Example
=======

In this example, we (fakely) track the population of the world, sending out an updated population event every time a baby is born. 

Code
----

Enough talk, let's see what we can do here!

.. literalinclude:: app_skeleton.c
	:language: c
	:linenos: