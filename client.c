#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include "ae.h"
#include "anet.h"
#include "zmalloc.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <algorithm>
#include <pthread.h>
#include <unistd.h>

#define PORT 4444
#define MAX_LEN 1024*16
static int total_req = 0;
static int total_res = 0;

//事件循环机制
aeEventLoop *g_event_loop = NULL;
void WriteHandler(aeEventLoop *el, int fd, void *privdata, int mask);
//获取当前时间函数，返回单位ms
long getCurrentTime();

long getCurrentTime(){
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

//定时器的入口，输出一句话
int PrintTimer(struct aeEventLoop *eventLoop, long long id, void *clientData){
	/*static int i = 0;
	//printf("Test Output: %d\n", i++);
	//10秒后再次执行该函数
	return 10000;*/
	printf("event test.....\n");
	aeDeleteTimeEvent(eventLoop, id);
	//return 5000;
}

//停止事件循环
void StopServer(){
	aeStop(g_event_loop);
}

void ClientClose(aeEventLoop *el, int fd, int err){
	//如果err为0，则说明是正常退出，否则就是异常退出
	if( 0 == err ){
		//printf("Client quit: %d\n", fd);
	}
	else if( -1 == err )
		fprintf(stderr, "Client Error: %s\n", strerror(errno));

	//删除结点，关闭文件
	aeDeleteFileEvent(el, fd, AE_READABLE);
	aeDeleteFileEvent(el, fd, AE_WRITABLE);
	close(fd);
}


//有数据传过来了，读取数据
void ReadFromServer(aeEventLoop *el, int fd, void *privdata, int mask)
{
	char buffer[MAX_LEN] = { 0 };
	int res;
	res = read(fd, buffer, MAX_LEN-1);
	
	if( res == 0 ){
		//printf("client socket closed...\n");
		ClientClose(el, fd, res);
	}
	else if( res < 0 ){
		if(errno == EWOULDBLOCK || errno == EAGAIN){
			//printf("read finished...\n");
		}
		else if(errno == EINTR){
			//printf("initevent....continue...");
		}
		else{
			//printf("read error...\n");
			ClientClose(el, fd, res);
		}
	}
	
	else if( res == MAX_LEN-1){
		//printf("partial content : %s", buffer);
		//printf("read not finished...continue...\n");
	}
	else if( 0 < res < MAX_LEN-1){
		//printf("partial content : %s", buffer);
		//printf("read finished...\n");
		total_res += 1;
		aeDeleteFileEvent(el, fd, AE_READABLE);
		WriteHandler(el, fd, NULL, 0);
	}
}

//接受新连接
void WriteHandler(aeEventLoop *el, int fd, void *privdata, int mask){
	int writenLen;
	char * head = "hello world !!!";

	// 确保sockfd是非阻塞的
	writenLen = write(fd, head, strlen(head));
	if (writenLen == -1){
		if(errno == ECONNRESET){
			//printf("client socket closed...\n");
			ClientClose(el, fd, writenLen);
		}
		else if (errno == EINTR || errno == EAGAIN){
			//printf("init....continue...");
		}
		else{
			// 其他错误
			//printf("write error...\n");
			ClientClose(el, fd, writenLen);
		}
	}

	else if (writenLen == 0){
		// 这里表示对端的socket已正常关闭.
		//printf("client socket closed...\n");
		ClientClose(el, fd, writenLen);
	}

	else{
		// 写了数据
		//printf("write finished...\n");
		//printf("write cnt:\n");
		//printf("%s\n", head);
		total_req += 1;
		aeDeleteFileEvent(el, fd, AE_WRITABLE);
		if( aeCreateFileEvent(el, fd, AE_READABLE, 
		ReadFromServer, NULL) == AE_ERR ){
			fprintf(stderr, "client connect fail: %d\n", fd);
			close(fd);
		}
	}
}

int main(int argc, char **argv){
	setbuf(stdout,NULL);
	printf("Start\n");

	signal(SIGINT, StopServer);

	//初始化网络事件循环
	g_event_loop = aeCreateEventLoop(1024*10);
	
	long int start_time = getCurrentTime();
	/*//设置监听事件
	int i;
	for(i=0; i<10; i++){
		int fd = anetTcpConnect(NULL, argv[1], atoi(argv[2]));
		if( ANET_ERR == fd ){
			fprintf(stderr, "connect error: %s\n", PORT, NULL);
		}
		if(anetNonBlock(NULL, fd) == ANET_ERR){
			fprintf(stderr, "set nonblock error: %d\n", fd);
		}
		
		
		WriteHandler(g_event_loop, fd, NULL, 0);
	}*/

	
	//设置定时事件
	aeCreateTimeEvent(g_event_loop, 5000, PrintTimer, NULL, NULL);

	//开启事件循环
	aeMain(g_event_loop);

	//删除事件循环
	aeDeleteEventLoop(g_event_loop);

	/*printf("End\n");
	long int end_time = getCurrentTime();
	double elapsed = (double)(end_time-start_time);
	printf("test_time : %0.2f s\n", elapsed/1000);
	printf("total_req : %d\n", total_req);
	printf("total_res : %d\n", total_res);*/
	return 0;
}

