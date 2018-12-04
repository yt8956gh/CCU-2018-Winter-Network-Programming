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

typedef struct
{     
    int enable;
    int sender;
    char detail[MESSAG_MAX];
}MessageNode;



ClientNode list[100];
MessageNode message[6]; //0 is public channel for broadcast ,1~5 is private channel
int count=1; //紀錄有幾個使用者登入了
int broadcast[6];
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
        broadcast[i] = 0;
    }
}

ssize_t endSend(int fd)
{
    char tmp[BUFFER_MAX]={'\0'};

    memcpy(tmp,"*",2);
    return send(fd,tmp,BUFFER_MAX,0);
}

void sendMessage(int fd, int clientNumber)
{
    char tmp[BUFFER_MAX+1];

    if(message[clientNumber].enable == 1)
    {
        sprintf(tmp,"[MESSAGE:%d] %s: %s\n",message[clientNumber].sender,list[message[clientNumber].sender].username,message[clientNumber].detail);
        send(fd,tmp,BUFFER_MAX,0);
        
        pthread_mutex_lock(&mutex);
        message[clientNumber].enable=0;
        pthread_mutex_unlock(&mutex);
    }

    if(broadcast[clientNumber]==1)
    {
        sprintf(tmp,"[BROADCAST:%d] %s: %s",message[0].sender,list[message[0].sender].username,message[0].detail);
        send(fd,tmp,BUFFER_MAX,0);

        pthread_mutex_lock(&mutex);
        broadcast[clientNumber]=0; //已經得到廣播訊息的client會設為0
        pthread_mutex_unlock(&mutex);
    }
}

void socketHandle(void *connectFmptr)
{
    int fd = *((int*)connectFmptr),clientNumber=0;
    int talkMode=off,receiver=0;
    char buff[BUFFER_MAX+1],command[BUFFER_MAX+1],tmp[BUFFER_MAX+1];
    char *rptr=NULL, *mptr=NULL, *delim = ":";
    ssize_t  ret;


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
        memset(command,0,sizeof(command));
        memset(tmp,0,sizeof(tmp));
        memcpy(command,buff,sizeof(buff));

        //client會按下ENTER以獲取訊息，server會回傳"."和message(如果有的話)給client
        if(!strncmp(command,"\n",1)) 
        {
            sprintf(tmp,".");
            ret = send(fd,tmp,BUFFER_MAX,0);
            sendMessage(fd, clientNumber);
            endSend(fd);
            continue;
        }

        printf("[%s's Request] Length:%ld %s\n",list[clientNumber].username,strlen(command),command);

        if(talkMode==on)
        {
            //cut string with token ":" for syntax checking
            //Format "Receiver Number:Message"
            rptr = strtok(command,delim);
            mptr = strtok(NULL,delim);

            if(rptr==NULL||mptr==NULL)
            {
                sprintf(tmp,"[Error] Illegal Syntax\n");
                ret = send(fd,tmp,BUFFER_MAX,0);
                ret = endSend(fd);
                talkMode=off;
                continue;
            }

            receiver = atoi(rptr);
            printf("[Talk Syntax] receiver: %d\n",receiver);
            printf("[Talk Syntax] Message: %s\n",mptr);

            if(receiver>5 || receiver<0 || receiver==clientNumber)
            {
                sprintf(tmp,"[Error] Illegal receiver\n");
                ret = send(fd,tmp,BUFFER_MAX,0);
                ret = endSend(fd);
                talkMode=off;
                continue;
            }
            else if(receiver==0)
            {
                for(int i=0;i<6;i++)
                {
                    broadcast[i]=1;
                }
            }

            message[receiver].enable = 1;
            message[receiver].sender = clientNumber;
            strcpy(message[receiver].detail,mptr);

            talkMode=off;
            sprintf(tmp,"Send Successfully\n");   
        }
        else if(!strncmp(command,"list\n",5))
        {
            printf("[Action] List all username\n");

            memset(buff,'\0',BUFFER_MAX+1);

            for(int i=1;i<count;i++) 
            {
                sprintf(buff,"%2d: %s\n",i,list[i].username);
                strcat(tmp,buff);
            }

            printf("%s\n",tmp);         
        }
        else if(!strncmp(command,"file\n",5))
        {
            strncpy(tmp,"GET filename\0",14);
        }
        else if(!strncmp(command,"talk\n",5))
        {
            sprintf(tmp,"%s\n%s",
                "GET Who is receiver? What is message?",
                "Format [receiver]:[message]");

            talkMode=on;
        }
        else if(!strncmp(command,"exit\n",5))
        {
            sprintf(tmp,"[Offline] %s",list[clientNumber].username);
            strcpy(list[clientNumber].username,tmp);
            sprintf(tmp,"EXIT\n");  
        }
        else
        {
            sprintf(tmp,"[Error] Illegal Syntax\n");
        }

        ret = send(fd,tmp,BUFFER_MAX,0);
        ret = endSend(fd);
        printf("---------------------------------------------\n");
    }
    close(fd);
    return;
}


int main()
{
	
    int listenfd, connfd, yes=1, userNumber=0;
    socklen_t addr_size;
    struct sockaddr_in ClientInfo;
    struct addrinfo hints,*res;
    char tmpstr[BUFFER_MAX];
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
