#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdarg.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <syslog.h>
#include <time.h>

#define max(a,b) (a > b ? a : b)
#define SA struct sockaddr
#define MAXLINE 1024
#define LISTENQ 1024
#define PORT 9999

