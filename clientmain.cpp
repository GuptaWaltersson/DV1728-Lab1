#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
//#define DEBUG

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include "protocol.h"
#include <errno.h>

// Included to get the support library
#include <calcLib.h>


bool find_connect(char* protocol, int& sockfd, char* Desthost, char* Destport)
{
  
  struct addrinfo filter;
  struct addrinfo* result, *info;
  memset(&filter,0,sizeof(filter));

  filter.ai_family = AF_UNSPEC;

  if (strcmp(protocol,"tcp") == 0)
  {
    filter.ai_socktype = SOCK_STREAM;
    
  }
  else if(strcmp(protocol,"udp")==0)
  {
    filter.ai_socktype = SOCK_DGRAM;
  }
  else if (strcmp(protocol,"any")==0)
  {

  }
  else
  {
    return false;
  }
  int err;
  if((err = getaddrinfo(Desthost,Destport,&filter,&result))!= 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    return false;
  }
  #ifdef DEBUG
  printf("getaddrinfo succeded: %s\n",Desthost);
  #endif
  
  for(info = result; info != NULL; info = info->ai_next)
  {
    sockfd=socket(info->ai_family,info->ai_socktype,info->ai_protocol);
    if(sockfd !=-1)
    {
      #ifdef DEBUG
      printf("found socket\n");
      #endif
      break;
    }
  }
  struct timeval tv;
  tv.tv_sec = 5;   // 5 seconds
  tv.tv_usec = 0;

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

  connect(sockfd,info->ai_addr,info->ai_addrlen);
  #ifdef DEBUG
  printf("connected to socket\n");
  #endif
  freeaddrinfo(result);
  return true;
}

char* calculator_helper(const char* msg,char* answer,int buffer_size,calcProtocol cp)
{
  uint32_t arith = ntohl(cp.arith);
  printf("ASSIGNMENT: ");
  int result=0;
  uint32_t num1, num2;
  char word[32];
  if(arith != 0)
  {
    num1 = ntohl(cp.inValue1);
    num2 = ntohl(cp.inValue2);
  }
  else if ((sscanf(msg,"%31s %d %d",word,&num1,&num2))!= 3)
  {
    printf("did not parse three things");
    return nullptr;
  }

  if(strstr(msg,"add")!=NULL || arith == 1)
  {
    printf("add %d %d\n",num1,num2);
    result = num1+num2;
  }
  else if(strstr(msg,"sub")!=NULL || arith == 2)
  {
    printf("sub %d %d\n",num1,num2);

    result = num1-num2;
  }
  else if(strstr(msg,"mul")!=NULL || arith == 3)
  {
    printf("mul %d %d\n",num1,num2);

    result = num1*num2;
  }
  else if(strstr(msg,"div")!=NULL || arith == 4)
  {
    printf("div %d %d\n",num1,num2);

    result = num1/num2;
  }
  #ifdef DEBUG
  printf("result is :%d\n",result);
  #endif
  snprintf(answer,buffer_size,"%d",result);
  
  return answer;
}

void receveive_helper(char* buffer,int sockfd,size_t buffer_size)
{
  int numbytes;
  if((numbytes=recv(sockfd,buffer,buffer_size,0)) <1 )
  {
    printf("ERROR: Timeout");
    buffer[0] = '\0';
    return;
  }
  buffer[numbytes] = '\0';
  #ifdef DEBUG
  printf("received (%d bytes): %s\n", numbytes, buffer);
  #endif
  
}

void receive_binary( int sockfd,calcProtocol* cp)
{
  ssize_t numbytes = recv(sockfd, cp, sizeof(*cp), 0);
  if (numbytes <= 0) {
    perror("recv");
    return;
  }
  if (numbytes < sizeof(*cp)) {
    fprintf(stderr, "incomplete calcProtocol received (%zd bytes)\n", numbytes);
    return;
  }

#ifdef DEBUG
    printf("received a calcProtocol (%zd bytes)\n", numbytes);
#endif
  
}

