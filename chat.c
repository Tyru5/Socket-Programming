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
#include <netdb.h>

typedef struct port_ip{

  char *ip;
  int port;
  
} Ip_Port;

// function prototypes:
void help_message();
void server();
void client(const int port, const char* IP);
int good_port(const int  port); // going to use the struct sockaddr_in --> inet_pton();
int good_ip_addr(const char* ip);
int hostname_to_ip(char * hostname, char *ip);
void process_cargs(const int argc, char *argv[], Ip_Port *ipp);

#define DEBUG false
#define MESSAGE_SIZE 140
#define PORT 51717

int main(int argc, char *argv[]){


  Ip_Port ipp;
  
  process_cargs(argc, argv, &ipp );

  switch(argc){
  case 1:
    server();
    break;
  case 2:
    help_message();
    break;
  case 5:
    client(ipp.port, ipp.ip);
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
  if(port >= 1024 && port <= 65535){
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

void server(){
  /* All the code to run the server */

  printf("Welcome to Chat!\n");

  // these struct's is what's going to hold all the data:
  struct sockaddr_in server_addr, client_addr;
  struct addrinfo hints, *info;

  int socketfd,client_file_descriptor, rec, sent, gai_result;
  socklen_t client_size;

  // Messages can only be 140 characters long:
  char server_buffer[MESSAGE_SIZE];

  // allocate a socket descriptor:
  socketfd = socket(AF_INET, SOCK_STREAM, 0);
  if(DEBUG) printf("A socket file descriptor is nothing but an int. %d\n", socketfd);
  // checking that I obtain the socket file descriptor:
  if(socketfd == -1 ){
    printf("Couldn't create the socket\n");
    exit(1);
  }


  // this is writing zero-valued bytes for the whole server_addr struct -- to clear it out.
  bzero( (char*) &server_addr, sizeof(server_addr) );
  server_addr.sin_family = AF_INET; // IPv4

  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  if ( (gai_result = getaddrinfo(hostname, "51717", &hints, &info)) != 0 ) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));
    exit(1);
  }

  freeaddrinfo(info);

  char ip[100];

  hostname_to_ip(hostname , ip);
  inet_pton(AF_INET, ip, &(server_addr.sin_addr) );
  
  // As talked about in class, we hardcode this port value.
  server_addr.sin_port = htons(51717); // <-- convert to BigEndian

  // binding socket to specified port number:
  if( bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1 ){ // means it failed to bind... 0 on success, -1 if not
    printf("Couldn't bind the port to the socket!\n");
    exit(1);
  }
  // The listen system call allows the process to listen on the socket for connections.
  if(listen(socketfd,1) == -1 ){
    printf("Coudn't listen...\n");
    exit(1);
  }

  printf("Waiting for a connection on %s port %d\n", ip, PORT);


  // Make server listen indefinitely:
  client_size = sizeof(client_addr);
  
  if( (client_file_descriptor = accept(socketfd, (struct sockaddr *) &client_addr, &client_size)) == -1 ){
    printf("Couldn't create the client socket. There was an error on accept.\n");
    exit(1);
  }
  
  while(1){
    
    // writing values of bytes in the buffer to 0
    bzero(server_buffer, MESSAGE_SIZE);
    // Lets recieve the data!
    if( ( rec = recv(client_file_descriptor, server_buffer, sizeof(server_buffer), 0) ) == -1 ){
      printf("Error recieving the data...\n");
      exit(1);
    }
    
    // printing out the data sent from the client:
    printf("Friend: %s", server_buffer);
    // writing back...
    printf("You: ");
    bzero(server_buffer, MESSAGE_SIZE);
    fgets(server_buffer, MESSAGE_SIZE, stdin);
    
    
    if( (sent = send(client_file_descriptor, server_buffer, sizeof(server_buffer), 0) ) == -1){
      printf("Wasn't able to send data... ERROR writing to the socket.\n");
      exit(1);
    }
    
  } // done with while.
  
  close(client_file_descriptor);
  close(socketfd);  
    
}

void client(const int port, const char* ip){
  /* All code for the client */

  int clientSocket, port_number, sent, rec;
  struct sockaddr_in server_addr;
  // struct in_addr inaddr;


  char client_buffer[MESSAGE_SIZE];

  // obtain the port number:
  port_number = port;
  if(DEBUG) printf("The client side, the port is: %d\n", port_number);

  // create the socket:
  clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if(clientSocket == -1){
    printf("Error creating socket..\n");
    exit(1);
  }

  bzero( (char *) &server_addr, sizeof(server_addr) );
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);

  // connecting to the server now..
  if( connect(clientSocket, (struct sockaddr *)&server_addr, sizeof(server_addr) ) == -1 ){
    printf("Error connecting to the server\n");
    exit(1);
  }

  // if connected, display this messgae:
  printf("Connecting to server... Connected!\n");
  printf("Connected to a friend! You send first.\n");


  // chat for an indefinite amount of time:
  while(1){
    // client enters in their message:
    printf("You: ");
    bzero(client_buffer, MESSAGE_SIZE);
    fgets(client_buffer, MESSAGE_SIZE, stdin);
    
    if( ( sent = send(clientSocket, client_buffer, sizeof(client_buffer), 0) ) == -1 ){
      printf("Error sending message to the server...\n");
      exit(1);
    }
    
    bzero(client_buffer, MESSAGE_SIZE);
    
    if( ( rec = recv(clientSocket, client_buffer, MESSAGE_SIZE, 0) ) == -1 ){
      printf("Error recieving the data from the server...\n");
      exit(1);
    }
    
    printf("Friend: %s", client_buffer);
    
  } // end of while.
  
  close(clientSocket);

}

int hostname_to_ip(char * hostname , char* ip){
  struct hostent *he;
  struct in_addr **addr_list;
  int i;

  if ( (he = gethostbyname( hostname ) ) == NULL)
    {
      // get the host info
      herror("gethostbyname");
      return 1;
    }

  addr_list = (struct in_addr **) he->h_addr_list;

  for(i = 0; addr_list[i] != NULL; i++)
    {
      // Return the first one;
      strcpy(ip , inet_ntoa(*addr_list[i]) );
      return 0;
    }

  return 1;
}

void process_cargs(const int argc, char *argv[],  Ip_Port *ipp){
  
  for(int i = 0; i < argc; i++){
    if( strcmp(argv[i], "-p") == 0) ipp->port = atoi(argv[i+1]);
    if( strcmp(argv[i], "-s") == 0) ipp->ip   = argv[i+1];
  }

  // printf("port = %d\n", ipp->port);
  // printf("ip = %s\n",   ipp->ip);
  
}
