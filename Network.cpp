// UDP Client Example in C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#include "Network.h"
#define PORT "5000"
#define LISTEN_BACKLOG 50
#define ADDRSTRLEN 1024

#define BUFFLEN 1024
char buffer[BUFFLEN];

/**
 * Generic intializer
 */
Network::Network()
{
  hostname = nullptr;
  port     = nullptr;
  sockfd   = -1;
}


/**
 * INITIALIZE THE SERVER INFO, GET READY TO SEND OR RECEIVE
 */
bool Network::setupListener(const char *hostname, const char * port)
{
  struct addrinfo hints;
  struct addrinfo *result;
  struct sockaddr servsock;
  socklen_t servsock_len;

  int status;
  
  // FIND HOST NAME...   AF_INET family
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_CANONNAME|AI_ADDRCONFIG;    /* For wildcard IP address. Do we want AI_NUMERICHOST? */
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Only datagram */
  hints. ai_protocol = 0;         // Any protocol
  //hints.ai_addrlen  =  ???? ;
  //hints.ai_addr     = ?????;
  //hints.ai_cannonname = ????;
  // ai_next = nullptr;
    
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */


    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
    
  // All others zero or null
    if (0 != (status =getaddrinfo(hostname, port, &hints, &result)))
      {
	perror("gethostbyname:");
	return EXIT_FAILURE;
      }

    int optval;
    // Now set up the socket and bind to it...
    // (Note: this version only uses the first entry returned by getaddrinfo)
    sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if(sockfd == -1)
      {
	perror("socket");
      }
    
    // re-use socket...
    if (0 != setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
      {
	perror("setsockopt");
	return EXIT_FAILURE;
      }

    
    // Now bind to it!
    if (0 != bind( sockfd, result->ai_addr, result->ai_addrlen))
      {
	perror("bind:");
	return(EXIT_FAILURE);
      }
	       
    freeaddrinfo(result);

    return(true);
}


/**
 * SEND a Packet to SkullDougery
 * @param msg - pointer to the message to send.
 * @param msgLen - number of bytes in message. If 0, use strlen(msg).
 * @return - true normally, false if socket is not active.
 */
bool Network::transmit(const char *msg, int msgLen)
{
  if (sockfd < 0) return(false);
  msgLen = (msgLen == 0)? strlen(msg) : msgLen;
  char *buf = (char *)malloc(msgLen+1);
  memcpy(buf, msg, msgLen);
  
  ssize_t totalBytesSent=0;
  while(totalBytesSent < msgLen)
    {
      ssize_t bytes = sendto( sockfd, msg, msgLen,'\0', &skull_addr, skull_addr_len);
      if (bytes<=0)
	{
	  terminate();
	  return(false);
	}
      
      if (bytes != msgLen)
	{
	  totalBytesSent += bytes;
	  memcpy(buf, buf+bytes, msgLen - totalBytesSent);
	}
    }
  free(buf);
  return(false);
}


/**
 * RECEIVE a Packet from SkullDougery
 * @param buf - pointer to a caller-supplied bufffer to put the data into
 * @param buflen - max space in buf
 * @param timeout - how long to wait - 0 means no-wait.
 * @return  number of bytes in buffer. 0 means no record available.
 */

int Network::receive(char *buf, int buflen, long int timeout)
  {
    int retval;
    if (sockfd<0) return(false);
    retval=recvfrom(sockfd, buf, sizeof(buf), MSG_DONTWAIT, &skull_addr, &skull_addr_len);
    return(retval);
  }


/**
 * TERMINATE the current connection
 *   (i.e.: close the connection)
 */
bool Network::terminate()
{
  // TODO:
  close(sockfd);
  sockfd=-1;
  return(false);
}
