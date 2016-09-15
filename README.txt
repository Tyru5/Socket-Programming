Tyrus Malmstrom
8/31/2016

This program is utilizing socket programming to allow for end to end communication. We are using the server / client communication model.

Model based of the paradign that the GTA showed us in lab.

Server:
	=> Need to create a socket and get a socket descriptor
	=> From here, bind the socket to a particular port
	=> Listen on that port for incomming connections
	=> Then accept an incomming connection
	=> From here, recieve the data that the client is sending to the server (the connection)
	=> Send reply is required
	=> close the connection.

Client:
	=> Need to also create a socket and obtain a socket file descriptor.
	=> Connect to the server.
	=> Send some message
	=> Recieve some reply if required.
	=> Finally, close the connection.

