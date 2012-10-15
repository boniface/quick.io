Client Architecture
*******************

Clients are built around a few, simple principles:

* If a client requests a callback, it will ALWAYS receive one.

	* The callback might contain "qio_error" if something goes wrong.

* If the server requests a callback, it is polite to send one.

* Clients can be killed at any moment by the server, but they are allowed to reconnect and reestablish themselves.

* Clients can be told to move to a different server; they must follow this instruction.