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
#include <signal.h> // for the signal() function.

#define DEBUG false
#define MESSAGE_SIZE 140
#define PORT 51717

typedef struct packet_info{
  // length 8 -> 1 for the null byte.
  char contents[8];
  // version:
  short version;
  // String length, 16 bits:
  short string_length;
  // actual message:
  char *message;  
} Packet;

// function prototypes:
void help_message();
void server();
void client(const int port, const char* IP);
int good_port(const int  port); // going to use the struct sockaddr_in --> inet_pton();
int good_ip_addr(const char* ip);
int hostname_to_ip(char * hostname, char *ip);
void process_cargs(const int argc, char *argv[], char *ip, int *port);
// function to handle signals:
void sig_handler(const int signal);
void free_packet(Packet *pkt);
void serialize(Packet *pkt, unsigned char *out_buffer);
void de_serialize(unsigned char *in_buffer, Packet *pkt);

int main(int argc, char *argv[]){


  char *ip = malloc( sizeof(char) * INET_ADDRSTRLEN);
  int port = 0;

  process_cargs(argc, argv, ip, &port );

  switch(argc){
  case 1:
    server();
    break;
  case 2:
    help_message();
    break;
  case 5:
    // Sanity Checking inputs:
    if( !good_port(port) ){
      printf("Please enter a valid port\n");
      exit(1);
    }
    if( !good_ip_addr(ip) ){
      printf("Please enter a valid ip address\n");
      exit(1);
    }
    client(port, ip);
    free(ip);
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
  // use ports withing the range: 1024 - 65535
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

  // function that handles interrupts:
  // signal( SIGINT, sig_handler(client_file_descriptor, socketfd) )


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

    Packet *recv_packet = malloc( sizeof(*recv_packet) );
    Packet *send_packet = malloc( sizeof(*send_packet) );
    recv_packet->message = malloc( sizeof(char)*140 );
    unsigned char recv_buffer[ sizeof(*recv_packet) ];
    unsigned char send_buffer[ sizeof(*recv_packet) ];

    
    if( ( rec = recv(client_file_descriptor, recv_buffer, sizeof(recv_packet), 0) ) == -1 ){
      printf("Error recieving the data...\n");
      exit(1);
    }

    de_serialize(recv_buffer, recv_packet);
    
    // printing out the data sent from the client:
    printf("Friend: %s", recv_packet->message);
    // writing back...
    printf("You: ");

    send_packet->version = 457;    
    char *input_string = fgets(send_packet->message, MESSAGE_SIZE, stdin);
    // printf("Length of the string is: %lu\n", strlen(input_string) );
    send_packet->string_length = (int) strlen(input_string);
    // create buffer to send over:    

    serialize(send_packet, send_buffer);
    
    if( (sent = send(client_file_descriptor, send_buffer, sizeof(send_buffer), 0) ) == -1){
      printf("Wasn't able to send data... ERROR writing to the socket.\n");
      exit(1);
    }

    free_packet( recv_packet );
    free_packet( send_packet );

  } // done with while.

  close(client_file_descriptor);
  close(socketfd);

}

void client(const int port, const char* ip){
  /* All code for the client */

  int clientSocket, port_number, sent, rec;
  struct sockaddr_in server_addr;

  // obtain the port number:
  port_number = port;
  if(DEBUG) printf("The client side, the port is: %d\n", port_number);
  if(DEBUG) printf("The client side, the port is: %s\n", ip);

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

    // create Packet then initialize it:
    Packet *client_packet =  malloc( sizeof(*client_packet) );
    Packet *recv_packet   = malloc( sizeof(*recv_packet) );
    client_packet->message = malloc( sizeof(char) * 140 );
    strcpy(client_packet->contents, "-s-s-cp");
    client_packet->version = 457;
    
    char *input_string = fgets(client_packet->message, MESSAGE_SIZE, stdin);
    // printf("Length of the string is: %lu\n", strlen(input_string) );
    client_packet->string_length = (int) strlen(input_string);
    // create buffer to send over:
    unsigned char send_buffer[ sizeof(*client_packet) ];
    unsigned char recv_buffer[ sizeof(*client_packet) ];
    
    serialize(client_packet, send_buffer);
    
    if( ( sent = send(clientSocket, send_buffer, sizeof(send_buffer), 0) ) == -1 ){
      printf("Error sending message to the server...\n");
      exit(1);
    }
    
    free_packet(client_packet);
    
    if( ( rec = recv(clientSocket, recv_buffer , sizeof(recv_buffer), 0) ) == -1 ){
      printf("Error recieving the data from the server...\n");
      exit(1);
    }

    de_serialize(recv_buffer, recv_packet);
    
    printf("Friend: %s", recv_packet->message);

    free_packet( recv_packet );
    
  } // end of while.

  close(clientSocket);

}

int hostname_to_ip(char * hostname , char* ip){
  struct hostent *he;
  struct in_addr **addr_list;
  int i;

  if ( (he = gethostbyname( hostname ) ) == NULL){
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

void process_cargs(const int argc, char *argv[], char *ip, int *port){

  for(int i = 0; i < argc; i++){
    if( strcmp(argv[i], "-p") == 0) *port = atoi(argv[i+1]);
    if( strcmp(argv[i], "-s") == 0) strcpy(ip, argv[i+1] );
  }
  
  // printf("port = %d\n", *port);
  // printf("ip = %s\n",   ip);

}

void free_packet(Packet *pkt){
  // free anything in struct and struct itself:
  free(pkt->message);
  free(pkt);
}

void serialize(Packet *pkt, unsigned char *out_buffer){

  // printf("here is the string: %s\n", pkt->contents);

  // essentially, C's substr...
  char subbuffer[3];
  memcpy( subbuffer, &pkt->contents[0] , 2);
  subbuffer[2] = '\0';
  // printf("subbuffer = %s\n", subbuffer);

  char subbuffer2[3];
  memcpy( subbuffer2, &pkt->contents[2] , 2);
  subbuffer2[2] = '\0';
  // printf("subbuffer2 = %s\n", subbuffer2);

  char subbuffer3[4];
  memcpy( subbuffer3, &pkt->contents[3], 3);
  subbuffer3[3] = '\0';
  // printf("subbuffer3 = %s\n", subbuffer3);

  if( strcmp(subbuffer, "-s") ) memcpy( out_buffer, &pkt->version, sizeof(pkt->version) );
  if( strcmp(subbuffer2,"-s") ) memcpy( (out_buffer + sizeof(pkt->version)) , &pkt->string_length, sizeof(pkt->string_length) );
  if( strcmp(subbuffer3, "-cp") ) memcpy( (out_buffer + sizeof(pkt->version) + sizeof(pkt->string_length)) , pkt->message, sizeof(pkt->string_length) );  
}


void de_serialize(unsigned char *in_buffer, Packet *pkt){

  // other way around:
  memcpy( &pkt->version, &in_buffer[2], (sizeof(char)*2) );
  memcpy( &pkt->string_length, &in_buffer[4], (sizeof(char)*2) );
  memcpy( &pkt->message, &in_buffer[8], (sizeof(char)*8) );  
  
}
