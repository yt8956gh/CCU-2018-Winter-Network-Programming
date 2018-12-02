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

#define BUFFER_MAX 8192
#define PORT "9527"

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char **argv)
{
    int sockfd,ret,sendSize;
    char buffer[BUFFER_MAX],tmp[BUFFER_MAX];
    struct addrinfo hints,*srvinfo;
    size_t numberByte=0;


    if(argc == 2)
    {
        printf("UserName: %s\n",argv[1]);
    }   
    else
    {
        fprintf(stderr,"Usage: client username");
        exit(-1);
    }


    memset(&hints,0,sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;


    if((ret = getaddrinfo("127.0.0.1",PORT,&hints,&srvinfo)) )
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		return 1;
    }

    if ((sockfd = socket(srvinfo->ai_family, srvinfo->ai_socktype,srvinfo->ai_protocol)) == -1)
    {
		perror("client: socket");
        exit(-1);
	}

	if (connect(sockfd, srvinfo->ai_addr, srvinfo->ai_addrlen) == -1)
    {
		perror("client: connect");
		close(sockfd);
        exit(-1);
	}

    inet_ntop(srvinfo->ai_family, get_in_addr((struct sockaddr *)srvinfo->ai_addr),tmp, sizeof(tmp));
	printf("client: connecting to %s\n", tmp);

	freeaddrinfo(srvinfo); // all done with this structure


    sendSize = sizeof(argv[1]);

    while((numberByte = write(sockfd,argv[1],sizeof(argv[1])))>0)
    {
        if(sendSize==0) break;
        sendSize-=numberByte;
        printf("%s\n",tmp);
    }

    while(fgets(tmp,BUFFER_MAX-1,stdin))
    {
        sendSize = sizeof(tmp);

        numberByte = write(sockfd,tmp,sizeof(tmp));
        // 因為fgets會依據參數BUFFER_MAX自動切字串，
        // 所以不需要擔心stdin會超過sizeof(tmp)，
        // 因此也不需要用while(numberByte = write(...))來傳資料

        continue;
    }


	close(sockfd);

	return 0;
}
