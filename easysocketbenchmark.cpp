/***************************************************************************
file: easysocketbenchmark.cpp
-------------------
begin : 2014/09/01
copyright : (C) 2014 by 
email : yangjun03@baidu.com
***************************************************************************/

/***************************************************************************
* *
* This program is free software; you can redistribute it and/or modify *
* it under the terms of the GNU General Public License as published by *
* the Free Software Foundation; either version 2 of the License, or *
* (at your option) any later version. *
* *
***************************************************************************/
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#include "ae.h"
#include "anet.h"
#include "zmalloc.h"

#define MAX_LEN 1024*16

pthread_mutex_t mutex;//global mutex
static int total_req = 0;
static int total_res = 0;
static int total_err = 0;

static int below_10=0;
static int between_10_20=0;
static int between_20_30=0;
static int over_30=0;
static long total_res_time=0;

class easysocketbenchmark{
	private:
		aeEventLoop *g_event_loop;
		std::vector<int> fd_list;
		char *ip;
		int port;
		int client_num;
		double test_time;
	public:
		easysocketbenchmark(char *ip, int port, int client_num, double test_time);
		int createclient();
		void asyncrequest(int fd);
		void startbenchmark();
		void stopbenchmark();
		static void handleresponse(aeEventLoop *el, int fd, void *privdata, int mask);
		void closeclient(int fd, int err);
		void startloop();
		void delloop();
		static int handletimmer(aeEventLoop *el, long long id, void *clientData);
		void timmerevent();
		static long getcurrenttime();
};

typedef struct callbackargs_{
	easysocketbenchmark *esb;
	long int start_time;
} *callbackargs;

easysocketbenchmark::easysocketbenchmark(char *ip, int port, int client_num, double test_time){
	//初始化网络事件循环
	this->g_event_loop = aeCreateEventLoop(1024*10);
	this->ip = zmalloc(sizeof(ip));
	strcpy(this->ip, ip);
	this->port = port;
	this->client_num = client_num;
	this->test_time = test_time;
}

int easysocketbenchmark::createclient(){
	int fd = anetTcpConnect(NULL, this->ip, this->port);
	while(ANET_ERR == fd){
		fd = anetTcpConnect(NULL, this->ip, this->port);
	}
	/*if( ANET_ERR == fd ){
		//fprintf(stderr, "connect error: %s\n", PORT, NULL);
		printf("connect error......");
	}*/
	if(anetNonBlock(NULL, fd) == ANET_ERR){
		//fprintf(stderr, "set nonblock error: %d\n", fd);
		//printf("set nonblock error......");
	}
	return fd;
}

void easysocketbenchmark::asyncrequest(int fd){
	int writenLen;
	char * head = "hello world !!!";

	// 确保sockfd是非阻塞的
	writenLen = write(fd, head, strlen(head));
	if (writenLen == -1){
		if(errno == ECONNRESET){
			//printf("client socket closed...\n");
			this->closeclient(fd, writenLen);
		}
		else if (errno == EINTR || errno == EAGAIN){
			//printf("init....continue...");
		}
		else{
			// 其他错误
			this->closeclient(fd, writenLen);
		}
	}

	else if (writenLen == 0){
		// 这里表示对端的socket已正常关闭.
		this->closeclient(fd, writenLen);
	}

	else{
		// 写了数据
		//printf("write finished...\n");
		//printf("write cnt:\n");
		//printf("%s\n", head);
		pthread_mutex_lock(&mutex);
		total_req += 1;
		pthread_mutex_unlock(&mutex);
		aeDeleteFileEvent(this->g_event_loop, fd, AE_WRITABLE);
		
		//typedef void* (*FUNC_REQ_CB)(aeEventLoop *el, int fd, void *privdata, int mask);//定义FUNC类型是一个指向函数的指针，该函数参数为void*，返回值为void*
		//FUNC_REQ_CB callback_req = (FUNC_REQ_CB)&easysocketbenchmark::handleresponse;//强制转换func()的类型
		
		callbackargs cbargs = (callbackargs)zmalloc(sizeof(struct callbackargs_));
		cbargs->esb = this;
		cbargs->start_time = easysocketbenchmark::getcurrenttime();
		if( aeCreateFileEvent(this->g_event_loop, fd, AE_READABLE,
		easysocketbenchmark::handleresponse, cbargs) == AE_ERR ){
			fprintf(stderr, "client connect fail: %d\n", fd);
			close(fd);
		}
	}
}

