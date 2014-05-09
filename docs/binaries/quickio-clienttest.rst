quickio-clienttest
==================

.. program:: quickio-clienttest

Synopsis
--------

quickio-clienttest

Description
-----------

Run QuickIO to test client implementations against it.

When run, this helper will instruct QuickIO to load up an application that is equipped to handle everything a client will need to run tests. The following events are created, and their functions follow.

=============================== ============
 Event                           How to Use
=============================== ============
/clienttest/broadcast            Receive a broadcast every 10 milliseconds. Used primarily for testing on() and off() methods.

/clienttest/close                Initiate a close on the server side to ensure that the client reconnects when the server terminates the connection.

/clienttest/drop-callback        Send an event to the server, requesting a callback, but never get one. Typically, after calling this, you should send `/clienttest/close`, wait for a reconnect, and ensure that the callback receives a -1 code (disconnected).

/clienttest/echo                 Responds to any callback with the data it was sent. Use this for testing callback chains.

/clienttest/error                Sending an event of the form `/clienttest/error/500` will send a callback with error code 500. You may replace the 500 with any integer to test any error code.

/clienttest/heartbeat-challenge  Have the server send a message of the form /qio/heartbeat:1=null. Just make sure that when it sends this you send back a callback.

/clienttest/in-progress          Test that the client handles a code 202 correctly when subscribing to events. This events works as follows: (1) send a /qio/on:1="/clienttest/in-progress" to the server (2) send a /qio/off to the server for the same event (3) send a /qio/on for the same event, again (4) send an event to /clienttest/in-progress, requesting a callback. The subscription will be processed and approved, and the callback with get a code 200 if everything went right. Otherwise, an error will be sent. The client should also check to make sure that, at this point, it has an active subscription to the event in its internal event storage.

/clienttest/move                 Force the server to send the client a move message immediately. The data sent to this event will be echoed back to the client, so it should be the address that the client should move to.

/clienttest/send-invalid         Force the server to send an event that does not follow QuickIO's event format. The client should immediately close the connection when seeing the event.

/clienttest/send-unsubscribed    Force the server to send an event to the client that it's not subscribed to. The client should only drop the message and nothing else.

=============================== ============

This helper creates two versions of QuickIO: a bare version and a proxified version. Both are necessary for thoroughly testing a client implementation.

Bare QuickIO
^^^^^^^^^^^^

This is just the bare QuickIO binary listening on port 55440. When testing with this port, the server will respond without any interference. This is best for testing basic client functions.

Proxified QuickIO
^^^^^^^^^^^^^^^^^

Since QuickIO clients have to run behind proxies, it's necessary to test that the clients can fallback to HTTP long polling as necessary. On port 55441, you'll find a mini server that sits between the client and the server running on port 55440 and that rewrites WebSocket's initial HTTP upgrade headers in much the same way that proxies do. When connected to 55441, your client MUST ALWAYS fallback to HTTP long polling. If the client doesn't, then that is a bug in your client.

Options
-------

There are no options specific to quickio-clienttest. It just runs.

.. include:: bugs.rst
