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


void socketHandle(int fd)
{
    int ret=0,openfd=0,extSize = sizeof(extensions)/sizeof(struct FileTypeLinker);
    int NoSyntaxCheck=off;
    char	buff[BUFFSIZE+1],*ptr=NULL,*returnFileType;
    char filename[1024];

    memset(filename,0,sizeof(filename));


    ret=read(fd, buff, sizeof(buff));

    if(ret==0 || ret==-1) exit(-1);
    
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
        exit(-1);
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

    //choose filename

    if (!strncmp(&buff[0],"GET /\0",6)||!strncmp(&buff[0],"get /\0",6) ) 
    {
        strcpy(filename,"index.html\0"); // if syntax match fotmat "GET /",server return index.html
    }
    else if (!strncmp(&buff[0],"GET /favicon.ico\0",17)||!strncmp(&buff[0],"get /favicon.ico\0",17) )
    {
        strcpy(filename,"favicon.ico\0"); // return icon of web
    }
    else if (!strncmp(&buff[0],"GET /..\0",8)||!strncmp(&buff[0],"get /..\0",8) )
    {
        write(fd, "Failed to open file", 19); // for forbid browser to trace parent directory
        exit(-1);
    }
    else if( (ptr=strstr(&buff[5], "/")) != NULL)
    {
        ptr++;
        strcpy(filename,ptr);
    }
    else
    {
        write(fd, "Failed to open file", 19);
        return;
    }

    // avoid Ambiguous Request e.g. filename without file extensions

    if( (ptr=strstr(filename, "."))==NULL )
    {
        write(fd, "Failed to open file", 19);
        fprintf(stderr,"Ambiguous Request\n");     
        exit(-1);       
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
  
    close(fd);
}



int main()
{
	
    int listenfd, connfd, yes=1;
    socklen_t addr_size;
    struct sockaddr_in clientInfo,servaddr;
    struct addrinfo hints,*res;
    char tmpstr[BUFFSIZE];
    pid_t childPID;

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



    //avoid Zombie Process in the way of notifying kernel that child will be recover by kernel not parent 
    signal(SIGCHLD,SIG_IGN);

    while(1){ 

        memset(&clientInfo,0,sizeof(clientInfo));
        addr_size = sizeof(clientInfo);

        //accept 要塞入 struct sockaddr_in
        connfd = accept(listenfd, (struct sockaddr *) &clientInfo, &addr_size); //abandon client's info

        if(connfd==-1) //accept fail
        {
            perror("Accept");
            continue;
        }
        else //printf client's info 
        {
            inet_ntop(AF_INET, &(clientInfo.sin_addr), tmpstr, INET_ADDRSTRLEN);
            printf("[Server's info]:accept from port:%d IP:%s\n",ntohs(clientInfo.sin_port),tmpstr);
        }

        if( (childPID=fork()) ==0)//child
        {
            close(listenfd); 
            socketHandle(connfd);  
            exit(0);
        }
        else close(connfd);
    }


    return 0;
}
