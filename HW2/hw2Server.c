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
#define USER_MAX 10
#define MAX_ONLINE
#define on 1
#define off 0



typedef struct//used to record user's info, 0 is for broadcast, 1~n is for user
{      
    int enable; //0 is offlilne
    char username[100];
    int pid;
}ClientNode;

typedef struct//傳送訊息用
{     
    int enable;//if message have not been read, enable is 1.
    int sender;// record sender's user number
    char detail[MESSAG_MAX];
}MessageNode;


typedef struct//傳送檔案用
{
    int enable;
    int sender;
    char filename[MESSAG_MAX];
}FileNode;

ClientNode list[100];
MessageNode message[USER_MAX+1]; //0 is public channel for broadcast ,1~USER_MAX is private channel
FileNode file[USER_MAX+1];

int count=1; //紀錄有幾個使用者登入了。因為0永來廣播，所以從1開始。
int broadcast[USER_MAX+1];
pthread_mutex_t mutex;


void initNode()
{

    sprintf(list[0].username,"Broadcast");
    list[0].pid = 0;
    list[0].enable=1;

    for(int i=1;i<100;i++) 
    {
        list[i].username[0] = '\0';
        list[i].pid = 0;
        list[i].enable=0;
    }

    for(int i=0;i<USER_MAX+1;i++) 
    {
        message[i].enable = 0;
        message[i].sender = 0;
        file[i].enable = 0;
        file[i].sender = 0;
        memset(message[i].detail,'\0',MESSAG_MAX);
        memset(file[i].filename,'\0',MESSAG_MAX);
        broadcast[i] = 0;
    }
}


//send "*" to notify client that this is end of data
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
        sprintf(tmp,"[MESSAGE] %s: %s\n",list[message[clientNumber].sender].username,message[clientNumber].detail);
        send(fd,tmp,BUFFER_MAX,0);
        
        pthread_mutex_lock(&mutex);
        message[clientNumber].enable=0;
        pthread_mutex_unlock(&mutex);
    }

    if(broadcast[clientNumber]==1)
    {
        sprintf(tmp,"[BROADCAST] %s: %s",list[message[0].sender].username,message[0].detail);
        send(fd,tmp,BUFFER_MAX,0);

        pthread_mutex_lock(&mutex);
        broadcast[clientNumber]=0; //已經得到廣播訊息的client會設為0
        pthread_mutex_unlock(&mutex);
    }
}


void sendFile(int fd, int clientNumber)
{
    if(file[clientNumber].enable == 0) return;

    int openfd;
    char tmp[BUFFER_MAX+1];
    ssize_t ret;

    //send "recvfile" so that client can start recv mode
    sprintf(tmp,"recvFile");
    ret = send(fd,tmp,BUFFER_MAX,0);

    //send file sender and filename to client for opening file
    sprintf(tmp,"%d",file[clientNumber].sender);
    send(fd,tmp,BUFFER_MAX,0);

    sprintf(tmp,"%s",file[clientNumber].filename);
    send(fd,tmp,BUFFER_MAX,0);

    //send file
    openfd = open(file[clientNumber].filename,O_RDONLY);

    while( (ret=read(openfd,tmp,BUFFER_MAX)) > 0)
    {
        ret = send(fd,tmp,ret,0);
        printf("SENDED: %ld\n",ret);
    }

    close(openfd);

    printf("Finish file transfer\n");

    pthread_mutex_lock(&mutex);
    file[clientNumber].enable=0;
    pthread_mutex_unlock(&mutex);
}

