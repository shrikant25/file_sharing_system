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
    int ServerPort = 7000;
    struct sockaddr_in remote= {0};
    
    remote.sin_addr.s_addr = INADDR_ANY; //Local Host
    remote.sin_family = AF_INET;
    remote.sin_port = htons(ServerPort);
    iRetval = connect(hSocket,(struct sockaddr *)&remote,sizeof(struct sockaddr_in));
    
    return iRetval;
}

int SocketSend(int hSocket,char* Rqst,short lenRqst){
    
    int shortRetval = -1;
   

    shortRetval = send(hSocket, Rqst, lenRqst, 0);
    
    return shortRetval;
}


int main(int argc, char *argv[])
{
    int hSocket, read_size;
    struct sockaddr_in server;
    char SendToServer[100] = {0};
    
    hSocket = SocketCreate();
    if(hSocket == -1){
        printf("Could not create socket\n");
        return 1;
    }
    printf("Socket is created\n");
   

    if (SocketConnect(hSocket) == -1){
        perror("connect failed.\n");
        return 1;
    }
    
    printf("Sucessfully conected with server\n");
   
        printf("Enter the Message: ");
        scanf("%s", SendToServer);
        
        SocketSend(hSocket, SendToServer, strlen(SendToServer));
   
    close(hSocket);
    
    shutdown(hSocket,2);
    
    return 0;
}