var net = require('net'),
	clients = 0;

var server = net.createServer(function (socket) {
	socket.write('Echo server\r\n');
	socket.pipe(socket);
	
	clients++;
});

setInterval(function() {
	console.log('Clients: ' + clients);
}, 1000)

server.listen(5000, '127.0.0.1');