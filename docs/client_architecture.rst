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

A client MUST implement the following in order to communicate with a Quick.IO server consistently and properly.

Client Events
-------------

A client should, for the sake of consistency, handle the following:

==================== =============================
Event                Description
==================== =============================
/open                Fire when a new connection to the server is established, every time a connection is established
/close               Fire whenever a connection to the server is lost or closed (even by a client)
/error               Fire when there is an error connecting to the server (this is typically platform dependent)
/qio/callback/{id}   Fired when the server is sending a callback to the client; the id parameter is the callback id. This should call the registered callback function registered at this id.
/qio/move            Fired when the server is forcing the client to balance to another QIO server. The data in the event is the server to move to. This should be as transparent as a reconnect.
==================== =============================

Client Functions
----------------

A client should implement the following functions:

==================== =============================
Function             Description
==================== =============================
(constructor)        Creates an object (or control structure) for handling all Quick.IO requests.
on                   Subscribe to an event on the server. This MUST update all internal structures to reflect that the client is now subscribed to the event and will be until `off` is called.
one                  Subscribe to the event, receive one event, and unsubscribe
off                  Unsubscribe from an event on the server
send                 Send an event to the server.
==================== =============================

Client Heartbeats
-----------------

Your average user will most likely be someone connecting to your service from behind some NAT gateway: this presents some interesting problems. In order to make sure the client maintains a connection to the server at all times, even when there is no activity, application-level heartbeats are employed. By default, a client will receive at least one message every 60 seconds, be it in the form of a callback, broadcast event, or heartbeat.

Heartbeats are implemented such that, if a client hasn't been sent a message in around 60 seconds (this is variable to within -10 seconds, but a client will never go more than 60 seconds without a message), it will receive a heartbeat.

The best method for implementing a heartbeat is:

1. The client should listen for a `/qio/heartbeat` event (but not subscribe).

2. Once the connection is open, the client should set a timer for 65 seconds, listening for messages from the server.

3. Every time the client receives a message from the server, it should reset its timer.

4. If there has been no activity within 65 seconds, assume the server is dead and reconnect.
