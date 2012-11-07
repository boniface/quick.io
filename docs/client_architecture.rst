Client Architecture
*******************

Clients are typically very simple and provide a few events for larger applications to hook in to.

Principles of Clients
=====================

Clients are built around a few, simple principles:

* If a client requests a callback, it will ALWAYS receive one.
	* The callback might contain "qio_error" if something goes wrong.
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
/qio/callback/{id}   Fired when the server is sending a callback to the client; the id paramater is the callback id.  This should call the registered callback function registered at this id.
/qio/move            Fired when the server is forcing the client to balance to another QIO server.  The data in the event is the server to move to.  This should be transparent to anything running, if possible.
==================== =============================

Client Functions
----------------

A client should implement the following functions:

==================== =============================
Function             Description
==================== =============================
(constructor)        Creates an object (or control structure) for handling all Quick.IO requests.
on                   Subscribe to an event on the server. This MUST update all internal structures to reflect that the client is now subscribed to the event and will be until `off` is called.
off                  Unsubscribe to an event on the server (semantics are your call).
send                 Send an event to the server.
one (optional)       Subscribe to the event, receive one event, and unsubscribe
==================== =============================

Client Heartbeats
-----------------

Your average user will most likely be someone connecting to your service from behind some NAT gateway: this presents some interesting problems. In order to make sure the client maintains a connection to the server at all times, even when there is no activity, application-level heartbeats are employed. By default, the server sends a `/qio/heartbeat` event to every client every 60 seconds (this is not client-specific: if a client connects, it might receive a heartbeat after a few seconds, followed by another 60 seconds thereafter).

The best method for implementing a heartbeat is:

1. Once the connection is open, the client should set a timer for 65 seconds for heartbeat events, thus giving the server 5 seconds padding around heartbeat events.

2. The client should listen for a `/qio/heartbeat` event (bot not subscribe).

3. When a `/qio/heartbeat` event comes in, it should reset its 65 second time for another 65 seconds.

4. If there is no heartbeat event in 65 seconds, the client should send a `/qio/ping` to the server, with a callback, to verify the server is still there.
	
	1. If the server sends the callback, restart the timer.
	2. If the socket is broken, reconnect logic should kick in.
	3. If using a websocket, the browser should fire off a `close` event on the socket, and reconnect logic should kick in.