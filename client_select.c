#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#define STACK_SIZE					50000


int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: %s <ip of server> <port of server>\n", argv[0]);
    return -1;
  }

  int port = 0;
  int socketfd = 0;
  struct sockaddr_in serv_addr;
  char buffer[80];

  bzero(&serv_addr, sizeof(serv_addr));
  bzero(buffer, sizeof(buffer));
  
  if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr) < 0) {
    printf("IP error\n");
    return -1;
  }
  
  port = atoi(argv[2]);
  if (port <= 1024) {
    printf("Port error\n");
    return -1;
  }

  serv_addr.sin_port = htons(port);
  serv_addr.sin_family = AF_INET;

  socketfd = socket(AF_INET, SOCK_STREAM, 0);
  if (socketfd < 0) {
    printf("socket() error");
    return -1;
  }

  if (connect(socketfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("connect() error\n");
    return -1;
  }
  
  int i = 0;
  int rc = 0;
  while (1) {
    sprintf(buffer, "%dth message\n", i);
    ++i;
    rc = send(socketfd, buffer, sizeof(buffer), 0);
    if (rc < 0) {
      printf("send() error\n");
      return -1;
    } else if (rc == 0) {
      printf("send nothing to server\n");
      return -1;
    } else {
      printf("send successfully\n");
    }

    rc = recv(socketfd, buffer, sizeof(buffer), 0);
    if (rc < 0) {
      printf("recv() error\n");
      return -1;
    } else if (rc == 0) {
      printf("receive nothing from server\n");
      return -1;
    } else {
      printf("received data: %s", buffer);
    }

    sleep(2);
  }

  return 0;
}
