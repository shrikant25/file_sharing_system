#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>


short SocketCreate(void){
    
    short hSocket;
    
    printf("Create the socket\n");
    hSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    return hSocket;
}

int SocketConnect(int hSocket){
    
    int iRetval=-1;
    int ServerPort = 6000;
    struct sockaddr_in remote= {0};
    
    remote.sin_addr.s_addr = INADDR_ANY; //Local Host
    remote.sin_family = AF_INET;
    remote.sin_port = htons(ServerPort);
    iRetval = connect(hSocket,(struct sockaddr *)&remote,sizeof(struct sockaddr_in));
    
    return iRetval;
}

int SocketSend(int hSocket,char* Rqst,int lenRqst){
    
    int shortRetval = -1;
   
    shortRetval = send(hSocket, Rqst, lenRqst, 0);
    
    return shortRetval;
}


int main(int argc, char *argv[])
{
    int hSocket, read_size, idx;
    struct sockaddr_in server;
    int n = 1024 * 128;
    char SendToServer[n];
 

    hSocket = SocketCreate();
    if(hSocket == -1){
        printf("Could not create socket\n");
        return 1;
    }
    int optval = (1024*128)/2;
    
setsockopt(hSocket, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
    if (SocketConnect(hSocket) == -1){
        perror("connect failed.\n");
        return 1;
    }

    setsockopt(hSocket, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
int optval2 ;
socklen_t sz = sizeof(optval2);
   int ans = getsockopt(hSocket,  SOL_SOCKET, SO_SNDBUF, &optval2, &sz);

   printf("ans is %d %d\n", optval, optval2) ;
  	printf("done");
       for(idx = 0; idx<10; idx++) {
       
		memset(SendToServer, '0', n);

		memset(SendToServer, '^', n);
		SendToServer[n-1] ='\0';
		
		//printf("size %ld\n", sizeof(SendToServer));
		int val = SocketSend(hSocket, SendToServer, n);
printf("%d\n",val);
     		sleep(2);  
       }
    close(hSocket);
    
    shutdown(hSocket,2);
    exit(0);
}
