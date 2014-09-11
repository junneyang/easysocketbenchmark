#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>

int main(){
	char str[100];
	memset(str, 0, 100);
	printf("str : %s", str);
	
	char a[20]="hello";
	memcpy(str, a, strlen(a));
	printf("str : %s", str);
	printf("a : %s", a);
	return 0;
}

