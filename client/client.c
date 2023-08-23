#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define SERVER_PORT 9090
#define SERVER_IP "192.168.221.128" 


int main(){
	// 1.创建客户端与目标服务端通信的套接字
	int client_fd = socket(AF_INET,SOCK_STREAM,0);
	if(client_fd == -1){
		perror("Failed to create communciate socket........\n");
		return -1;
	}

	// 2.连接到目标服务端的IP和Port
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr.s_addr);
	
	int ret = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(ret == -1){
		perror("Failed to connect the server........\n");
		return -1;
	}
	
	int number = 1;
	// 3.目标服务端和客户端开始通信
	while(1){
		// 客户端要发送给服务端的数据
		char buff[1024];
		
		// fgets(buff, sizeof(buff), stdin);
		sprintf(buff, "hello word %d, i am epoll\n", number++);
		printf("will sent to server: %s\n", buff);
		send(client_fd, buff, sizeof(buff), 0);
		
		// 客户端从服务端收到的数据
		memset(buff, 0, sizeof(buff));
		int len = recv(client_fd, buff, sizeof(buff), 0);
		if(len == -1){
			perror("failed to read message......\n");
			exit(1);
		}
		else if(len == 0){
			printf("server had diconnect............\n");
		}
		printf("read from server: %s\n", buff);
		sleep(1);		
	}
	
	// 关闭文件描述符
	close(client_fd);

	return 0;
}
