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
#include <ctype.h>

#define PORT "9527"
#define BUFFER_MAX 4096
#define MESSAG_MAX 2048
#define MAX_ONLINE
#define on 1
#define off 0



typedef struct
{       
    char username[100];
    int pid;

}ClientNode;

ClientNode list[100];

typedef struct
{     
    int enable;
    int sender;
    char detail[MESSAG_MAX];

}MessageNode;

MessageNode message[6]; //0 is public channel for broadcast ,1~5 is private channel


int count=1;
int broadcastRead[6];


pthread_mutex_t mutex;


void initNode()
{
    for(int i=0;i<100;i++) 
    {
        list[i].username[0] = '\0';
        list[i].pid = 0;
    }

    for(int i=0;i<6;i++) 
    {
        message[i].enable = 0;
        message[i].sender = 0;
        memset(message[i].detail,'\0',MESSAG_MAX);
    }

    memset(broadcastRead,'0',sizeof(broadcastRead));
}

ssize_t endSend(int fd)
{
    int ret=0;
    char tmp[BUFFER_MAX]={'\0'};

    memcpy(tmp,"*",2);
    return send(fd,tmp,BUFFER_MAX,0);
}

void socketHandle(void *connectFdPtr)
{
    int fd = *((int*)connectFdPtr),clientNumber=0;
    int openfd=0,answerMode=off,talkMode=off,receiver=0, broadcast=off;
    char	buff[BUFFER_MAX+1],command[BUFFER_MAX+1],tmp[BUFFER_MAX+1];
    char *ptr=NULL,*rptr, *dptr, *delim = ":";
    ssize_t  ret;
    ClientNode *NewNode=NULL,*nodeptr;


    printf("[Thread] New Thread's PID:%d\n",getpid());

    if( (ret=read(fd, buff, BUFFER_MAX)) > 0)
    {
        printf("[Client] Username: %s\n",buff);
    }
    else
    {
        strncpy(tmp,"Fail to receive username\n",27);
        fprintf(stderr,tmp,NULL);
        send(fd,tmp,strlen(tmp),0);

        exit(-1);
    }

    printf("---------------------------------------------\n");


    pthread_mutex_lock(&mutex);
    
    strncpy(list[count].username,buff,strlen(buff));
    list[count].pid = getpid();
    clientNumber = count;
    //printf("count:%d in %s %d\n",count,list[count].username,list[count].pid);

    count++;

    pthread_mutex_unlock(&mutex);


    sprintf(tmp,"Login Successfully\n");
    send(fd,tmp,strlen(tmp),0);


    while((ret=recv(fd, buff, BUFFER_MAX,0))>0)
    {
        if(message[clientNumber].enable == 1)
        {
            sprintf(tmp,"[%s] %s\n",list[message[clientNumber].sender].username,message[clientNumber].detail);
            ret = send(fd,tmp,BUFFER_MAX,0);
            ret = endSend(fd);

            message[clientNumber].enable=0;
        }

        if(broadcastRead[clientNumber]==0)
        {
            sprintf(tmp,"[%s] %s",list[message[0].sender].username,message[0].detail);
            ret = send(fd,tmp,BUFFER_MAX,0);
            ret = endSend(fd);

            pthread_mutex_lock(&mutex);
            broadcastRead[clientNumber]=1;
            pthread_mutex_unlock(&mutex);
        }

        memcpy(command,buff,strlen(buff));

        if(!strncmp(command,"\n",1))
        {
            sprintf(tmp,".");
            ret = send(fd,tmp,BUFFER_MAX,0);
            endSend(fd);
            continue;
        }

        printf("%s:%ld:%s\n",list[clientNumber].username,strlen(command),command);

        if(talkMode==on)
        {
            rptr = strtok(command,delim);
            dptr = strtok(NULL,delim);

            if(rptr==NULL||dptr==NULL)
            {
                sprintf(tmp,"illegal Syntax\n");
                ret = send(fd,tmp,BUFFER_MAX,0);
                ret = endSend(fd);
                talkMode=off;
                continue;
            }

            receiver = atoi(rptr);
            printf("receiver: %d\n",receiver);

            if(receiver>5 || receiver<0)
            {
                sprintf(tmp,"illegal receiver\n");
                ret = send(fd,tmp,BUFFER_MAX,0);
                ret = endSend(fd);
                talkMode=off;
                continue;
            }
            
            if(receiver==0)
            {
                for(int i=0;i<6;i++)
                {
                    broadcastRead[i]=0;
                }
            }

            message[receiver].enable = 1;
            message[receiver].sender = clientNumber;
            strcpy(message[receiver].detail,dptr);

            talkMode=off;
            sprintf(tmp,"Send Successfully\n");
            ret = send(fd,tmp,BUFFER_MAX,0);
        }

        if(!strncmp(command,"list\n",5))
        {
            printf("[Action] List all username\n");

            memset(buff,'\0',BUFFER_MAX+1);

            for(int i=1;i<count;i++) 
            {
                sprintf(tmp,"%2d: Username:%s\tPID:%d\n",i,list[i].username,list[i].pid);
                strcat(buff,tmp);
            }

            printf("%s\n",buff);
            ret = send(fd,buff,BUFFER_MAX,0);
            
        }
        else if(!strncmp(command,"file\n\0",6))
        {
            strncpy(tmp,"GET filename\0",14);
            ret = send(fd,tmp,BUFFER_MAX,0);
        }
        else if(!strncmp(command,"talk\n\0",6))
        {
            sprintf(tmp,"%s %s","GET who's receiver? What's message?\n"
                ,"Format [receiver]:[message]");
            ret = send(fd,tmp,BUFFER_MAX,0);
            talkMode=on;
        }
        else
        {
            sprintf(tmp,"illegal syntax\n");
            ret = send(fd,tmp,BUFFER_MAX,0);
        }
        ret = endSend(fd);
    }

    close(fd);
    return;

    /*
    if(openfd==-1) write(fd, "Failed to open file", 19);

    while( (ret=read(openfd,buff,BUFFER_MAX)) > 0)
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
    char tmpstr[BUFFER_MAX];
    pid_t childPID;
    pthread_t thread1,thread2,thread3,thread4,thread5;
    
    initNode();

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
            printf("[Server] Accept from port:%d IP:%s\n",ntohs(ClientInfo.sin_port),tmpstr);
        }

        userNumber++;

        if((userNumber%5)==1)
        {
            pthread_create(&thread1,NULL,(void *)socketHandle,&connfd);
        }
        else if((userNumber%5)==2)
        {
            pthread_create(&thread2,NULL,(void *)socketHandle,&connfd);
        }
        else if((userNumber%5)==3)
        {
            pthread_create(&thread3,NULL,(void *)socketHandle,&connfd);
        }
        else if((userNumber%5)==4)
        {
            pthread_create(&thread4,NULL,(void *)socketHandle,&connfd);
        }
        else if((userNumber%5)==0)
        {
            pthread_create(&thread5,NULL,(void *)socketHandle,&connfd);
        }
    }

    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
    pthread_join(thread3,NULL);
    pthread_join(thread4,NULL);
    pthread_join(thread5,NULL);
    

    return 0;
}
