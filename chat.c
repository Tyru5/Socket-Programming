// Project 1
// Author: Tyrus Malmstrom
// Class: cs457
// Date: 8/31/2016
// 'Simple' chat program

// directives:
#include <stdio.h>
#include <stdlib.h> //  exit();
#include <string.h>
#include <strings.h> // bzero(); && bcopy();
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // struct sockaddr_in is defined in.
#include <arpa/inet.h> // inet_pton();
#include <unistd.h> // for closing file descriptor
#include <netdb.h>
#include <signal.h> // for the signal(); function.

#define DEBUG false
#define PORT 51717

// Packet Structure:
typedef struct packet_info{
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
int  good_port(const int  port); // going to use the struct sockaddr_in --> inet_pton();
int  good_ip_addr(const char* ip);
int  hostname_to_ip(char * hostname, char *ip);
void process_cargs(const int argc, char *argv[], char *ip, int *port);
void free_packet(Packet *pkt);
void serialize(Packet pkt, char *out_buffer);
void de_serialize(char *in_buffer, Packet *pkt);
void sig_handler(int signal);
void create_packet(Packet *pkt, char *input_buf);
int verify_input(const int input_length);
void free_packet(Packet *pkt);

// globals:
// sockets:
int socketfd, client_socket, client_file_descriptor;
char *ip;

int main(int argc, char *argv[]){


  ip = malloc( sizeof(char) * INET_ADDRSTRLEN);
  int port = 0;

  process_cargs(argc, argv, ip, &port );

  // calling / defining the signal function:
  signal(SIGINT, sig_handler);
  
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
  printf("Welcome to Chat!\n");

  struct sockaddr_in server_addr, client_addr;
  struct addrinfo hints, *info;

  int rec, sent, getaddrinfo_res;
  socklen_t client_size;

  // allocate a socket descriptor:
  if( (socketfd = socket(AF_INET, SOCK_STREAM, 0) ) == -1 ){
    printf("Couldn't create the socket\n");
    exit(1);
  }

  // printf("first socket = %d\n", socketfd);
  
  bzero( (char*) &server_addr, sizeof(server_addr) );
  server_addr.sin_family = AF_INET;

  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  if ( (getaddrinfo_res = getaddrinfo(hostname, "51717", &hints, &info)) != 0 ) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_res));
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

    Packet recv_packet;
    Packet send_packet;

    char message_buffer[145];
    message_buffer[144] = '\0';
    char recv_buffer[141];
    recv_buffer[140] = '\0';
    char send_buffer[141];
    send_buffer[140] = '\0';
    bzero(message_buffer, sizeof(message_buffer) );
    bzero(recv_buffer, sizeof(recv_buffer) );
    bzero(send_buffer, sizeof(send_buffer) );
    
    if( ( rec = recv(client_file_descriptor, recv_buffer, sizeof(recv_buffer), 0) ) == -1 ){
      printf("Error recieving the data...\n");
      exit(1);
    }

    de_serialize(recv_buffer, &recv_packet);

    // printing out the data sent from the client:
    printf("Friend: %s", recv_packet.message);
    // writing back...
    printf("You: ");

    do{
      char *strang = fgets(message_buffer, 145, stdin);
      // initialize packet:
      create_packet(&send_packet, strang);
    }while( verify_input( send_packet.string_length ) );
    
    // serializing packet:
    serialize(send_packet, send_buffer);
    
    if( (sent = send(client_file_descriptor, send_buffer, sizeof(send_buffer), 0) ) == -1){
      printf("Wasn't able to send data... ERROR writing to the socket.\n");
      exit(1);
    }

    free_packet(&send_packet);
    free_packet(&recv_packet);
    
  } // done with while.

}

