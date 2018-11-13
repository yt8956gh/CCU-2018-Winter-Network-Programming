#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <arpa/inet.h>
#include <signal.h>

#define BUFFSIZE 8192
#define on 1
#define off 0



struct FileTypeLinker{
    char *fileExt;
    char *returnSytax;
};

struct FileTypeLinker extensions [] = 
{
    {"gif",     "image/gif" },
    {"jpg",     "image/jpeg"},
    {"jpeg",    "image/jpeg"},
    {"png",     "image/png" },
    {"ico",     "image/ico" },
    {"html",    "text/html" },
    {0,0} 
}; 

void *get_in_addr(struct sockaddr *sa) {
  return sa->sa_family == AF_INET
    ? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
    : (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void socketHandle(int fd)
{
    int ret=0,openfd=0,extSize = sizeof(extensions)/sizeof(struct FileTypeLinker);
    int openFileSwitch=off;
    char	buff[BUFFSIZE+1],*ptr=NULL,*returnFileType;
    char filename[1024];

    memset(filename,0,sizeof(filename));


    ret=read(fd, buff, sizeof(buff));

    if(ret==0 || ret==-1) return;
    
    if(ret>0&&ret<BUFFSIZE)
    {
        buff[ret]='\0';
    }
    else buff[0]='\0';//when buffer overflows, it will be abandoned. 


    for(int i=0;i<ret;i++) //remove all except GET instruction
    {
        if(buff[i]=='\r'||buff[i]=='\n')
        {
            buff[i]='\0';
            break;
        }
    }


    if( strncmp(buff,"GET ",4) && strncmp(buff,"get ",4) ) //check format
    {
        return;
    } 

    ptr = buff+4;

    for(int i=4;i<ret;i++) //remove remain string except "Get /filename"
    {
        if(buff[i]==' ')
        {
            buff[i]='\0';
            break;
        }
    }

    printf("[Request Filter]:%s\n",buff);

    //choose filenames
    if (!strncmp(&buff[0],"GET /\0",6)||!strncmp(&buff[0],"get /\0",6) ) 
    {
        strcpy(filename,"index.html\0"); // if syntax match fotmat "GET /",server return index.html
    }
    else if (!strncmp(&buff[0],"GET /favicon.ico\0",17)||!strncmp(&buff[0],"get /favicon.ico\0",17) )
    {
        strcpy(filename,"favicon.ico\0"); // return icon of web
    }
    else if( (ptr=strstr(&buff[5], "/")) != NULL)
    {
        ptr++;
        strcpy(filename,ptr);
    }
    else if (!strncmp(&buff[0],"GET /..\0",8)||!strncmp(&buff[0],"get /..\0",8) )
    {
        write(fd, "Failed to open file", 19); // forbid browser to trace parent directory
        return;
    }
    else
    {
        write(fd, "Failed to open file", 19);
        return;
    }

    // avoid Ambiguous Request e.g. request filename without file extensions
    if((ptr=strstr(filename, ".")) == NULL)
    {
        write(fd, "Failed to open file", 19);
        fprintf(stderr,"[SERVER] Browser Gives Ambiguous Request\n");          
    }

    printf("Request File Type: %s\n",++ptr);

    //prepare Content-Types 
    for(int i=0;i<extSize;i++)
    {       
        if(strcmp(ptr,extensions[i].fileExt)==0)
        {
            returnFileType = extensions[i].returnSytax;
            break;
        }
    }

    //return HTTP Status Code to Browser
    sprintf(buff,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", returnFileType);
    write(fd,buff,strlen(buff));

    printf("[Server's Response] %s\n", buff);

    //open file and write it to client

    printf("Open: %s\n",filename);
    openfd = open(filename,O_RDONLY);

    if(openfd==-1) write(fd, "Failed to open file", 19);

    while( (ret=read(openfd,buff,BUFFSIZE)) > 0)
    {
        write(fd,buff,ret);
    }
}



int main()
{
	
	int	listenfd, connfd, fdmax, addrlen, yes=1;
    socklen_t addr_size;
    struct sockaddr_in clientInfo,servaddr;
    struct sockaddr_storage remoteaddr; // client address
    struct addrinfo hints,*res;
    char tmpstr[BUFFSIZE];
    pid_t childPID;

    fd_set enable_connect_set;
    fd_set enable_read_set;
    

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL,"80",&hints,&res);

    if( (listenfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol)) <0) // 0 for any
    {
        perror("Socket Error:");
        exit(-1);
    } 

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if( (bind(listenfd, res->ai_addr, res->ai_addrlen)) <0) // link specified port to kernel
    {
        perror("Bind Error:");
        exit(-1);
    }

    listen(listenfd, 1024); //max connection 


    signal(SIGCHLD,SIG_IGN);//通知kernel 「 parent不回收child，child由kernel回收 」

    // 將 listenfd 新增到 enable_connect_set set
    FD_SET(listenfd, &enable_connect_set);

    // 持續追蹤最大的 file descriptor
    fdmax = listenfd; // 到listernfd為止
    
    while(1) 
    {
        enable_read_set = enable_connect_set; // 複製 enable_connect_set

        if (select(fdmax+1, &enable_read_set, NULL, NULL, NULL) == -1) 
        {
            perror("[SERVER] Select");
            exit(4);
        }

        // 在現存的連線中尋找需要讀取的資料
        for (int i=0; i<=fdmax; i++) 
        {
            if (FD_ISSET(i, & enable_read_set)) // 找到一個!
            { 

                if (i == listenfd) 
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    connfd = accept(listenfd,(struct sockaddr * ) & remoteaddr, &addrlen);

                    if (connfd == -1)
                    {
                        perror("[SERVER] Accept");
                    } 
                    else 
                    {
                        FD_SET(connfd, & enable_connect_set); // 新增到 enable_connect_set
                        if (connfd > fdmax) { // 持續追蹤最大的 fd
                            fdmax = connfd;
                        }
                        printf("[SERVER] new connection from %s on socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                            get_in_addr((struct sockaddr * ) & remoteaddr),
                            tmpstr, INET6_ADDRSTRLEN),
                            connfd);
                    }

                } 
                else 
                {
                    // 處理來自 client 的資料
                    socketHandle(i);
                    FD_CLR(i, & enable_connect_set); // 從 enable_connect_set set 中移除
                    close(i); // bye!
                }
            }
        }
    }
    return 0;
}
