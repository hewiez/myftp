#include "server.h"

struct epoll_event ev,events[10];
int epollfd;
int islogin=0;  //用于判断用户是否登陆
char result[1024];  //存储返回给客户端的消息
char dirname[1024]; //存储目录信息
char filename[1024];    //存储文件信息
char username[100]; //存储登陆用户名
char workpath[1024];    //存储用户工作目录
int ftp_listenfd;    //监听套结字
int ftp_connfd;  //连接套结字
char ip_addr[100];  //存储用户输入的ip地址

void ftp_echo(int); //处理函数
int add_epoll_listen(int,int);  //添加对套结字的监听
int get_rand(); //获取随机数，用于随机端口
void get_time();    //获取当前时间，用于错误信息打印
void log_error();   //将错误信息写入log_error中

int main(int argc,char **argv)  {
    int listenfd,connfd;
    int res=0,n=0,rep=1;
    pid_t childpid;
    struct  sockaddr_in cliaddr,servaddr;  
    socklen_t clilen;
    fd_set rset,tset;
    
    if(argc!=2){  
        printf("useage:./server <IPaddress>\n");  
        exit(1);  
    }
    
    strcpy(ip_addr,argv[1]);
    listenfd=socket(AF_INET,SOCK_STREAM,0);
    //设置listen端口可重用
    res=setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&rep,sizeof(rep));
    if(res!=0){
        log_error("setsockopt error!\n");
        return -1;
    }
    
    daemon(1,0);    //后台化
    bzero(&servaddr,sizeof(servaddr));  
    servaddr.sin_family=AF_INET;
    inet_pton(AF_INET,ip_addr,&servaddr.sin_addr);
    servaddr.sin_port=htons(PORT);
    
    res=bind(listenfd,(SA *)&servaddr,sizeof(servaddr));
    if(res!=0){
        log_error("bind error!\n");
        return -1;
    }
    
    res=0;
    res=listen(listenfd,LISTENQ);
    if(res!=0){
        log_error("listen error!\n");
        return -1;
    }
    
    FD_ZERO(&rset);
    FD_SET(listenfd,&rset);
    
    for(;;)  
    {
        tset=rset;
        n=select(listenfd+1,&tset,NULL,NULL,NULL);
        if(n==-1){
            log_error("select error!\n");
            continue;
        }
        
        clilen=sizeof(cliaddr);  
        connfd=accept(listenfd,(SA *)&cliaddr,&clilen);
        if(connfd==-1){
            log_error("accept error!\n");
            continue;
        }
        
        if((childpid=fork())==0)  
        {
            close(listenfd);  
            ftp_echo(connfd);  
            exit(0);  
        }  
        close(connfd);  
    }  
}

void ftp_echo(int sockfd)  {
    int nfds=0,i,n;
    char cmdstr[50];
    
    bzero(cmdstr,sizeof(cmdstr));
    bzero(result,sizeof(result));
    memset(username,0,sizeof(username));

    epollfd=epoll_create(10);
    ev.events=EPOLLIN;
    ev.data.fd=sockfd;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,sockfd,&ev);
    
    for(;;){
        nfds=epoll_wait(epollfd,events,10,-1);
        for(i=0;i<nfds;i++){
            
            //sockfd准备好，处理控制数据
            if(events[i].data.fd==sockfd){
                n=read(sockfd,cmdstr,sizeof(cmdstr));
                if(n==0)//客户端断开连接
                    exit(0);
                cmdstr[n-1]='\0';//将回车换为字符串结束符
                 
                //只有先验证登陆，才能进行其他操作
                if(!islogin&&strncmp(cmdstr,"login",5)!=0){
                    strcpy(result,"error 001:sorry,please login first!\n");
                    write(sockfd,result,strlen(result));
                    bzero(result,sizeof(result));
                    bzero(cmdstr,sizeof(cmdstr));
                }else{
                    //若要传输文件，则创建一个数据连接，并添加监听
                    if(strncmp(cmdstr,"get",3)==0||strncmp(cmdstr,"put",3)==0){
                        //添加一个端口用于传送数据，将该端口发送给客户端，让客户端来连接。
                        add_epoll_listen(sockfd,get_rand());
                        break;
                    }
                    //若是普通的控制指令，直接执行，并将结果写回
                    else{
                        execute_cmd(cmdstr);
                        write(sockfd,result,strlen(result));
                        bzero(result,sizeof(result));
                        bzero(cmdstr,sizeof(cmdstr));
                    }
                }
            }
            
            //监听套结字准备好，准备接受客户端的数据连接
            if(events[i].data.fd==ftp_listenfd){
                ftp_connfd=accept(ftp_listenfd,NULL,NULL);

                if(ftp_connfd!=-1){
                    if(strncmp(cmdstr,"get",3)==0){
                        ev.events=EPOLLOUT|EPOLLET;
                    }else{
                        ev.events=EPOLLIN|EPOLLET;
                    }
                    ev.data.fd=ftp_connfd;
                    //添加对连接套结字的监听
                    epoll_ctl(epollfd,EPOLL_CTL_ADD,ftp_connfd,&ev);
                }else{
                    log_error("connect error!\n");
                    return ;
                }
            }
            
            //连接套结字准备好，准备和客户端传输数据
            if(events[i].data.fd==ftp_connfd){//i写成n！！！！！！！！！！
                execute_cmd(cmdstr);
                write(sockfd,result,strlen(result));
                bzero(result,sizeof(result));
            }
        }
    }
}

//添加对数据传输套结字的监听
int add_epoll_listen(int sockfd,int tport){
    int res=0;
    char buf[100];
    int rep=1;
    struct  sockaddr_in servaddr; 
    
    ftp_listenfd=-1;
    bzero(buf,sizeof(buf));
    bzero(&servaddr,sizeof(servaddr));

    ftp_listenfd=socket(AF_INET,SOCK_STREAM,0);
    //设置端口可重用
    res=setsockopt(ftp_listenfd,SOL_SOCKET,SO_REUSEADDR,&rep,sizeof(rep));
    if(res!=0){
        log_error("datatransform setsockopt error!\n");
        return -1;
    }
    servaddr.sin_family=AF_INET;
    inet_pton(AF_INET,ip_addr,&servaddr.sin_addr);
    servaddr.sin_port=htons(tport);
    
    res=bind(ftp_listenfd,(SA *)&servaddr,sizeof(servaddr));
    if(res!=0){
        log_error("datatransform bind error!\n");
        return -1;
    }
    
    res=listen(ftp_listenfd,3);
    if(res!=0){
        log_error("datatransform listen error!\n");
        return -1;
    }
    
    //添加ftp_listenfd监听
    ev.events=EPOLLIN|EPOLLET;
    ev.data.fd=ftp_listenfd;
    if(epoll_ctl(epollfd,EPOLL_CTL_ADD,ftp_listenfd,&ev)==-1){
        log_error("datatransform ftp_listenfd add error!\n");
        return -1;
    }
    
    //将传输数据端口发送给客户端
    sprintf(buf,"%d",tport);
    write(sockfd,buf,strlen(buf));
    return 0;
}

int get_rand(){
    srand(time(0));
    return (rand()%(9998-4000)+4000);
}

void get_time(char *s){
    time_t timep;
    time(&timep);
    strcpy(s,ctime(&timep));
}

void log_error(char *error){
    FILE *fp;
    char time[100];
    memset(time,0,sizeof(time));
    fp=fopen("log_error","a+");
    if(fp==NULL){
        return ;
    }
    get_time(time);
    fwrite(time,strlen(time),1,fp);
    fwrite(error,strlen(error),1,fp);
    fclose(fp);
}