void client(const int port, const char* ip){

  int port_number, sent, rec;
  struct sockaddr_in server_addr;

  // obtain the port number:
  port_number = port;
  if(DEBUG) printf("The client side, the port is: %d\n", port_number);
  if(DEBUG) printf("The client side, the port is: %s\n", ip);

  // create the socket:
  if( (client_socket = socket(AF_INET, SOCK_STREAM, 0) ) == -1){
    printf("Error creating socket...\n");
    exit(1);
  }

  // printf("second socket = %d\n", client_socket);
  
  bzero( (char *) &server_addr, sizeof(server_addr) );
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port); // <-- convert to BigEndian.

  // connecting to the server now...
  if( connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr) ) == -1 ){
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

    // create Packet:
    Packet send_packet, recv_packet;
    
    char message_buffer[145];
    message_buffer[144] = '\0';
    char send_buffer[141];
    send_buffer[140] = '\0';
    char recv_buffer[141];
    recv_buffer[140] = '\0';
    bzero(message_buffer, sizeof(message_buffer) );
    bzero(recv_buffer, sizeof(recv_buffer) );
    bzero(send_buffer, sizeof(send_buffer) );
    
    
    do{
      char *strang = fgets(message_buffer, 145 , stdin);
      // initialize packet:
      create_packet(&send_packet, strang);
    }while( verify_input( send_packet.string_length ) );
    
    // serialize packet before sending over:
    serialize(send_packet, send_buffer);

    if( ( sent = send(client_socket, send_buffer, sizeof(send_buffer), 0) ) == -1 ){
      printf("Error sending message to the server...\n");
      exit(1);
    }
    
    if( ( rec = recv(client_socket, recv_buffer , sizeof(recv_buffer), 0) ) == -1 ){
      printf("Error recieving the data from the server...\n");
      exit(1);
    }

    de_serialize(recv_buffer, &recv_packet);
    
    printf("Friend: %s", recv_packet.message);

    free_packet(&send_packet);
    free_packet(&recv_packet);
    
  } // end of while.

}

int hostname_to_ip(char * hostname , char* ip){
  // using the hostent structure to find out what the hostname of the machine is:
  struct hostent *he;
  struct in_addr **addr_list;
  int i;

  if ( (he = gethostbyname( hostname ) ) == NULL){
    // get the host info
    printf("Couldn't retrive the hostname from the machine...\n");
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

  int i;
  for(i = 0; i < argc; i++){
    if( strcmp(argv[i], "-p") == 0) *port = atoi(argv[i+1]);
    if( strcmp(argv[i], "-s") == 0) strcpy(ip, argv[i+1] );
  }
  
  // printf("port = %d\n", *port);
  // printf("ip = %s\n",   ip);

}

int verify_input(const int input_length){
  // printf("input_length = %d\n", input_length);
  if( input_length > 140 ){
    printf("Error: Input too long.\n");
    return 1;
  }
  return 0;
}

void create_packet(Packet *pkt, char *input_buf){

  // setting version:
  pkt->version = 457;
  // setting string_length:
  pkt->string_length = (int) strlen(input_buf);
  pkt->message = malloc( pkt->string_length );
  // copying string into message field:
  strcpy(pkt->message, input_buf);
  
}

void free_packet(Packet *pkt){
  // free contents of packet:
  free(pkt->message);
}

void serialize(Packet pkt, char *out_buffer){
  
  memcpy(  out_buffer, &(pkt.version) , sizeof(pkt.version) );
  memcpy( (out_buffer + sizeof(pkt.version)) , &(pkt.string_length), sizeof(pkt.string_length) );
  memcpy( (out_buffer + sizeof(pkt.version) + sizeof(pkt.string_length)) , pkt.message, pkt.string_length );

  if(DEBUG){
    short v = 0;
    short l = 0;
    char *res = malloc(pkt.string_length +2 +2);
    
    memcpy(&v, out_buffer, 2);
    memcpy(&l, out_buffer+2, 2);
    memcpy( res, out_buffer+4, pkt.string_length);
    
    printf("v = %d\n", v);
    printf("l = %d\n", l);
    printf("string = %s\n", res);
    
    printf("at the end\n");
    
    free(res);
  }
  
}

void de_serialize(char *in_buffer, Packet *pkt){
  
  // other way around:
  memcpy( &(pkt->version), in_buffer, sizeof(short) );
  memcpy( &(pkt->string_length), (sizeof(short) + in_buffer) , sizeof(short) );
  pkt->message = malloc( pkt->string_length );
  memcpy( pkt->message, ( (sizeof(short) * 2) +  in_buffer), pkt->string_length );
  
}

void sig_handler(int signal){ // this isn't working...

  printf("\nClosing all Sockets...\n");  
  close( socketfd );
  close( client_file_descriptor );
  close( client_socket );
  free(ip);
  exit(0);
  
}
