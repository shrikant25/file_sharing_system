#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}


int set_server(int argc, char *argv[]){

    int sockfd, newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);   //create a socket

    if (sockfd < 0) error("ERROR opening socket");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);                     
    
    serv_addr.sin_family = AF_UNIX;         // set connection type
    serv_addr.sin_addr.s_addr = INADDR_ANY; //set IP address of machine 
    serv_addr.sin_port = htons(portno); // convert portno from host byte order to network byte order
    
    if ( bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr) ) < 0) // bind socket to a address 
        error("ERROR on binding");
    
    listen(sockfd,5);               //keep listening
    
    clilen = sizeof(cli_addr);      


    while(1){
            
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);  
        //once a connection is formed, use the returned file descriptor
    
        if (newsockfd < 0) error("ERROR on accept");
    
        int pid = fork();
        
        if (pid < 0) error("Failed to fork");

        if (pid == 0) {
            close(sockfd);
            work(newsockfd);
            exit(0);
        }
        else
            close(newsockfd);

    }

    // put following lines in work func
    
    
    /*
    n = write(newsockfd,"I got your message",18);
    if (n < 0) error("ERROR writing to socket");
    */

    return 0; 
}
