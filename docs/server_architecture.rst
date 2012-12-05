Server Architecture
*******************

Quick.IO is broken into 5 components:

Connections as Clients
======================

A single connection to the server is treated as an individual client. A single client may hold many subscriptions, many server-callbacks, and incomplete messages (coming off the wire). Each client and all its state exists only as long as the connection from the client is maintained; if that connection is ever closed, all state on the client is lost. The general philosophy is: to the server, a client is just some event subscriptions and server-callbacks, as soon as that client is gone, all of the data is useless and cannot be associated with any other client. That being said, clients can maintain a list of subscriptions and re-subscribe to everything on reconnect.

All clients, as far as the server is concerned, are stateless connections that can be hurled from server to server at any given moment without any ill effect. Client applications should be designed such that a server disconnect doesn't mean the end for the client: once reconnected, the client should resume its normal operation as though everything is going as planned.

quick-event
-----------

The server is built on top of a statically-routed, event-driven framework called "quick event". Originally, all of the socket code and everything was part of QIO, but it became too complicated to maintain in the same project, and it was broken into quick-event. Quick-event provides a thread-agnostic implementation of event routing that uses minimal locking.

Thread Safety
-------------

Events on clients are limited to happening in 1 thread. When an operation needs to be performed on a global struture, there are a few locks to ensure atomicity. With the list of connected clients, however, many threads may append to it at the same time using a read lock (used as an append lock). For removal, a write lock must be obtained. Each of these operations happens very quickly, so spin locks are used rather than involving the operating system, which is incredibly costly compared with waiting (at most) a few hundred cycles.

Client Handlers
===============

Though Quick.IO is primarily a WebSocket server, it allows other types of clients to connect, be they mobile devices, other servers, or even `a badger <http://www.strangehorizons.com/2004/20040405/badger.shtml>`_. Each client may speak to the server in different ways, depending on how they're feeling, the alignment of the planets, or the laziness of their programmer; thus, Quick.IO has a concept of handlers. Each client has an associated handler, and it is responsible for decoding messages to the server and framing messages to the client so that, when the client receives the message, it is capable of decoding and processing it.

Applications
============

Applications are the core of what runs on the server. Aside from the framework for supporting events, subscriptions, and clients, applications doing the heavy lifting with the clients. Applications may be written in C/C++, Python, Javascript, or any other language that has a C/C++ interface. In the source, there are (incomplete) example apps for running Python and NodeJS applications.

Client Events
=============

Client events are events that are sent *from* the server *to* the client. The functions inside the server dealing with this are all prefixed with evs_client_*.

Server Events
=============

Server events are events that are sent *from* the client *to* the server. The functions inside the server dealing with this are all prefixed with evs_server_*.