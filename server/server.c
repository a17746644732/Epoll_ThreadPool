#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "threadpool.h"
#include "threadpool.c"

#define SERVER_PORT 9090
#define MAXEVENTS 1024

typedef struct socketinfo{
	ThreadPool* p;
	int fd;
	int epfd;
}SocketInfo;

// 处理连接的线程函数
void acceptconn(void* arg){

	printf("Accept thread ID: %ld\n", pthread_self());
	SocketInfo* info = (SocketInfo*)arg;
	// 建立新连接并加入epfd
	// 创建用于接收客户端ip端口新的结构体
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int cfd = accept(info->fd,(struct sockaddr*)&client_addr, &client_len);

	// 打印客户端IP和端口信息
	char ip[32];
	printf("client IP: %s, Port: %d\n",inet_ntop(AF_INET,
				&client_addr.sin_addr.s_addr,ip,sizeof(ip)),ntohs(client_addr.sin_port));

	// 水平触发
	// ev.events = EPOLLIN;
	// 边缘触发
	// 设置非阻塞模式:边缘触发需要，否则当读缓冲区空且
	// 客户端未断开连接时将陷入阻塞状态
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = cfd;
	int ret = epoll_ctl(info->epfd, EPOLL_CTL_ADD, cfd, &ev);
	if(ret == -1){
		perror("failed to epoll ctl.........\n");
		exit(1);
	}

}


// 处理通信的线程函数
void communication(void* arg){

	printf("Communication thread ID: %ld\n", pthread_self());
	SocketInfo* info = (SocketInfo*)arg;
	int curfd = info->fd;
	int epfd = info->epfd;
	// 处理通信文件描述符
	// 接收数据并在其尾部添加客户端fd后再发送给客户端
	char temp[4096];
	bzero(temp,sizeof(temp));
	while(1){
		bzero(buf,sizeof(buf));
		int len = recv(curfd, buf, sizeof(buf), 0);
		if(len > 0){
			printf("read from client %d: %s\n", curfd, buf);
			for(int i=0; i<len; ++i){
				buf[i] = toupper(buf[i]);
			}
			strncat(temp+strlen(temp), buf, len);
			bzero(buf, sizeof(buf));
		}
		else if(len == -1){
			if(errno == EAGAIN){
				printf("data had been read over......\n");
				printf("will sent to client %d: %s\n", curfd, temp);
				send(curfd, temp, strlen(temp)+1, 0);
				break;	
			}
			else{
				perror("failed to recv message......\n");
				break;		
			}
		}
		else if(len == 0){
			printf("client %d has close the connect.......\n", curfd);
			epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);
			close(curfd);
			break;
		}
	}
}

int main(int argc, const char *argv[]){

	// 创建服务端套接字
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if(lfd == -1){
		perror("failed to create socket.......\n");
		exit(1);
	}

	// 初始化服务端IP端口
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 设置端口复用
	int optval = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	// 绑定socket和ip端口
	int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(ret == -1){
		perror("failed to bind socket and ip_port........\n");
		exit(1);
	}

	// 监听
	ret = listen(lfd, 64);
	if(ret == -1){
		perror("failed to listen..........\n");
		exit(1);
	}

	printf("The server is waiting for connect..........\n");

	// 创建epoll实例
	int epfd = epoll_create(1);
	if(epfd == -1){
		perror("failed to create epfd......\n");
		exit(0);
	}

	// 初始化epoll_event结构体
	struct epoll_event ev;

	// 水平触发模式
	// ev.events = EPOLLIN;
	// 边缘触发模式
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = lfd;

	// 将监听socket和epoll_event数据放入epoll实例
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if(ret == -1){
		perror("failed to epoll ctl........\n");
		exit(1);
	}

	struct epoll_event events[MAXEVENTS];
	int size = sizeof(events) / sizeof(events[0]);
	ThreadPool* pool = threadPoolCreate(3, 8, 100);

	while(1){
		// 检测就绪的文件描述符
		int ready = epoll_wait(epfd, events, size, -1);
		for(int i = 0; i < ready; ++i){
			// 取出当前的文件描述符
			int curfd = events[i].data.fd;
			SocketInfo* info = (SocketInfo*)malloc(sizeof(SocketInfo));
			info->p = pool;
			info->fd = curfd;
			info->epfd = epfd;
			// 判断是否为监听文件描述符
			if(curfd == lfd){
				threadPoolAdd(info->p, acceptconn, info);
			}
			else{
				threadPoolAdd(info->p, communication, info);
			}

		}
	}

	close(lfd);

	return 0;
}
