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

//��Ŵ�����Ϣ���ַ���
char g_err_string[1024];

//�¼�ѭ������
aeEventLoop *g_event_loop = NULL;

//��ʱ������ڣ����һ�仰
int PrintTimer(struct aeEventLoop *eventLoop, long long id, void *clientData){
	static int i = 0;
	//printf("Test Output: %d\n", i++);
	//10����ٴ�ִ�иú���
	return 10000;
}

//ֹͣ�¼�ѭ��
void StopServer(){
	aeStop(g_event_loop);
}

void ClientClose(aeEventLoop *el, int fd, int err){
	//���errΪ0����˵���������˳�����������쳣�˳�
	if( 0 == err ){
		//printf("Client quit: %d\n", fd);
	}
	else if( -1 == err )
		fprintf(stderr, "Client Error: %s\n", strerror(errno));

	//ɾ����㣬�ر��ļ�
	aeDeleteFileEvent(el, fd, AE_READABLE);
	aeDeleteFileEvent(el, fd, AE_WRITABLE);
	close(fd);
}

void SendToClient(aeEventLoop *el, int fd, void *privdata, int mask){
	int writenLen;
	char * head = privdata;

	// ȷ��sockfd�Ƿ�������
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
			// ��������
			//printf("write error...\n");
			ClientClose(el, fd, writenLen);
		}
	}

	else if (writenLen == 0){
		// �����ʾ�Զ˵�socket�������ر�.
		//printf("client socket closed...\n");
		ClientClose(el, fd, writenLen);
	}

	else{
		// д������
		aeDeleteFileEvent(el, fd, AE_WRITABLE);
		//printf("write finished...\n");
		//printf("write cnt:\n");
		//printf("%s\n", head);
	}
}

//�����ݴ������ˣ���ȡ����
void ReadFromClient(aeEventLoop *el, int fd, void *privdata, int mask)
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
		char *ret = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
		//���Բο�redisԴ�룬�ڴ��������֮�����ר�ŵĺ������õȴ�д�¼�
		if( aeCreateFileEvent(el, fd, AE_WRITABLE,
		SendToClient, ret) == AE_ERR ){
			fprintf(stderr, "set writeable fail: %d\n", fd);
			//close(fd);
		}
	}
}

//����������
void AcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask){
	int cfd, cport;
	char ip_addr[128] = { 0 };
	cfd = anetTcpAccept(g_err_string, fd, ip_addr, strlen(ip_addr), &cport);
	printf("Connected from %s:%d\n", ip_addr, cport);

	if(anetNonBlock(g_err_string, cfd) == ANET_ERR){
		fprintf(stderr, "set nonblock error: %d\n", fd);
	}
	if(anetEnableTcpNoDelay(g_err_string,fd) == ANET_ERR){
		fprintf(stderr, "set nodelay error: %d\n", fd);
	}
	
	if( aeCreateFileEvent(el, cfd, AE_READABLE, 
	ReadFromClient, NULL) == AE_ERR ){
		fprintf(stderr, "client connect fail: %d\n", fd);
		close(fd);
	}
}

int main(){
	setbuf(stdout,NULL);
	printf("Start\n");

	signal(SIGINT, StopServer);

	//��ʼ�������¼�ѭ��
	g_event_loop = aeCreateEventLoop(1024*10);

	//���ü����¼�
	int fd = anetTcpServer(g_err_string, PORT, NULL, 3);
	if( ANET_ERR == fd ){
		fprintf(stderr, "Open port %d error: %s\n", PORT, g_err_string);
	}
	if( aeCreateFileEvent(g_event_loop, fd, AE_READABLE, 
	AcceptTcpHandler, NULL) == AE_ERR ){
		fprintf(stderr, "Unrecoverable error creating server.ipfd file event.");
	}

	//���ö�ʱ�¼�
	//aeCreateTimeEvent(g_event_loop, 1, PrintTimer, NULL, NULL);

	//�����¼�ѭ��
	aeMain(g_event_loop);

	//ɾ���¼�ѭ��
	aeDeleteEventLoop(g_event_loop);

	printf("End\n");
	return 0;
}