void easysocketbenchmark::startbenchmark(){
	//std::vector<int> fd_list;
	int i;
	for(i=0; i < this->client_num; i++){
		this->fd_list.push_back(this->createclient());
	}
	std::vector<int>::iterator itr;
	std::vector<int>::iterator last;
	for(itr = this->fd_list.begin(), last = this->fd_list.end(); itr != last; ++itr){
		this->asyncrequest(*itr);
	}
}

void easysocketbenchmark::stopbenchmark(){
	aeStop(this->g_event_loop);
	std::vector<int>::iterator itr;
	std::vector<int>::iterator last;
	for(itr = this->fd_list.begin(), last = this->fd_list.end(); itr != last; ++itr){
		close(*itr);
	}
}

int easysocketbenchmark::handletimmer(aeEventLoop *el, long long id, void *clientData){
	easysocketbenchmark *esb = (easysocketbenchmark *)clientData;
	aeDeleteTimeEvent(el, id);
	esb->stopbenchmark();
}

void easysocketbenchmark::timmerevent(){
	aeCreateTimeEvent(this->g_event_loop, this->test_time*60*1000, easysocketbenchmark::handletimmer, this, NULL);
}

void easysocketbenchmark::handleresponse(aeEventLoop *el, int fd, void *privdata, int mask){
	callbackargs cbargs = (callbackargs)privdata;
	easysocketbenchmark *esb = cbargs->esb;
	long int start_time = cbargs->start_time;
	char buffer[MAX_LEN] = { 0 };
	int res;
	res = read(fd, buffer, MAX_LEN-1);
	
	if( res == 0 ){
		//printf("client socket closed...\n");
		esb->closeclient(fd, res);
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
			pthread_mutex_lock(&mutex);
			total_err += 1;
			pthread_mutex_unlock(&mutex);
			esb->closeclient(fd, res);
		}
	}
	
	else if( res == MAX_LEN-1){
		//printf("partial content : %s", buffer);
		//printf("read not finished...continue...\n");
	}
	else if( 0 < res < MAX_LEN-1){
		//printf("partial content : %s", buffer);
		//printf("read finished...\n");
		long end_time = easysocketbenchmark::getcurrenttime();
		int cost_time = end_time - start_time;
		
		pthread_mutex_lock(&mutex);
		total_res += 1;
		total_res_time+=cost_time;
		if(cost_time<10){
			below_10++;
		}else if(cost_time>=10 && cost_time<20){
			between_10_20++;
		}else if(cost_time>=20 && cost_time<30){
			between_20_30++;
		}else{
			over_30++;
		}
		pthread_mutex_unlock(&mutex);
		
		aeDeleteFileEvent(el, fd, AE_READABLE);
		esb->asyncrequest(fd);
	}
}

void easysocketbenchmark::closeclient(int fd, int err){
	//如果err为0，则说明是正常退出，否则就是异常退出
	if( 0 == err ){
		//printf("Client quit: %d\n", fd);
	}
	else if( -1 == err ){
		//printf("client error: %s\n", strerror(errno));
	}
	//删除结点，关闭文件
	aeDeleteFileEvent(this->g_event_loop, fd, AE_READABLE);
	aeDeleteFileEvent(this->g_event_loop, fd, AE_WRITABLE);
	close(fd);
}

void easysocketbenchmark::startloop(){
	aeMain(this->g_event_loop);
}

void easysocketbenchmark::delloop(){
	aeDeleteEventLoop(this->g_event_loop);
}

