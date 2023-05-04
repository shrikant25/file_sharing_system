#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libpq-fe.h>
#include "user_program.h"

PGconn *connection;

int import_file (char *file_path) 
{
    int fd = 0;
    char file_name[pathsize];
    size_t file_length = 0;
    char file_length_param[20];
 
    int pathlen = strlen(file_path);
    int i = pathlen-1;
    if (file_path[i] == '\n') {
        file_path[i] = '\0';
    }

    memset(file_name, 0, sizeof(pathsize));
    while (i > 0 && file_path[i--] != '/');
    strncpy(file_name, file_path+i+2, pathlen-i-1);

    if ((fd = open(file_path, O_RDONLY)) == -1) {
        printf("failed to open file :%s, error : %s\n",file_path, strerror(errno));
        return -1;
    }

    file_length = lseek(fd, 0, SEEK_END);
    close(fd);

    if (file_length <= 0) {
        printf("invalid file length %d %s, error : %s\n", file_length, file_name, strerror(errno));
        return -1;
    }

    sprintf(file_length_param, "%d", file_length);

    PGresult *res = NULL;
    const char *const param_values[] = {file_name, file_length_param, file_path};
    const int paramLengths[] = {sizeof(file_name), sizeof(file_length_param), sizeof(file_path)};
    const int paramFormats[] = {0, 0, 0};
    int resultFormat = 0;
    
    res = PQexecPrepared(connection, dbs[0].statement_name ,dbs[0].param_count, param_values, paramLengths, paramFormats, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("failed to store file %s, error : %s\n", file_name, PQerrorMessage(connection));
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;

}


int export_file (char *path) 
{
    printf("not implemented\n");
} 


int send_file (char* path)
{
    printf("not implemented\n");
}


int connect_to_database (char *conninfo) 
{   
    connection = PQconnectdb(conninfo);

    if (PQstatus(connection) != CONNECTION_OK) {
        printf("Connection to database failed: %s\n", PQerrorMessage(connection));
        return -1;
    }

    return 0;
}


int prepare_statements () 
{    
    int i, status = 0;

    for (i = 0; i<statement_count; i++){

        PGresult* res = PQprepare(connection, dbs[i].statement_name, dbs[i].statement, dbs[i].param_count, NULL);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
            status = -1;
            PQclear(res);
            break;
        }

        PQclear(res);
    }
    
    return status;
}


int get_input (char *buf, int bufsize) 
{
    printf("Enter absolute file path\n");
    memset(buf, 0, bufsize);

    if (fgets(buf, bufsize, stdin) != NULL) {
        return 0;
    }

    printf("Invalid Input\n");
    return -1;    
}


int main (int argc, char *argv[]) 
{

    char path[pathsize];    
    char confdata[confdatasize];
    char db_conn_command[db_conn_command_size];
    char username[conn_param_size];
    char dbname[conn_param_size];
    int conffd = -1;
    char choice1[3];

    if (argc != 2){
        printf("invalid arguments\n");
        return -1;
    }

    if((conffd = open(argv[1], O_RDONLY)) == -1) {
        printf("failed to open config file\n");
        return -1;
    }

    memset(confdata, 0, confdatasize);
    if(read(conffd, confdata, confdatasize) > 0){
        sscanf(confdata, "USERNAME=%s\nDBNAME=%s", username, dbname);
    }
    else {
        printf("failed to read config file");
        return -1;
    }

    close(conffd);

    sprintf(db_conn_command, "user=%s dbname=%s", username, dbname);

    if (connect_to_database(db_conn_command) == -1) { return -1;}
    if (prepare_statements() == -1) { return -1;}

    do {       
        printf("Enter 1 to import a file into database\n");
        printf("Enter 2 to send a file\n");
        printf("Enter 3 to export a file from database\n");
        printf("Enter 4 to close the program\n");
        fflush(stdin);
        memset(choice1, 0, sizeof(choice1));
        fgets(choice1, sizeof(choice1), stdin);    
    
        switch (choice1[0]) {
    
            case '4':   PQfinish(connection);
                        return 0;
                        break;
    
            case '1':   if(get_input(path, pathsize) != -1) {
                            import_file(path);
                        }
                        break;
        
            case '2':   if(get_input(path, pathsize) != -1) {
                            send_file(path);
                        }   
                        break;
            
            case '3':
                        if(get_input(path, pathsize) != -1) {
                            export_file(path);
                        }
                        break;
            
            default:    printf("Invalid Input\n");        
                        break;
        };

    } while(1);

}