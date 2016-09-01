// Project 1
// Author: Tyrus Malmstrom
// Class: cs457
// Date: 8/31/2016
// 'Simple' chat program


// directives:
#include <stdio.h>
#include <stdlib.h> // exit();
#include <string.h>
#include <strings.h> // bzero() && bcopy()
#include <stdbool.h>
// ** needed directives for socket programming** //
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // struct sockaddr_in is defined in.
#include <arpa/inet.h> // inet_pton();
#include <unistd.h> // for closing file descriptor
#include <netdb.h> //  for hostnet services

// function prototypes:
void help_message();
void server(const char* ip_addr, const int port);
void client(const int port, const char* IP);
int good_port(const int  port); // going to use the struct sockaddr_in --> inet_pton();
int good_ip_addr(const char* ip);

#define DEBUG false
#define MESSAGE_SIZE 140

int main(int argc, char *argv[]){

  int port;
  char *IP;
  
    switch(argc){
  case 1:
    port = atoi(argv[2]);
    IP = argv[4];
    server(IP, port);
    break;
  case 2:
    help_message();
    break;
  case 5:
    port = atoi(argv[2]);
    if(DEBUG) printf("The port number passed was: %d\n", port);
    if( !(good_port(port)) ){
      printf("That is not a valid port number! Please enter that in again.\n");
      return -1;
    }
    IP = argv[4];
    if(DEBUG) printf("The IP address that was passed was: %s\n", IP);
    if( !(good_ip_addr(IP)) ){
      printf("That is not a valid IP address! Please enter that in again.\n");
      return -1;
    }
    client(port, IP);
    break;
  default:
    help_message();
    break;
  }

  return 0;
}


void help_message(){
  printf("Usage: chat [options] [ -p port number | -s IP address of other node ]\n\n");
  printf("Options:\n");
  printf("   -p \t specifies the port number to connect to\n");
  printf("   -s \t specifies the IP address of the other node\n");
  exit(0);
}

int good_port(const int port){
  // use ports withing the range: 49152 - 65535
  // All ports below 1024 are RESERVED
  if(port >= 49152 && port <= 65535){
    return 1;
  }
  return 0;
}

int good_ip_addr(const char* ip){
  struct sockaddr_in sa;
  // IP address format conversion --> inet_pton :: convert IP address from dot notation to 32 bit binary.
  int res = inet_pton(AF_INET, ip, &(sa.sin_addr) );
  return res != 0;
}

void server(const char *ip_addr, const int port){
  /* All the code to run the server */

  printf("Welcome to Chat!\n");
  printf("Waiting for a connection on %s port %d\n", ip_addr, port);

  // these struct's is what's going to hold all the data:
  struct sockaddr_in server_addr, client_addr;
  int socketfd,newsocketfd, rec, sent;
  socklen_t client_size;

  // Messages can only be 140 characters long:
  char message_buffer[MESSAGE_SIZE];

  // allocate a socket descriptor:
  socketfd = socket(AF_INET, SOCK_STREAM, 0);
  if(DEBUG) printf("A socket file descriptor is nothing but an int. %d\n", socketfd);
  // checking that I obtain the socket file descriptor:
  if(socketfd == -1 ){
    printf("Couldn't create the socket\n");
    exit(1);
  }
  // clearing out any data that remains in the server_addr:
  // this is writing zero-valued bytes for the whole server_addr struct -- to clear it out.
  bzero( (char*) &server_addr, sizeof(server_addr) );
  // testing...
  server_addr.sin_family = AF_INET; // IPv4
  server_addr.sin_addr.s_addr = INADDR_ANY;  /* puts server's IP automatically */
  server_addr.sin_port = htons(port);
  // binding socket to specified port number:
  if( bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1 ){ // means it failed to bind... 0 on success, -1 if not
    printf("Couldn't ding the port to the socket!");
    exit(1);
  }
  // The listen system call allows the process to listen on the socket for connections.
  listen(socketfd,1);

  client_size = sizeof(client_addr);
  newsocketfd = accept(socketfd, (struct sockaddr *) &client_addr, &client_size);
  if(newsocketfd == -1){
    printf("Couldn't create the client socket. There was an error on accept.\n");
    exit(1);
  }

  // writing values of bytes in the buffer to 0
  bzero(message_buffer, 140);

  // Lets recieve the data!
  rec = recv(newsocketfd, message_buffer, 140, 0);
  if( rec < 0 ){
    printf("Error recieving the data...\n");
    exit(1);
  }

  // printing out the data sent from the client:
  printf("Friend: %s\n", message_buffer);

  // writing back...
  sent = send(newsocketfd, "You: I recieved your message!", 140, 0);
  if(sent < 0){
    printf("Wasn't able to send data... ERROR writing to the socket.\n");
    exit(0);
  }

  // now closing sockets:
  close(newsocketfd);
  close(socketfd);

}

void client(const int port, const char* ip){
  /* All code for the client */

  int socketfd, port_number, sent, rec;
  struct sockaddr_in server_addr;
  // struct hostent *the_server; // this is a struct with many host related inqueries.

  char client_buffer[MESSAGE_SIZE];

  // obtain the port number:
  port_number = port;
  if(DEBUG) printf("The client side, the port is: %d\n", port_number);

  // create the socket:
  socketfd = socket(AF_INET, SOCK_STREAM, 0);
  if(socketfd == -1){
    printf("Error creating socket..\n");
    exit(1);
  }

  /*if( !inet_aton(ip, &ia) ){
    printf("Can't parse the IP address %s\n", ip);
    exit(1);
    }*/

  /*if ((the_server = gethostbyaddr( (const void *)&ia, sizeof(ia), AF_INET)) == NULL )
    printf("no name associated with %s", ip);*/

  // if(DEBUG) printf("name associated with %s is %s\n", ip, the_server->h_name);

  bzero( (char *) &server_addr, sizeof(server_addr) );

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_pton(AF_INET, ip, &(server_addr.sin_addr) ); // convert readable IP to bits

  
  // bcopy( (char *) the_server->h_addr, (char *) &server_addr.sin_addr, the_server->h_length);

  server_addr.sin_port = htons(port);
  
  // connecting to the server now..
  if( connect(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr) ) < 0 ){
    printf("Error connecting to the server\n");
    exit(1);
  }

  puts("Connected?");
  
  // client enters in their message:
  printf("You: \n");
  bzero(client_buffer, MESSAGE_SIZE);
  fgets(client_buffer, MESSAGE_SIZE, stdin);

  sent = send(socketfd, client_buffer, sizeof(client_buffer), 0);
  if(sent == -1){
    printf("Error sending message to the server...\n");
    exit(1);
  }

  bzero(client_buffer, MESSAGE_SIZE);

  rec = recv(socketfd, client_buffer, MESSAGE_SIZE, 0);
  if( rec == -1){
    printf("Error recieving the data from the server...\n");
    exit(1);
  }

  printf("Friend: %s\n", client_buffer);

  // close the socket:
  close(socketfd);

}
