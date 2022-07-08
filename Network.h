/**
 * The network class creates a udp connections to
 * the server, and provides for sending and receiving.
 */
class Network
{
 private:
  char *hostname;
  char *port;
  int sockfd;
  bool notopen;
  struct sockaddr skull_addr;
  socklen_t skull_addr_len;
  
 public:
  Network();
  bool setupListener(const char *hostname, const char *port);
  int receive(char *buf, int buflen, long int timeout);
  bool transmit(const char *msg , int msgLen);
  bool terminate();
};
