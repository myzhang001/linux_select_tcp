#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

int main(int argc, char* argv[]) {
  if (2 != argc) {
    printf("Usage: %s <port of serve>\n", argv[0]);
    return -1;
  }

  int len, rc, on = 1;
  int listen_sd, max_sd, new_sd;
  int desc_ready, end_server = FALSE;
  int close_conn;
  char buffer[80];
  struct sockaddr_in addr;
  struct timeval timeout;
  fd_set master_set, working_set;

  listen_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sd < 0) {
    perror("socket() falied\n");
    return -1;
  }

  rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR,
                  (char*)&on, sizeof(on));
  if (rc < 0) {
    perror("setsockopt() failed\n");
    close(listen_sd);
    return -1;
  }
  
  // 这里设置为非阻塞的listen模式，这样accpet函数在
  // 接收不到连接的时候，不会发生阻塞。
  rc = ioctl(listen_sd, FIONBIO, (char*)&on);
  if (rc < 0) {
    perror("ioctl() failed\n");
    close(listen_sd);
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  int port = atoi(argv[1]);
  if (port <= 1024) {
    perror("port error\n");
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  rc = bind(listen_sd, (struct sockaddr*)&addr, sizeof(addr));
  if (rc < 0) {
    perror("bind() error\n");
    close(listen_sd);
    return -1;
  }

  rc = listen(listen_sd, 32);
  if (rc < 0) {
    perror("listen() error\n");
    close(listen_sd);
    return -1;
  }

  FD_ZERO(&master_set);
  max_sd = listen_sd;
  FD_SET(listen_sd, &master_set);

  timeout.tv_sec = 3 * 60; // 3分钟的时间
  timeout.tv_usec = 0;

  do {
    memcpy(&working_set, &master_set, sizeof(master_set));
    printf("Waiting on select()...\n");
    rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);
    
    if (rc < 0) {
      perror("select() failed\n");
      break;
    }
    if (rc == 0) {
      printf("select() timed out. End program.\n");
      break;
    }

    desc_ready = rc;  // 就绪的个数
    for (int i = 0; i <= max_sd && desc_ready > 0; ++i) {
      if (FD_ISSET(i, &working_set)) {
        desc_ready -= 1;
        if(i == listen_sd) {  // 有新的连接到来
          printf("Listening socket is readable.\n");
          do {  // 这里的死循环是为了接收完监听队列中所有的连接
            new_sd = accept(listen_sd, NULL, NULL);
            if (new_sd < 0) {
              if (errno != EWOULDBLOCK) {
                perror("accept() failed.\n");
                end_server = TRUE;
              }
              break;
            }

            printf("New coming connection - %d\n", new_sd);
            FD_SET(new_sd, &master_set);  // 新连接加入任务队列，注意是master
            if(new_sd > max_sd) {  // 更新最大的fd
              max_sd = new_sd;
            }
          } while(new_sd != -1);
        } else {  // 已经建立的连接收到数据
          printf("Descriptor %d readable\n", i);
          close_conn = FALSE;

          do {
            // 这里处理接收到客户端的信息，死循环是为了接收完所有可能的数据
            // 注意这里，recv本身是一个阻塞的函数，所以只要客户端不主动关闭连接，
            // 那么服务器会一直阻塞在这里，又因为使用了while(TRUE)方式循环接收，
            // 因此出现了如果使用多个客户端进行连接，只有当前面的关闭连接后，
            // 后面的才会收到数据。在高性能的服务器编程中，客户端的连接应该使用
            // 多线程或者多进程的方式处理。如果资源充足，应该给每个客户端一个进程
            // 或者线程，当然这样可能也会出现资源不足的情况。更好的方式是多线程(进程)结合
            // 心跳检测机制，把下面的send发送数据替换成心跳函数。如果收不到心跳，
            // 就认定已经断线，此时把客户端的连接剔除即可。本例子中客户端主动断开
            // 连接也会被剔除，因为send函数收不到回复了。
            // 当然，这个例子只是一个示范select的作用，没有那么复杂。
            rc = recv(i, buffer, sizeof(buffer), 0);
            if (rc < 0) {
              if (errno != EWOULDBLOCK) {
                perror("recv() failed");
                close_conn = TRUE;
              }
              break;
            }
            if (rc == 0) {
              printf("Connection closed\n");
              close_conn = TRUE;
              break;
            }

            len = rc;
            printf("%d bytes received\n", len);
            rc = send(i, buffer, len, 0);  // 在这里把客户端的数据重新发回去
            if (rc < 0) {
              perror("send() failed");
              close_conn = TRUE;
              break;
            }
          } while(TRUE);

          if (close_conn) {
            close(i);
            FD_CLR(i, &master_set);
            if (i == max_sd) {
              // 在这里循环关掉所有的未连接socket
              while (FD_ISSET(max_sd, &master_set) == FALSE) {
                max_sd -= 1;
              }
            }
          }
        }
      }
    }
  } while(end_server == FALSE);

  // 关闭所有连接
  for (int i = 0; i <= max_sd; ++i) {
    if (FD_ISSET(i, &master_set)) {
      close(i);
    }
  }

  return 0;
}
