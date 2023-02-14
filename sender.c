#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> // contains important fucntionality and api used to create sockets
#include <sys/types.h>  // contains various types required to create a socket   
#include <netinet/in.h> // contains structures to store address information


int create_connection(unsigned short int port_number, unsigned int ip_address) {
    
    int network_socket; // to hold socket file descriptor
    int connection_status; // to show wether a connection was established or not
    struct sockaddr_in server_address; // create a address structure to store the address of remote connection
    /* 
        create a socket
       first parameter is domain of the socket = AF_INET represents that it is an internet socket
       second parameter is type of socket = SOCK_STREAM represents that it is a TCP sockets
       third parametets specifies that protocol that is being used, since we want to use default i.e tcp
       hence it will be set to 0 
    */
    network_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    // address of family 
    server_address.sin_family = AF_INET;    
    
    /*  
        port number
        htons converts the unsigned short from host byte order to network byte order
        host byte order is mainly big-endian, 
        while local machine may have little-endian byte order 
    */
    server_address.sin_port = htons(port_number);  

    /*  
        actual server address i.e Ip address 
        sin_addr is a structure that contains the field to hold the address
        
        in order to connect to local machine address can be 0000 or constant INADDR_ANY
    */
    server_address.sin_addr.s_addr = ip_address; 

    /*
        function to connect to remote machine
        first paratmeter is our socket
        second paratmeter is address structure, cast it to a different structure 
        third parameter is size of address structure

        connect returns a integer that will indicate wether connection was succesfull or not
    */
    connection_status = connect(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connection_status == -1) {
        printf("Error connecting to server\n");
    }

    return network_socket;
}


int close_connection(unsigned int network_sokcet) {
    close(network_sokcet);
}


int main(void) {


    //read ip, data from data base
    //search for socket in related to ip in database
    // send data
    //once done close the socket
    //provide 
    close(network_socket);
    
    return 0;
}