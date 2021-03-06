/*使用select函数可以以非阻塞的方式和多个socket通信。程序只是演示select函数的使用，即使某个连接关闭以后也不会修改当前连接数，连接数达到最大值后会终止程序。
1. 程序使用了一个数组fd，通信开始后把需要通信的多个socket描述符都放入此数组
2. 首先生成一个叫sock_fd的socket描述符，用于监听端口。
3. 将sock_fd和数组fd中不为0的描述符放入select将检查的集合fdsr。
4. 处理fdsr中可以接收数据的连接。如果是sock_fd，表明有新连接加入，将新加入连接的socket描述符放置到fd。
5. 添加新的fd 到数组中 判断有效的连接数是否小于最大的连接数，如果小于的话，就把新的连接套接字加入集合
6. 线程是使用的分离线程方式，程序运行结束后即可以释放资源不用单独释放
 */ 
// select_server.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <asm/types.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/sysinfo.h>
#include <linux/netlink.h>
 
#define MYPORT 1234 //连接时使用的端口
#define MAXCLINE 2 //连接队列中的个数
#define BUF_SIZE 200
 
int fd[MAXCLINE]; //连接的fd
int conn_amount; //当前的连接数

#define STACK_SIZE					50000
 
typedef struct _server_param{
	int clientfd;
	int clientip;
	char mac[18];
}ServerParam;


void *commonclient_pth(void *data);

void showclient()
{
    int i;
    printf("client amount:%d\n",conn_amount);
    for(i=0;i<MAXCLINE;i++)
    {
        printf("[%d]:%d ",i,fd[i]);
    }
    printf("\n\n");
}

int from_socket_get_mac( int sock_fd, char *mac, const char* net_card_name ) 
{ 
    struct arpreq arpreq; 
    struct sockaddr_in dstadd_in; 
    socklen_t  len = sizeof( struct sockaddr_in ); 
    memset( &arpreq, 0, sizeof( struct arpreq )); 
    memset( &dstadd_in, 0, sizeof( struct sockaddr_in ));  
    if( getpeername( sock_fd, (struct sockaddr*)&dstadd_in, &len ) < 0 ) 
    { 
       //perror( "get peer name wrong, %s/n", strerror(errno)); 
        return -1; 
    } 
    else 
    { 
        memcpy( ( struct sockaddr_in * )&arpreq.arp_pa, ( struct sockaddr_in * )&dstadd_in, sizeof( struct sockaddr_in )); 
        strcpy(arpreq.arp_dev, net_card_name); 
        arpreq.arp_pa.sa_family = AF_INET; 
        arpreq.arp_ha.sa_family = AF_UNSPEC; 
        if( ioctl( sock_fd, SIOCGARP, &arpreq ) < 0 ) 
        { 
            //perror( "ioctl SIOCGARP wrong, %s/n", strerror(errno) ); 
            printf("ioctl SIOCGARP wrong");
            return -1; 
        } 
        else 
        { 
            unsigned char* ptr = (unsigned char *)arpreq.arp_ha.sa_data; 
			memcpy(mac,ptr,6);
		} 
     } 
     return 0; 
}

int getpeermac(int sockfd, char *buf) 
{
	struct arpreq arpreq; 
	struct sockaddr_in dstadd_in; 
	socklen_t len;
	unsigned char* ptr = NULL; 

	len = sizeof(struct sockaddr_in);
	memset(&arpreq, 0, sizeof(struct arpreq)); 
	memset(&dstadd_in, 0, sizeof(struct sockaddr_in)); 

	if(getpeername(sockfd, (struct sockaddr*)&dstadd_in, &len) < 0)
	{
		//VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: getpeername err\n", FUN, LINE);
		return -1;
	}

	memcpy(&arpreq.arp_pa, &dstadd_in, sizeof(struct sockaddr_in));
	strcpy(arpreq.arp_dev, "wlp5s0");//wlp5s0
	arpreq.arp_pa.sa_family = AF_INET; 
	arpreq.arp_ha.sa_family = AF_UNSPEC;
	if(ioctl(sockfd, SIOCGARP, &arpreq) < 0)
	{
		//VAVAHAL_Print(LOG_LEVEL_ERR, "[%s][%d]: ioctrl err\n", FUN, LINE);
        printf("error get peermac");
		return -1;
	}

	ptr = (unsigned char *)arpreq.arp_ha.sa_data;
	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5)); 

	return 0;
}



