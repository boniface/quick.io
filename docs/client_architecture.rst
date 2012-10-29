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

Client Events
=============

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
================

A client should implement the following functions:

==================== =============================
Function             Description
==================== =============================
on                   Subscribe to an event on the server. This MUST update all internal structures to reflect that the client is now subscribed to the event and will be until `off` is called.
off                  Unsubscribe to an event on the server (symantics are your call).
send                 Send an event to the server.
one (optional)       Subscribe to the event, recieve one event, and unsubscribe
==================== =============================