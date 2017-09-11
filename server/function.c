#include "server.h"

extern int epollfd;
extern int islogin;  //用于判断用户是否登陆
extern char result[1024];  //存储返回给客户端的消息
extern char dirname[1024]; //存储目录信息
extern char filename[1024];    //存储文件信息
extern char username[100]; //存储登陆用户名
extern char workpath[1024];    //存储用户工作目录
extern int ftp_listenfd;    //监听套结字
extern int ftp_connfd;  //连接套结字
extern char ip_addr[100];  //存储用户输入的ip地址

void execute_cmd(char *);
void where();
void show();
int list_dir();
void add_dir(char *);
void delete_dir(char *);
int is_dir(char *);
void login(char *,char *);
void bye();
void go(char *);
void put(char *);
void get(char *);
void delete(char *);
void close_ftpdata(char *);

void execute_cmd(char *cmdstr){
    char *tmpcmd;
    char *tmparg;
    char *tmparg1;
    tmpcmd=strtok(cmdstr," ");
    tmparg=strtok(NULL," ");
    tmparg1=strtok(NULL," ");
    
    if(strncmp(tmpcmd,"where",5)==0){
        where();
    }else if(strncmp(tmpcmd,"show",4)==0){
        show();
    }else if(strncmp(tmpcmd,"adddir",6)==0){
        add_dir(tmparg);
    }else if(strncmp(tmpcmd,"deletedir",9)==0){
        delete_dir(tmparg);
    }else if(strncmp(tmpcmd,"login",5)==0){
        login(tmparg,tmparg1);
    }else if(strncmp(tmpcmd,"bye",3)==0){
        bye();
    }else if(strncmp(tmpcmd,"go",4)==0){
        go(tmparg);
    }else if(strncmp(tmpcmd,"put",3)==0){
        put(tmparg);
        close_ftpdata(tmpcmd);
    }else if(strncmp(tmpcmd,"get",3)==0){
        get(tmparg);
        close_ftpdata(tmpcmd);
    }else if(strncmp(tmpcmd,"delete",6)==0){
        delete(tmparg);
    }else{
        strcpy(result,"command not found!\n");
    }
}