void send_helper(const char* msg, int sockfd)
{
  #ifdef DEBUG
    printf("trying to send\n");
  #endif
  ssize_t bytes_sent = send(sockfd, msg, strlen(msg), 0);
  if(bytes_sent == -1)
  {
    printf("nothing to send\n");
    return;
  }
  #ifdef DEBUG
    printf("sent %zd bytes: %s\n", bytes_sent, msg);
  #endif
}

void TCP_text(char* Desthost, char* Destport, int sockfd)
{
  #ifdef DEBUG
    printf("tcp text\n");
  #endif
  int buffer_size=1024;
  char recv_buffer[buffer_size];
  receveive_helper(recv_buffer,sockfd,buffer_size-1);
  if(strstr(recv_buffer,"TEXT TCP 1.1")!= NULL)
  {
    const char* msg = "TEXT TCP 1.1 OK\n";
    send_helper(msg,sockfd);
  }
  else{
    printf("ERROR: MISSMATCH PROTOCOL\n");
    close(sockfd);
    return;
  }
  

  receveive_helper(recv_buffer,sockfd,buffer_size-1);
  //printf("ASSIGNMENT: %s",recv_buffer);
  char text[64];
  calcProtocol cp;
  memset(&cp,0,sizeof(cp));
  
  calculator_helper(recv_buffer,text,sizeof(text),cp);
  strcat(text,"\n");
  send_helper(text,sockfd);
  
  receveive_helper(recv_buffer,sockfd,buffer_size-1);
  if(strstr(recv_buffer,"OK")!=NULL)
  {
    int number = atoi(text);
    printf("OK (myresult=%d)\n",number);
  }

  close(sockfd);
}

void TCP_binary(char* Desthost, char* Destport, int sockfd)
{
  #ifdef DEBUG
    printf("tcp binary\n");
  #endif

  int buffer_size = 1024;
  char recv_buffer[buffer_size];
  receveive_helper(recv_buffer,sockfd,buffer_size-1);
  if(strstr(recv_buffer,"BINARY TCP 1.1")== NULL)
  {
    printf("ERROR: MISSMATCH PROTOCOL\n");
    close(sockfd);
    return;
  }

  const char* msg = "BINARY TCP 1.1 OK\n";
  send_helper(msg,sockfd);

  calcProtocol cp;
  receive_binary(sockfd,&cp);
  
  uint32_t num1 = ntohl(cp.inValue1);
  uint32_t num2 = ntohl(cp.inValue2);
  #ifdef DEBUG
  printf("num1: %d, num2: %d\n",num1,num2);
  #endif
  char ansbuf[32];
  calculator_helper(msg,ansbuf,32,cp);

  calcProtocol resp_cp;
  
  resp_cp.type = htons(22);
  resp_cp.major_version = htons(1);
  resp_cp.minor_version = htons(1);
  resp_cp.id = htonl(ntohl(cp.id));
  resp_cp.inResult = htonl(atoi(ansbuf));
  
  send(sockfd,&resp_cp,sizeof(resp_cp),0);
  calcMessage ans_cp;
  ssize_t numbytes = recv(sockfd, &ans_cp, sizeof(ans_cp), 0);
  if(numbytes < 1)
  {
    printf("ERROR TIMEOUT");
    return;
  }
  uint32_t ans = ntohl(ans_cp.message);
  if(ans == 1)
  {
    printf("OK (myresult=%d)\n",atoi(ansbuf));
  }
  close(sockfd);
}

void UDP_text(char* Desthost, char* Destport, int sockfd)
{

}

void UDP_binary(char* Desthost, char* Destport,int sockfd)
{

}


