Server Architecture
*******************

QuickIO is broken into 4 components:

Clients
=======

Clients are the individual instances of some object sitting out on a client computer that is attempting to talk to the server. A single client may hold many subscriptions, many server-callbacks, and incomplete messages (coming off the wire). Each client and all its state exists only as long as the session with the server is maintained in accordance with the underlying protocol. Once that session is lost, all state of the client on the server is lost, and the client will need to setup its state again with the server. The general philosophy is: to the server, a client is just some event subscriptions and server-callbacks, as soon as that client is gone, all of the data is useless and cannot be associated with any other client. That being said, clients can maintain a list of subscriptions and re-subscribe to everything on reconnect.

All clients, as far as the server is concerned, are stateless connections that can be hurled from server to server at any given moment without any ill effect. Client applications should be designed such that a server disconnect doesn't mean the end for the client: once reconnected, the client should resume its normal operation as though everything is going as planned.

quick-event
-----------

The server is built on top of a statically-routed, event-driven framework called "quick event". Originally, all of the socket code and everything was part of QIO, but it became too complicated to maintain in the same project, and it was broken into quick-event. Quick-event provides tons of useful data structures, high-optimized buffers, monitoring, benchmarking, resource polling, and a bunch of other really awesome stuff. At its core, though, it's a thread-agnostic event loop on top of the network layer that uses minimal locking.

Protocols
=========

Though QuickIO is primarily a WebSocket server, it allows other types of clients to connect, be they mobile devices, other servers, or even `a badger <http://www.strangehorizons.com/2004/20040405/badger.shtml>`_. Each client may speak to the server in different ways, depending on how they're feeling, the alignment of the planets, or the laziness of their programmer; thus, QuickIO has a concept of handlers. Each client has an associated protocol, and that protocol is responsible for decoding messages to the server and framing messages to the client so that, when the client receives the message, it is capable of decoding and processing it.

Since clients may communicate with the server over HTTP, WebSocket, or any other future protocols, it's impossible to tie a client to a single socket. Each client is bound to some underlying socket by the providers that are running on the server. For WebSocket clients, they are bound to a single socket, and data can flow at will in either direction. For HTTP clients, they are bound only temporarily to a socket while an HTTP request is active.

Applications
============

Applications are the core of what runs on every server in a cluster. They are typically what developers will be implementing to get their own functionality into the server and work in the same way that any HTTP application would: when a client sends a "request" (in the QuickIO case, an event), a handler for the request will be invoked. Applications are just a way of compartmentalizing a bunch of similar events.

Applications may be written in C/C++, Python, JavaScript, or any other language that has a C/C++ interface.

Events
======

Everything that passes through QuickIO to an application, to an event handler, from a subscription to a client, is an event. Events are formatted uniquely based on their protocols, but at the end of the day, they consist of three parts: an event path, a callback ID, and any data to be given to the event handler. When an event comes from a client, the event handler is looked up and given the data that the client sent.
