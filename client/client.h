/*包含一些要用到的头文件，一些宏定义*/

#include<stdio.h>
#include<strings.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/select.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<errno.h>

#define max(a,b) (a > b ? a : b)
#define SA struct sockaddr
#define MAXLINE 1024
#define PORT 9999

