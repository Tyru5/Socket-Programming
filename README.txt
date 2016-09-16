Tyrus Malmstrom
8/31/2016 - 9/15/2016
README file for grading Project 1.

!!!****************************************************IMPORTANT**************************************************************!!!
 1) Quick note on the format of the output. There are some extra newlines in my output when testing it with myself however
   when testing it with another student's program, the output matches exactly as it was shown in the assignment. This is because when recieving what other's have sent,
   some folk append a newline ('\n') character at the end of their standard in text and some don't. I just want to make it clear that this isn't a problem with my program,
   but that of a formatting / stdin of the text.

 2) I just want it to also be known that I spent a lot of hours on this assingment trying to understand the Packet structure. This involved understanding memcpy, and
   where to copy the bytes into and at what position in the buffer. Also a lot more. With that said, this assignment was awesome and I learned a lot about socket programming. The reason to
   why I'm saying this is because I believe that some students didn't create the Packet structure the way it said to in the assignment instructions. I did and it probably won't work with
   their code because of the way I serialize and de-serialize the packet. Here is the form of my Packet Struct:

// Packet Structure:
typedef struct packet_info{
  // version:
  short version;
  // String length, 16 bits:
  short string_length;
  // actual message:
  char *message; <----- I choose to define the message within the struct as a pointer.
  char message[140] <---- However some students defined it as this which probably won't work with my code.
} Packet;               



 ::Structure of Assignment:: 
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

::The process of invoking my Program::
- To compile and invoke my program are as follows:

  1) To compile my program, all the user has to do is run 'make all' which will compile the chat.c file. On the contrary, for the user to clean up the project / directory, one would have to
     run 'make clean' which will automatically clean the project and directory. Lastly, if the user wanted to clean the old project and then compile in one command, the user would type in
     'make clean all'.
  2) There are 3 ways of invoking my program. They are as follows:
     	   - by invoking the program with the '-h' parameter will bring up a help / usage dialog on how to run / invoke my program. --> ./chat -h
	   - by invoking the program with NO parameters, my program will run as the SERVER. --> ./chat
	   - by invoking the program with arguments, my program acts as the CLIENT. '-p' refers the the specific port and '-s' refers to the specific IP address that the server is running on. --> ./chat -p <port number> -s <IP address>

::Questions I had for the TA::
 - I had a lot of questions for the TA regarding this project. The are as follows:

Q: How do you print off the IP address of the machine that the server is running on?
A: The TA answered that within one of the weeks recitation slides. Essentially use the gethostname() and getaddrinfo() functions.

Q: How exactly do you want us to setup the Packet structure for this project?
A: The TA answered this question over a series of long conversations. Essentially follow the packet structure stated in the assignment directions.

Q: How do you serialize and de-serialize packet structures?
A: Again, this was answered over a series of long converstaions. However the main point is to use memcpy() function on each field of the structure.

And that's pretty much it!
Again, I want to state that this was an awesome assignment and I learned a lot while completing it.
Thanks!

Best,
Tyrus Malmstrom.
