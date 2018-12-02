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
#include <pthread.h>
#include <semaphore.h>

#define PORT "9527"
#define BUFFSIZE 8192
#define on 1
#define off 0



struct client
{       
    char username[100];
    int pid;
};

typedef struct client ClientNode;

ClientNode list[100];
int count=0;


pthread_mutex_t mutex;


void init_node()
{
    for(int i=0;i<100;i++) 
    {
        list[i].username[0] = '\0';
        list[i].pid = 0;
    }

}

void socketHandle(void *connectFdPtr)
{
    int fd = *((int*)connectFdPtr);
    int openfd=0;
    char	buff[BUFFSIZE+1],command[BUFFSIZE+1],tmp[BUFFSIZE+1];
    char *ptr=NULL,*delim = "\n";
    ssize_t  ret;
    ClientNode *NewNode=NULL,*nodeptr;


    printf("[Thread]\tNew Thread's PID:%d\n",getpid());

    if( (ret=read(fd, buff, BUFFSIZE)) > 0)
    {
        printf("[Client]\tUsername: %s\n",buff);
    }
    else
    {
        fprintf(stderr,"Fail to recieve username\n");
        exit(-1);
    }

    printf("---------------------------------------------\n");


    pthread_mutex_lock(&mutex);
    
    strncpy(list[count].username,buff,strlen(buff));
    list[count].pid = getpid();
    //printf("count:%d in %s %d\n",count,list[count].username,list[count].pid);

    count++;

    pthread_mutex_unlock(&mutex);


    while((ret=recv(fd, buff, sizeof(buff),0))>0)
    {
        printf("%ld:%s\n",strlen(buff),buff);

        memcpy(command,buff,strlen(buff));

        if(!strncmp(command,"list\n\0",6))
        {
            printf("<list all username>\n");

            for(int i=0;i<count;i++) 
            {
                printf("%2d: Username:%s\tPID:%d\n",i,list[i].username,list[i].pid);
            }

        }
        else if(!strncmp(command,"file\n\0",6))
        {
            strncpy(tmp,"who will recieve the transfered file ?\n\0",42);

            send(fd,tmp,strlen(tmp),0);
        }



    }


    close(fd);
    return;

    /*
    if(openfd==-1) write(fd, "Failed to open file", 19);

    while( (ret=read(openfd,buff,BUFFSIZE)) > 0)
    {
        write(fd,buff,ret);
    }
  
    close(fd);*/
}





int main()
{
	
    int listenfd, connfd, yes=1, userNumber=0;
    socklen_t addr_size;
    struct sockaddr_in ClientInfo,servaddr;
    struct addrinfo hints,*res;
    char tmpstr[BUFFSIZE];
    pid_t childPID;
    pthread_t thread1,thread2,thread3;
    
    init_node();

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL,PORT,&hints,&res);

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

        memset(&ClientInfo,0,sizeof(ClientInfo));
        addr_size = sizeof(ClientInfo);

        //accept 要塞入 struct sockaddr_in
        connfd = accept(listenfd, (struct sockaddr *) &ClientInfo, &addr_size); //abandon client's info

        if(connfd==-1) //accept fail
        {
            perror("Accept");
            continue;
        }
        else //printf client's info 
        {
            inet_ntop(AF_INET, &(ClientInfo.sin_addr), tmpstr, INET_ADDRSTRLEN);
            printf("[Server]\taccept from port:%d IP:%s\n",ntohs(ClientInfo.sin_port),tmpstr);
        }

        userNumber++;

        if((userNumber%3)==1)
        {
            pthread_create(&thread1,NULL,(void *)socketHandle,&connfd);
        }
        else if((userNumber%3)==2)
        {
            pthread_create(&thread2,NULL,(void *)socketHandle,&connfd);
        }
        else if((userNumber%3)==0)
        {
            pthread_create(&thread3,NULL,(void *)socketHandle,&connfd);
        }
    }

    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
    pthread_join(thread3,NULL);
    

    return 0;
}