void socketHandle(void *connectFmptr)
{
    int fd = *((int*)connectFmptr),openfd=0,clientNumber=0;
    int talkMode=off,fileMode,receiver=0, fileMessageSwitch=1;
    char buff[BUFFER_MAX+1],command[BUFFER_MAX+1],tmp[BUFFER_MAX+1],filename[BUFFER_MAX+1];
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
        send(fd,tmp,sizeof(tmp),0);

        exit(-1);
    }

    printf("---------------------------------------------\n");


    pthread_mutex_lock(&mutex);
    
    strncpy(list[count].username,buff,strlen(buff));
    list[count].pid = getpid();
    list[count].enable=1;
    clientNumber = count;
    //printf("count:%d in %s %d\n",count,list[count].username,list[count].pid);

    count++;

    pthread_mutex_unlock(&mutex);

    //send login info to client
    sprintf(tmp,"Login Successfully\n");
    send(fd,tmp,strlen(tmp),0);


    while((ret=recv(fd, buff, BUFFER_MAX,0))>0)
    {
        memset(command,0,sizeof(command));
        memset(tmp,0,sizeof(tmp));
        memcpy(command,buff,sizeof(buff));

        //client會按下ENTER(送出newline)以獲取訊息，server會回傳"."和message(如果有的話)給client
        if(!strncmp(command,"\n",1)) 
        {
            sprintf(tmp,".");
            ret = send(fd,tmp,BUFFER_MAX,0);

            if(fileMessageSwitch == 1)
            {
                sendMessage(fd, clientNumber);
                fileMessageSwitch=0;
            }
            else
            {
                sendFile(fd, clientNumber);
                fileMessageSwitch=1;
            }

            endSend(fd);
            
            continue;
        }

        printf("[%s's Request] Length:%ld %s\n",list[clientNumber].username,strlen(command),command);

        if(talkMode==on)
        {
            //Format "Receiver Number:Message"
            //cut string with token ":" for syntax checking
   
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

            // Check if receiver is ligel.
            // Illegal condition includes sending message to itself or someone offline.
            if(receiver>USER_MAX || receiver<0 || receiver==clientNumber || list[receiver].enable==0)
            {
                sprintf(tmp,"[Error] Illegal receiver\n");
                ret = send(fd,tmp,BUFFER_MAX,0);
                ret = endSend(fd);
                talkMode=off;
                continue;
            }
            else if(receiver==0)// for broadcast
            {
                for(int i=0;i<USER_MAX+1;i++) broadcast[i]=1;
            }

            pthread_mutex_lock(&mutex);
            message[receiver].enable = 1;
            message[receiver].sender = clientNumber;
            strcpy(message[receiver].detail,mptr);
            pthread_mutex_unlock(&mutex);

            talkMode=off;
            sprintf(tmp,"Send Successfully\n");   
        }
        else if(fileMode==on) // recv file from client
        {
            command[1]='\0';

            receiver = atoi(command);

            printf("[Receiver] %d\n",receiver);

            if(receiver>USER_MAX || receiver<1 || receiver==clientNumber)
            {
                sprintf(tmp,"[Error] Illegal receiver\n");
                ret = send(fd,tmp,BUFFER_MAX,0);
                ret = endSend(fd);
                fileMode=off;
                continue;
            }

            if( (ret=read(fd, buff, BUFFER_MAX)) > 0)
            {
                strcpy(filename,buff);
            }

            openfd = open(filename, O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG);

            if(openfd<0)
            {
                perror("FILE");
                sprintf(tmp,"[Error] Server cannot receive file.\n");
                ret = send(fd,tmp,BUFFER_MAX,0);
                ret = endSend(fd);
                fileMode=off;
                continue;
            }

            printf("[filename] %s\n",filename);

            while( (ret=recv(fd,buff,BUFFER_MAX,0)) > 0)
            {

                write(openfd,buff,ret);
                printf("RECV: %zu\n",ret);
                printf("%s\n",buff);
                if(ret<BUFFER_MAX) break;
            }

            close(openfd);
             
            pthread_mutex_lock(&mutex);
            file[receiver].enable=1;
            file[receiver].sender=clientNumber;
            strcpy(file[receiver].filename,filename);
            pthread_mutex_unlock(&mutex);

            fileMode=off;

            printf("Receive file successfully.\n");
            sprintf(tmp,"Send file successfully.\n");
        }
        else if(!strncmp(command,"list\n",5))
        {
            printf("[Action] List all username.\n");

            memset(buff,'\0',BUFFER_MAX+1);

            for(int i=0;i<count;i++) 
            {
                sprintf(buff,"%2d: %s\n",i,list[i].username);
                strcat(tmp,buff);
            }

            printf("%s\n",tmp);         
        }
        else if(!strncmp(command,"file\n",5))
        {
            sprintf(tmp,"fileMode");
            fileMode=on;
        }
        else if(!strncmp(command,"talk\n",5))
        {
            sprintf(tmp,"%s\n %s",
                "GET Who is receiver? What is message?",
                "Format [receiver]:[message]");

            talkMode=on;
        }
        else if(!strncmp(command,"exit\n",5))
        {
            sprintf(tmp,"[Offline] %s",list[clientNumber].username);
            strcpy(list[clientNumber].username,tmp);
            list[clientNumber].enable=0;
            sprintf(tmp,"EXIT\n");
            break;  
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

    printf("%s's thread dead !!\n",list[clientNumber].username);
    return;
}


int main()
{
	
    int listenfd, connfd, yes=1, userNumber=0;
    socklen_t addr_size;
    struct sockaddr_in ClientInfo;
    struct addrinfo hints,*res;
    char tmpstr[BUFFER_MAX];
    pthread_t thread[USER_MAX];
    
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

        pthread_create(&thread[userNumber],NULL,(void *)socketHandle,&connfd);

        userNumber++;
    }

    for(int i=0,i<userNumber,i++) pthread_join(thread[i],NULL);
       
    return 0;
}
