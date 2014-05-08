Description
===========

Run QuickIO to test client implementations against it.

This helper creates two versions of QuickIO: a bare version and a proxified version. Both are necessary for thoroughly testing a client implementation.

Bare QuickIO
------------

This is just the bare QuickIO binary listening on port 55440. When testing with this port, the server will respond without any interference. This is best for testing basic client functions.

Proxified QuickIO
-----------------

Since QuickIO clients have to run behind proxies, it's necessary to test that the clients can fallback to HTTP long polling as necessary. On port 55441, you'll find a mini server that sits between the client and the server running on port 55440 and that rewrites WebSocket's initial HTTP upgrade headers in much the same way that proxies do. When connected to 55441, your client MUST ALWAYS fallback to HTTP long polling. If the client doesn't, then that is a bug in your client.