//上传文件
void put(char *str){
    int out=-1;
    int nread=0;
    char block[1024];
    if(access(str,0)==0){
        sprintf(result,"the file %s is already exists!\n",str);
        return ;
    }
    out=open(str,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
    if(out==-1){
        sprintf(result,"create file %s fail!\n",str);
        return ;
    }
    while((nread=read(ftp_connfd,block,sizeof(block)))>0){
        if(write(out,block,nread)==-1){
            sprintf(result,"write file %s fail!\n",str);
            return ;
        }
    }
    close(out);
    sprintf(result,"upload file %s success!\n",str);
}

//下载文件
void get(char *str){
    int in=0;
    int nread=0;
    char block[1024];
    if(access(str,0)!=0){
        sprintf(result,"the file %s is not exists!\n",str);
        return ;
    }
    
    in=open(str,O_RDONLY);
    if(in==-1){
        sprintf(result,"open file %s error!\n",str);
        return ;
    }
    
    while((nread=read(in,block,sizeof(block)))>0){
        if((write(ftp_connfd,block,nread))==-1){
            sprintf(result,"write to socket error!\n");
            return ;
        }
    }
    sprintf(result,"download file %s success!\n",str);
}

//传输完成，撤销对相关套结字的监听
void close_ftpdata(char *str){
    if(epoll_ctl(epollfd,EPOLL_CTL_DEL,ftp_listenfd,NULL)==-1){
        return ;
    }
    if(epoll_ctl(epollfd,EPOLL_CTL_DEL,ftp_connfd,NULL)==-1){
        return ;
    }
    if(strncmp(str,"get",3)==0){
        shutdown(ftp_connfd,SHUT_WR);
    }else{
        shutdown(ftp_connfd,SHUT_RD);
    }
    close(ftp_listenfd);
}

//服务端的cd命令
void go(char *str){
    char pwd[256];
    
    if(str[0]=='/'){    
        strcpy(result,"permission denied!\n");
        return ;
    }
    
    if(strcmp("..",str)==0){
        getcwd(pwd,sizeof(pwd));
        if(strcmp(pwd,workpath)<=0){
            strcpy(result,"permission denied!\n");
            return ;
        }
        chdir(str);
        sprintf(result,"move to %s success!\n",str);
    }else{
        getcwd(pwd,sizeof(pwd));
        strcat(pwd,"/");
        strncat(pwd,str,strlen(str));
        if(is_dir(pwd)){
            chdir(pwd);
            sprintf(result,"move to %s success!\n",str);
        }else{
            strcpy(result,"no such directory!\n");
        }
    }
}

void bye(){
    //close(connfd);
    bzero(username,sizeof(username));
    exit(0);
}

//服务端的pwd指令
void where(){
    if(!getcwd(result,sizeof(result))){
        strcpy(result,"getpwd error!\n");
        return ;
    }
    strncat(result,"\n",1);
    return ;
}

//服务端的ls命令
void show(){
    if(list_dir()==-1){
        return ;
    }else{
        strcpy(result,"D:");
        strncat(result,dirname,strlen(dirname));
        strcat(result,"\nF:");
        strncat(result,filename,strlen(filename));
        strcat(result,"\n");
    }
}

int list_dir(){
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    char strbuf[50];
    char *str;
    str=getcwd(strbuf,sizeof(strbuf));
    
    if((dp=opendir(str))==NULL){
        sprintf(result,"can't open directory:%s\n",str);
        return -1;
    }
    
    bzero(dirname,sizeof(dirname));
    bzero(filename,sizeof(filename));
    
    while((entry=readdir(dp))!=NULL){
        lstat(entry->d_name,&statbuf);
        if(S_ISDIR(statbuf.st_mode)){
            if(strcmp(".",entry->d_name)==0||strcmp("..",entry->d_name)==0)
                continue;
            strncat(dirname,entry->d_name,strlen(entry->d_name));
            strncat(dirname,"  ",2);
        }else{
            strncat(filename,entry->d_name,strlen(entry->d_name));
            strncat(filename,"  ",2);
        }
    }
    closedir(dp);
    return 0;
}

//服务端的mkdir命令
void add_dir(char *str){
    if(is_dir(str)){
        sprintf(result,"directory %s is already exist!\n",str);
        return ;
    }
    if(mkdir(str,0755)==0){
        sprintf(result,"make directory %s success!\n",str);
    }else{
        sprintf(result,"make directory %s failed!\n",str);
    }
}

//服务端的rmdir命令
void delete_dir(char *str){
    if(!is_dir(str)){
        sprintf(result,"directory %s not exist!\n",str);
        return ;
    }
    if((rmdir(str))==0){
        sprintf(result,"delete directory %s success!\n",str);
    }else{
        sprintf(result,"delete directory %s error!\n",str);
    }
}

//服务端的rm命令
void delete(char *str){
    if(access(str,0)!=0){
        sprintf(result,"the file %s is not exist!\n",str);
        return ;
    }
    
    if(unlink(str)!=0){
        sprintf(result,"delete file %s fail\n",str);
        return ;
    }else{
        sprintf(result,"delete file %s success\n",str);
        return ;
    }
}

//判断是否存在该目录
int is_dir(char *str){
    return (opendir(str)==NULL)?0:1;
}

void login(char *user,char *pwd){
    char line[100];
    char *tuser,*tpwd;
    FILE *fp=NULL;
    if((fp=fopen("passwd","r"))==NULL){
        strcpy(result,"error 001:user configuration file open failed!\n");
        return ;
    }
    
    while(!feof(fp)){
        fgets(line,sizeof(line),fp);
        tuser=strtok(line,":");
        if(strcmp(user,tuser)==0){
            tpwd=strtok(NULL," ");
            if(strncmp(pwd,tpwd,strlen(tpwd)-1)==0){
                strcpy(username,user);
                islogin=1;
                sprintf(result,"hello,%s!\n",user);
                //进入用户目录 
                {
                    char pwd[1024];
                    getcwd(pwd,sizeof(pwd));
                    strcat(pwd,"/");
                    strncat(pwd,user,sizeof(user));
                    strcpy(workpath,pwd);
                    if(!is_dir(pwd))
                         mkdir(user,0755);
                    chdir(pwd);
                }
                
                fclose(fp);
                return ;
            }else{
                strcpy(result,"error 001:sorry,incorrect passwd!\n");
                return ;
            }
        }
    }
    sprintf(result,"error 001:sorry,unknow user %s!\n",user);
    fclose(fp);
}