int main(int argc, char *argv[]){
  
  
  
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
    
  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string (Desthost) and one integer (port). 
     Atm, works only on dotted notation, i.e. IPv4 and DNS. IPv6 does not work if its using ':'. 
  */
    char protocolstring[6], hoststring[2000],portstring[6], pathstring[7];

    char *input = argv[1];
    
    /* Some error checks on string before processing */
    // Check for more than two consequtive slashes '///'.

    if (strstr(input, "///") != NULL ){
      printf("Invalid format: %s.\n", input);
      return 1;
    }
    

    // Find the position of "://"
    char *proto_end = strstr(input, "://");
    if (!proto_end) {
        printf("Invalid format: missing '://'\n");
        return 1;
    }

     // Extract protocol
    size_t proto_len = proto_end - input;
    if (proto_len >= sizeof(protocolstring)) {
        fprintf(stderr, "Error: Protocol string too long\n");
        return 1;
    }
    
    // Copy protocol
    strncpy(protocolstring, input, proto_end - input);
    protocolstring[proto_end - input] = '\0';

    // Move past "://"
    char *host_start = proto_end + 3;

    // Find the position of ":"
    char *port_start = strchr(host_start, ':');
    if (!port_start || port_start == host_start) {
	printf("Error: Port is missing or ':' is misplaced\n");
        return 1;
    }

    // Extract host
    size_t host_len = port_start - host_start;
    if (host_len >= sizeof(hoststring)) {
        printf("Error: Host string too long\n");
        return 1;
    }
    
    // Copy host
    strncpy(hoststring, host_start, port_start - host_start);
    hoststring[port_start - host_start] = '\0';

        // Find '/' which starts the path
    char *path_start = strchr(host_start, '/');
    if (!path_start || *(path_start + 1) == '\0') {
        fprintf(stderr, "Error: Path is missing or invalid\n");
        return 1;
    }

    // Extract path
    if (strlen(path_start + 1) >= sizeof(pathstring)) {
        fprintf(stderr, "Error: Path string too long\n");
        return 1;
    }
    strcpy(pathstring, path_start + 1);

    // Extract port


    size_t port_len = path_start - port_start - 1;
    if (port_len >= sizeof(portstring)) {
        fprintf(stderr, "Error: Port string too long\n");
        return 1;
    }
    strncpy(portstring, port_start + 1, port_len);
    portstring[port_len] = '\0';

    // Validate port is numeric
    for (size_t i = 0; i < strlen(portstring); ++i) {
        if (portstring[i] < '0' || portstring[i] > '9') {
            fprintf(stderr, "Error: Port must be numeric\n");
            return 1;
        }
    }


    
    char *protocol, *Desthost, *Destport, *Destpath;
    protocol=protocolstring;
    Desthost=hoststring;
    Destport=portstring;
    Destpath=pathstring;
      
  // *Desthost now points to a sting holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter. 
	// TEST MOTHA FUCKA

    
  /* Do magic */
  


  initCalcLib();
  int port;
  try{ port=atoi(Destport);
  } catch (...){
    printf("Error: Invalid Port format");
  }

  
  if (port <= 1 or port >65535) {
    printf("Error: Port is out of server scope.\n");
    if ( port > 65535 ) {
      printf("Error: Port is not a valid UDP or TCP port.\n");
    }
    return 1;
  }
  
  bool connected;
  int sockfd=0;
  connected = find_connect(protocol,sockfd,Desthost,Destport);
  if(!connected)
  {
    printf("could not connect");
    return -1;
  }
  
  if(!strcmp(protocol,"tcp") || !strcmp(protocol,"TCP"))
  {
    if (!strcmp(Destpath,"text")|| !strcmp(Destpath,"TEXT"))
    {
      TCP_text(Desthost,Destport,sockfd);
    }
    else if (!strcmp(Destpath,"binary")||!strcmp(Destpath,"BINARY"))
    {
      TCP_binary(Desthost,Destport,sockfd);
    }
  }
  else if(!strcmp(protocol,"udp") || !strcmp(protocol,"UDP"))
  {
    
    if (!strcmp(Destpath,"text") || !strcmp(Destpath,"TEXT"))
    {
      //UDP_text();
    }
    else if (!strcmp(Destpath,"binary") || !strcmp(Destpath,"BINARY"))
    {
      //UDP_binary();
    }
  }
  else if (strcmp(protocol,"any")==0 || strcmp(protocol,"ANY")==0)
  {

  }
  else
  {
    printf("Nope");
  }
#ifdef DEBUG 
  printf("Protocol: %s Host %s, port = %d and path = %s.\n",protocol, Desthost,port, Destpath);
#endif


  
}