long easysocketbenchmark::getcurrenttime(){
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

typedef struct benchmarkargs_{
	char *ip;
	int port;
	int thread_num;
	int client_num;
	double test_time;
} *benchmarkargs;

void *easysocketbenchmarkthread(void *data){
	benchmarkargs args = (benchmarkargs)data;
	char *ip = (char *)zmalloc(sizeof(args->ip));
	strcpy(ip, args->ip);
	int port = args->port;
	int thread_num = args->thread_num;
	int client_num = args->client_num;
	double test_time = args->test_time;
	//printf("easysocketbenchmark start...\n");
	easysocketbenchmark *esb = new easysocketbenchmark(ip, port, client_num, test_time);
	esb->timmerevent();
	esb->startbenchmark();
	esb->startloop();
	esb->delloop();
	//printf("easysocketbenchmark stop...\n");
}

void easysocketbenchmarkthreadpool(void *data){
	benchmarkargs args = (benchmarkargs)data;
	std::vector<pthread_t> threads;
	int i;
	for(i=0; i<args->thread_num; i++){
		pthread_t pid;
		threads.push_back(pid);
		int ret=pthread_create(&pid,NULL,easysocketbenchmarkthread,args);
		if(ret!=0){
			printf ("create pthread error!\n");
			exit (1);
		}
	}
	/*for(i=0; i<args->thread_num; i++){
		int rc = pthread_join(threads[i], NULL);
		if (rc){
			printf("pthread join error %d\n", rc);
			exit (1);
		}
	}*/
}

void helpinfo(void){
	printf("%s", "======================================================================================\n");
	printf("%s", "|                                 Usage Instructions                                 |\n");
	printf("%s", "======================================================================================\n");
	printf("%s", "|USAGE          : ./easysocketbenchmark IP PORT THREADNUM CLIENTNUM TESTTIME\n");
	printf("%s", "|           IP  : sever ip, eg : 127.0.0.1 \n");
	printf("%s", "|         PORT  : server port, eg : 4444 \n");
	printf("%s", "|   THREADNUM   : thread pool size, eg : 1 \n");
	printf("%s", "|   CLIENTNUM   : socket connections of per thread, eg : 20 \n");
	printf("%s", "|    TESTTIME   : total test time(min), eg : 1 \n|\n");
	printf("%s", "|EXAMPLE        : ./easysocketbenchmark 127.0.0.1 4444 1 20 1\n");
	printf("%s", "|-------------------------------------------------------------------------------------\n");
	printf("%s", "|MORE           : if any questions, please contact 597092663@qq.com.\n");
	printf("%s", "======================================================================================\n");
}

void testreport(int thread_num, int client_num, double test_time, int total_req, int total_res, int total_err, 
double avglatency, int below_10, int between_10_20, int between_20_30, int over_30){
	printf("%s", "======================================================================================\n");
	printf("%s", "|                                 TEST REPORT                                        |\n");
	printf("%s", "======================================================================================\n");
	printf("|THREADNUM            : %d \n", thread_num);
	printf("|THREADCLIENT         : %d \n", client_num);
	printf("|TOTALCLIENTS         : %d \n", thread_num*client_num);
	printf("|TESTTIME(MIN)        : %0.2f \n", test_time);
	printf("|-------------------------------------------------------------------------------------\n");
	printf("|TOTALREQ             : %d \n", total_req);
	printf("|TOTALRES             : %d \n", total_res);
	printf("|TOTALERR             : %d \n", total_err);
	printf("|QPS                  : %0.2f \n", total_res/(test_time*60));
	printf("|-------------------------------------------------------------------------------------\n");
	printf("|AVGLATENCY(MS)       : %0.2f \n", avglatency);
	printf("|BELOW_10(MS)         : %d \n", below_10);
	printf("|BT_10_20(MS)         : %d \n", between_10_20);
	printf("|BT_20_30(MS)         : %d \n", between_20_30);
	printf("|OVER_30(MS)          : %d \n", over_30);
	printf("%s", "======================================================================================\n");
}

int main(int argc, char **argv){
	if(pthread_mutex_init(&mutex,NULL)!=0){
		return -1;
	}
	
	setbuf(stdout,NULL);
	if(argc != 6){
		helpinfo();
		return -1;
	}
	
	benchmarkargs args = (benchmarkargs)zmalloc(sizeof(struct benchmarkargs_));
	args->ip = (char *)zmalloc(sizeof(argv[1]));
	strcpy(args->ip, argv[1]);
	args->port = atoi(argv[2]);
	args->thread_num = atoi(argv[3]);
	args->client_num = atoi(argv[4]);
	args->test_time = atof(argv[5]);
	easysocketbenchmarkthreadpool(args);
	
	long int start_time = easysocketbenchmark::getcurrenttime();
	long int end_time = easysocketbenchmark::getcurrenttime();
	long int total_test_time = args->test_time*60*1000;
	while(end_time - start_time < total_test_time){
		//wait 100ms
		usleep(100000);
		end_time = easysocketbenchmark::getcurrenttime();
	}
	double avglatency = (double)total_res_time/(double)total_res;
	testreport(args->thread_num, args->client_num, args->test_time, total_req, total_res, total_err, 
		avglatency, below_10, between_10_20, between_20_30, over_30);
	
	return 0;
}


