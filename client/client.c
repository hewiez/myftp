#include "client.h"

void ftp_cli(FILE *,int);
void print_user();
void help();
void inputId();

char key[100];
char user[100];
char host[100];
char prefix[100];

int main(int argc,char **argv)  
{
    int sockfd;
    int res=0;
    struct sockaddr_in servaddr;  
    
    if(argc!=2){  
        printf("useage:./client <IPaddress>\n");  
        exit(1);  
    }
    bzero(host,sizeof(host));
    strcpy(host,argv[1]);
    
    help();
    
    sockfd=socket(AF_INET,SOCK_STREAM,0);
  
    bzero(&servaddr,sizeof(servaddr));  
    servaddr.sin_family=AF_INET;  
    servaddr.sin_port=htons(PORT);  
    inet_pton(AF_INET,argv[1],&servaddr.sin_addr);  
  
    res=connect(sockfd,(SA *)&servaddr,sizeof(servaddr));  
    if(res==-1){
        perror("connect error");
    }
    
    ftp_cli(stdin,sockfd);  
  
    exit(0);  
}

void ftp_cli(FILE *fp,int sockfd){  
    int maxfdp1;  
    fd_set rset;  
    char buf[MAXLINE];  //存储客户端发送的信息
    char sbuf[MAXLINE]; //存储接收服务器发来的信息
    char *arg;
    int n,res;  
    int out;
    int sockdatafd; //传输文件连接的描述符
    int nread;
    char block[1024];
    char tmp[100];
    struct sockaddr_in servdata;
    socklen_t servlen;
    
    bzero(&servdata,sizeof(servdata));
    bzero(buf,sizeof(buf));
    bzero(sbuf,sizeof(sbuf));
    
    //验证用户名,密码
    inputId();
    write(sockfd,key,strlen(key));
        
    FD_ZERO(&rset);  
    
    for(;;){
        FD_SET(fileno(fp),&rset);
        FD_SET(sockfd,&rset);
        
        maxfdp1=max(fileno(fp),sockfd)+1;  
        select(maxfdp1,&rset,NULL,NULL,NULL); 
            
        //----------------处理套接字-------------------
        if(FD_ISSET(sockfd,&rset)){ 
            bzero(sbuf,sizeof(sbuf));
            n=read(sockfd,sbuf,sizeof(sbuf));
            if(n==-1)   //读出错
                continue;
            else if(n==0){  //服务器断开
                printf("server terminated!\n");
                exit(0);
            }
            
            //login错误，重新验证用户名，密码
            if(strncmp(sbuf,"error 001",9)==0){
                memset(tmp,0,sizeof(tmp));
                printf("%s\n",sbuf);
                printf("input b to exit,c to continue:");
                scanf("%s",tmp);
                if(strncmp(tmp,"b",1)==0){
                    write(sockfd,buf,n);
                    shutdown(sockfd,SHUT_WR);
                    exit(0);
                }else{
                    inputId();
                    write(sockfd,key,strlen(key));
                    bzero(sbuf,sizeof(sbuf));
                    continue;
                }
            }
            //下载文件
            else if(strncmp(buf,"get",3)==0){
                //获取文件名
                arg=strtok(buf," ");
                arg=strtok(NULL," ");
                arg[strlen(arg)-1]='\0';
                if(access(arg,0)==0){   //access用于检查文件是否已经存在
                    printf("the file %s is already exist!\n",arg);
                    
                    print_user();
                    continue;
                }
                //获取套结字，连接
                servlen=sizeof(servdata);
                bzero(&servdata,servlen);
                getpeername(sockfd,(struct sockaddr *)&servdata,&servlen);
                servdata.sin_port=htons(atoi(sbuf));    //passive mode下服务器将端口号发送给客户端，让客户端去连接
                sockdatafd=socket(AF_INET,SOCK_STREAM,0);
                do{
                    n=connect(sockdatafd,(struct sockaddr *)&servdata,sizeof(servdata));
                }while(errno==ECONNREFUSED);
                if(n==-1){
                    printf("connect error!\n");
                    close(sockdatafd);
                    bzero(buf,sizeof(buf));
                    continue;
                }
                //文件下载
                out=open(arg,O_CREAT|O_WRONLY,S_IRUSR|S_IWUSR); 
                while((nread=read(sockdatafd,block,sizeof(block)))>0){  
                    if(write(out,block,nread)==-1){
                        printf("write to file error!\n");
                        break;
                    }
                }
                //收尾
                close(out);
                close(sockdatafd);
                bzero(buf,sizeof(buf));
            }
            //上传文件
            else if(strncmp(buf,"put",3)==0){ 
                //获取文件名
                arg=strtok(buf," ");
                arg=strtok(NULL," ");
                arg[strlen(arg)-1]='\0';
                if(access(arg,0)!=0){
                    printf("the file %s is not exist!\n",arg);
                    print_user();
                    continue;
                }
                //获取套结字，连接
                servlen=sizeof(servdata);
                bzero(&servdata,servlen);
                getpeername(sockfd,(struct sockaddr *)&servdata,&servlen);
                servdata.sin_port=htons(atoi(sbuf));
                sockdatafd=socket(AF_INET,SOCK_STREAM,0);
                do{
                    n=connect(sockdatafd,(struct sockaddr *)&servdata,sizeof(servdata));
                }while(errno==ECONNREFUSED);
                if(n==-1){
                    printf("connect error!\n");
                    close(sockdatafd);
                    bzero(buf,sizeof(buf));
                    continue;
                }
                //文件上传
                out=open(arg,O_RDONLY);
                while((nread=read(out,block,sizeof(block)))>0){
                    if(write(sockdatafd,block,nread)==-1){
                        printf("write error!\n");
                        break;
                    }
                }
                //收尾
                close(out);
                close(sockdatafd);
                bzero(buf,sizeof(buf));
            }
            //一般命令处理：直接将服务端返回结果输出
            else{
                write(fileno(stdout),sbuf,n);
                bzero(sbuf,sizeof(sbuf));
                print_user();
            }
        }  
            
        //-------------处理标准输入------------------
        if(FD_ISSET(fileno(fp),&rset)){  
            bzero(buf,sizeof(buf));
            n=read(fileno(fp),buf,sizeof(buf));
            if(n==-1||(n==1&&buf[0]=='\n')){
                print_user();
                continue;
            }
                
            buf[n]='\0';    //替换尾部的换行，方便下面ls,pwd的调用
            
            //处理本地指令
            if(strncmp("ls",buf,2)==0||strncmp("pwd",buf,3)==0){
                res=system(buf);    //调用子进程运行本地指令
                if(res!=0){
                    printf("system error!\n");
                    continue;
                }
                print_user();
            }else if(strncmp(buf,"help",4)==0){
                help();
                print_user();
            }else if(strncmp(buf,"bye",3)==0){
                write(sockfd,buf,n);
                strcpy(buf,"Thank you for using!\n");
                write(fileno(stdout),buf,strlen(buf));
                shutdown(sockfd,SHUT_WR);   //不用close(sockfd);
                exit(0);
            }else{  //处理其他指令：直接发送到服务端
                write(sockfd,buf,n);
            }
        }  
    }  
}  

void help(){
    printf("######Welcome To Use Myftp!######\nMyftp support those simple commands:\nls,pwd,show,where,go,get,put,help,adddir,deletedir,delete,bye\n");
}

void inputId(){
    char pwd[100];
    memset(user,0,sizeof(user));
    memset(pwd,0,sizeof(pwd));
    memset(key,0,sizeof(key));
    
    printf("name:");
    scanf("%s",user);
    printf("password:");
    scanf("%s",pwd);
    
    strcpy(key,"login ");
    strcat(key,user);
    strcat(key," ");
    strcat(key,pwd);
    strcat(key," ");
}

void print_user(){
    char tmpbuf[100];
    bzero(tmpbuf,sizeof(tmpbuf));
    sprintf(tmpbuf,"%s@%s >",user,host);
    write(fileno(stdout),tmpbuf,strlen(tmpbuf));
}
