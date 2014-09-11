#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include "ae.h"
#include "anet.h"
#include "zmalloc.h"

#define PORT 4444
#define MAX_LEN 10

//存放错误信息的字符串
char g_err_string[1024];
typedef struct _socket_buf {
	int fd;
	char buf[1024*16];
	char w_buf[1024*16];
	size_t len;
	size_t w_len;
	int write_cnt;
} *socket_buf;

//事件循环机制
aeEventLoop *g_event_loop = NULL;

//定时器的入口，输出一句话
int PrintTimer(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
	static int i = 0;
	printf("Test Output: %d\n", i++);
	//10秒后再次执行该函数
	return 10000;
}

//停止事件循环
void StopServer()
{
	aeStop(g_event_loop);
}

void ClientClose(aeEventLoop *el, int fd, int err)
{
	//如果err为0，则说明是正常退出，否则就是异常退出
	if( 0 == err )
		printf("Client quit: %d\n", fd);
	else if( -1 == err )
		fprintf(stderr, "Client Error: %s\n", strerror(errno));

	//删除结点，关闭文件
	aeDeleteFileEvent(el, fd, AE_READABLE);
	close(fd);
}

void SendToClient(aeEventLoop *el, int fd, void *privdata, int mask){
	socket_buf client_socket_buf = (socket_buf)privdata;
	
	int writenLen;
	//char * head = line;
	char * head = client_socket_buf->w_buf;

	// 确保sockfd是非阻塞的
	writenLen = write(fd, head + client_socket_buf->write_cnt, client_socket_buf->w_len);
	if (writenLen == -1)
	{
		if(errno == ECONNRESET)
		{
			// 对端重置,对方发送了RST
			//CloseAndDisable(sockfd, events[i]);
			printf("client socket closed...\n");
			ClientClose(el, fd, writenLen);
		}
		else if (errno == EINTR || errno == EAGAIN)
		{
			// 被信号中断或者缓冲区满，暂时写不了数据
			//continue;
			printf("init....continue...");
		}
		else
		{
			// 其他错误
			printf("write error...\n");
			ClientClose(el, fd, writenLen);
		}
	}

	else if (writenLen == 0)
	{
		// 这里表示对端的socket已正常关闭.
		printf("client socket closed...\n");
		ClientClose(el, fd, writenLen);
	}

	// 以下的情况是writenLen > 0
	else
	{
		// 写了数据
		//break; // 退出while(1)
		client_socket_buf->write_cnt += writenLen;
	}
	
	if(client_socket_buf->write_cnt == client_socket_buf->len){
		//写完了
		printf("write finished:\n");
		printf("write cnt:\n");
		printf("%s", client_socket_buf->buf);
		memset(client_socket_buf->w_buf, 0, 1024*16);
		client_socket_buf->w_len=0;
		client_socket_buf->write_cnt=0;
		//zfree(client_socket_buf->buf);
		//zfree(client_socket_buf);
		
		aeDeleteFileEvent(el, fd, AE_WRITABLE);
	}
}

//有数据传过来了，读取数据
void ReadFromClient(aeEventLoop *el, int fd, void *privdata, int mask)
{
	socket_buf client_socket_buf = (socket_buf)privdata;
	
	char buffer[MAX_LEN] = { 0 };
	int res;
	res = read(fd, buffer, MAX_LEN);
	
	if( res == 0 )
	{
		//free(client_socket_buf->buf);
		//memset(&stTest,0,sizeof(struct sample_struct));
		printf("client socket closed...\n");
		ClientClose(el, fd, res);
	}
	else if( res < 0 )
	{
		if(errno == EWOULDBLOCK || errno == EAGAIN){
			//printf("read finished...break...read cnt(EAGAIN):\n");
			//printf("total read cnt:\n%s\n", client_socket_buf->buf);
			printf("read finished...total read cnt(EAGAIN):\n%s\n", client_socket_buf->buf);

			memcpy(client_socket_buf->w_buf, client_socket_buf->buf, client_socket_buf->len);
			
			memset(client_socket_buf->buf, 0, 1024*16);
			client_socket_buf->len=0;
			//可以参考redis源码，在处理函数完毕之后调用专门的函数设置等待写事件
			if( aeCreateFileEvent(el, fd, AE_WRITABLE,
			SendToClient, client_socket_buf) == AE_ERR )
			{
				fprintf(stderr, "set writeable fail: %d\n", fd);
				//close(fd);
			}
		}
		else if(errno == EINTR){
			printf("initevent....continue...");
			//continue;
		}
		else{
			//free(client_socket_buf->buf);
			//memset(&stTest,0,sizeof(struct sample_struct));
			printf("read error...\n");
			ClientClose(el, fd, res);
		}
	}
	else if( res == MAX_LEN){
		memcpy(client_socket_buf->buf + client_socket_buf->len, buffer, res);
		client_socket_buf->len+=res;
		client_socket_buf->w_len+=res;
		printf("read not finished...continue(res == MAX_LEN)...\n");
	}
	else if( 0 < res < MAX_LEN){
		memcpy(client_socket_buf->buf + client_socket_buf->len, buffer, res);
		client_socket_buf->len+=res;
		client_socket_buf->w_len+=res;

		printf("read finished...total read cnt(res < MAX_LEN):\n%s\n", client_socket_buf->buf);

		memcpy(client_socket_buf->w_buf, client_socket_buf->buf, client_socket_buf->len);
		
		memset(client_socket_buf->buf, 0, 1024*16);
		client_socket_buf->len=0;
		
		//可以参考redis源码，在处理函数完毕之后调用专门的函数设置等待写事件
		if( aeCreateFileEvent(el, fd, AE_WRITABLE,
		SendToClient, client_socket_buf) == AE_ERR )
		{
			fprintf(stderr, "set writeable fail: %d\n", fd);
			//close(fd);
		}
		//break;
	}
}

//接受新连接
void AcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask)
{
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
	
	socket_buf client_socket_buf = (socket_buf)zmalloc(sizeof(struct _socket_buf));
	//client_socket_buf->fd=fd;
	//client_socket_buf->buf=(char *)zmalloc(1024*16);
	//client_socket_buf->w_buf=(char *)zmalloc(1024*16);
	memset(client_socket_buf->buf, 0, 1024*16);
	memset(client_socket_buf->w_buf, 0, 1024*16);
	client_socket_buf->len=0;
	client_socket_buf->w_len=0;
	client_socket_buf->write_cnt=0;
	
	
	if( aeCreateFileEvent(el, cfd, AE_READABLE,
		ReadFromClient, client_socket_buf) == AE_ERR )
	{
		fprintf(stderr, "client connect fail: %d\n", fd);
		close(fd);
	}
}

int main()
{
	setbuf(stdout,NULL);
	printf("Start\n");

	signal(SIGINT, StopServer);

	//初始化网络事件循环
	g_event_loop = aeCreateEventLoop(1024*10);

	//设置监听事件
	int fd = anetTcpServer(g_err_string, PORT, NULL, 3);
	if( ANET_ERR == fd )
		fprintf(stderr, "Open port %d error: %s\n", PORT, g_err_string);
	if( aeCreateFileEvent(g_event_loop, fd, AE_READABLE, 
		AcceptTcpHandler, NULL) == AE_ERR )
		fprintf(stderr, "Unrecoverable error creating server.ipfd file event.");

	//设置定时事件
	//aeCreateTimeEvent(g_event_loop, 1, PrintTimer, NULL, NULL);

	//开启事件循环
	aeMain(g_event_loop);

	//删除事件循环
	aeDeleteEventLoop(g_event_loop);

	printf("End\n");
	return 0;
}