int main(void)
{
    int sock_fd,new_fd; //监听套接字 连接套接字
    struct sockaddr_in server_addr; // 服务器的地址信息
    struct sockaddr_in client_addr; //客户端的地址信息
    socklen_t sin_size;
    int yes = 1;
    char buf[BUF_SIZE];
    int ret;
    int i;
    ServerParam s_param;
    //建立sock_fd套接字
    if((sock_fd = socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        perror("setsockopt");
        exit(1);
    }
    //设置套接口的选项 SO_REUSEADDR 允许在同一个端口启动服务器的多个实例
    // setsockopt的第二个参数SOL SOCKET 指定系统中，解释选项的级别 普通套接字
    if(setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1)
    {
        perror("setsockopt error \n");
        exit(1);
    }
  
    server_addr.sin_family = AF_INET; //主机字节序
    server_addr.sin_port = htons(MYPORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;//通配IP
    memset(server_addr.sin_zero,'\0',sizeof(server_addr.sin_zero));
    if(bind(sock_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1)
    {
        perror("bind error!\n");
        exit(1);
    }
    if(listen(sock_fd,MAXCLINE)==-1)
    {
        perror("listen error!\n");
        exit(1);
    }
    printf("listen port %d\n",MYPORT);
    fd_set fdsr; //文件描述符集的定义
    int maxsock;
    struct timeval tv;
    conn_amount =0;
    sin_size = sizeof(client_addr);
    maxsock = sock_fd;
    while(1)
    {
        //初始化文件描述符集合
        FD_ZERO(&fdsr); //清除描述符集
        FD_SET(sock_fd,&fdsr); //把sock_fd加入描述符集
        //超时的设定
        tv.tv_sec  = 0;
        tv.tv_usec = 500000;
        //添加活动的连接
        #if 0
        for(i=0;i<MAXCLINE;i++) 
        {
            if(fd[i]!=0)
            {
                FD_SET(fd[i],&fdsr);
            }
        }
        #endif
        //如果文件描述符中有连接请求 会做相应的处理，实现I/O的复用 多用户的连接通讯
        //ret = select(maxsock +1,&fdsr,NULL,NULL,&tv);
        ret = select(maxsock +1,&fdsr,NULL,NULL,&tv);
        if(ret < 0) //没有找到有效的连接 失败
        {
            perror("select error!\n");
            break;
        }
        else if(ret ==0)// 指定的时间到，
        {
            //printf("timeout \n");
           
            continue;
        }
        //循环判断有效的连接是否有数据到达
        #if 0
        for(i=0;i<conn_amount;i++)
        {
            if(FD_ISSET(fd[i],&fdsr))
            {
                ret = recv(fd[i],buf,sizeof(buf),0);
                if(ret <=0) //客户端连接关闭，清除文件描述符集中的相应的位
                {
                    printf("client[%d] close\n",i);
                    close(fd[i]);
                    FD_CLR(fd[i],&fdsr);
                    fd[i]=0;
                    conn_amount--;
                
                }
                //否则有相应的数据发送过来 ，进行相应的处理
                else
                {
                    if(ret <BUF_SIZE)
                    memset(&buf[ret],'\0',1);
                    printf("client[%d] send:%s\n",i,buf);
                }
            }
        }
        #endif
        if(FD_ISSET(sock_fd,&fdsr))
        {
            new_fd = accept(sock_fd,(struct sockaddr *)&client_addr,&sin_size);
            if(new_fd <=0)
            {
                perror("accept error\n");
                continue;
            }

 	        s_param.clientfd = new_fd;
			s_param.clientip = inet_addr(inet_ntoa(client_addr.sin_addr));
			memset(s_param.mac, 0, 18);
			ret = getpeermac(s_param.clientfd, s_param.mac);
            if(ret == 0)
            {

                printf("\r\n zmy ---- ok%s ---------",&s_param.mac[0]);
                #if 1
                pthread_t recv_id;
                pthread_attr_t attr;
                pthread_attr_init(&attr); 
                pthread_attr_setdetachstate(&attr, 1);
                pthread_attr_setstacksize(&attr, STACK_SIZE);
                ret = pthread_create(&recv_id, &attr, commonclient_pth, &s_param);
                pthread_attr_destroy(&attr);
                #endif
            }
            #if 0
            if(conn_amount <MAXCLINE)
            {
                for(i=0;i< MAXCLINE;i++)
                {
                    if(fd[i]==0)
                    {
                    fd[i] = new_fd;
                    break;
                    }
                }
                conn_amount++;
                printf("new connection client[%d]%s:%d\n",conn_amount,inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
                if(new_fd > maxsock)
                {
                    maxsock = new_fd;
                }

                #if 1
                pthread_t recv_id;
				pthread_attr_t attr;

                s_param.clientfd = new_fd;
                s_param.clientip = inet_addr(inet_ntoa(client_addr.sin_addr));

                pthread_attr_init(&attr); 
				pthread_attr_setdetachstate(&attr, 1);
				pthread_attr_setstacksize(&attr, STACK_SIZE);
				ret = pthread_create(&recv_id, &attr, commonclient_pth, &s_param);
				pthread_attr_destroy(&attr);
                #endif
            }
            else
            {
                printf("max connections arrive ,exit\n");
                send(new_fd,"bye",4,0);
                close(new_fd);
                continue;
            }
            #endif
        }
        //showclient();
      }
 
 #if 0
      for(i=0;i<MAXCLINE;i++)
      {
        if(fd[i]!=0)
        {
          close(fd[i]);
        }
      }
#endif  
      exit(0);
} 



//处理client 端过来的数据
void *commonclient_pth(void *data)
{
    fd_set rdRd;
    char recvbuff[128];
    int ret;
    int recvsize;
    struct timeval timeout;
    int clientfd; 
    int clientip;
    char clientmac[18];
    char ip_str[16];

    ServerParam *sp = (ServerParam *)data;

    clientfd = sp->clientfd;

    clientip = sp->clientip;
	memset(clientmac, 0, 18);
	strcpy(clientmac, sp->mac);

	memset(ip_str, 0, 16);
	sprintf(ip_str, "%d.%d.%d.%d", (clientip & 0x000000FF),
	                               (clientip & 0x0000FF00) >> 8,
	                               (clientip & 0x00FF0000) >> 16,
	                               (clientip & 0xFF000000) >> 24);



    while(1)
    {
        FD_ZERO(&rdRd);
		FD_SET(clientfd, &rdRd);        

        timeout.tv_sec = 0;				
		timeout.tv_usec = 500000;

		ret = select(clientfd + 1, &rdRd, NULL, NULL, &timeout);
        if(ret < 0)
		{
			//VAVAHAL_Print(LOG_LEVEL_WARING, "[%s][%d]: select err, channel - %d\n", FUN, LINE, channel);
            printf("\r\n zmy select err \r\n");
			break;
		}
		else if(ret == 0)
		{
			continue;       //timeout
		}

        if(FD_ISSET(clientfd, &rdRd))
        {
            ret = recv(clientfd, recvbuff + recvsize, 128 - recvsize, 0);        
            if(ret < 0)
			{
				if(errno == EAGAIN)
				{
					continue;
				}
				break;
			}
			else if(ret == 0)
			{
                printf("\r\n zmy ret = 0");
				//VAVAHAL_Print(LOG_LEVEL_DEBUG, "[%s][%d]: client exit [channel - %d][%s]\n", FUN, LINE, channel, ip_str);
				break;
			}

            printf("\r\n zmy %s",recvbuff);
            //使用心跳机制 超过五次没有数据 断开socket 避免同一个客户端多次连接
            #if 0
			recvsize += ret;
            while(1)
            {
                if(recvsize < 128)
				{
					break;
				}
                printf("\r\n zmy %d",recvsize);
            }
            #endif
        }
    }

    close(clientfd);
    return NULL;
}