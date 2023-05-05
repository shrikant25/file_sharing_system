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
    char file_name[bufsize];
    PGresult *res = NULL;
    
    int pathlen = strlen(file_path);
    int i = pathlen-1;
    if (file_path[i] == '\n') {
        file_path[i] = '\0';
    }

    while (i > 0 && file_path[i--] != '/');

    memset(file_name, 0, sizeof(bufsize));
    strncpy(file_name, file_path+i+2, pathlen-i-1);

    const char *const param_values[] = {file_name, file_path};
    const int paramLengths[] = {sizeof(file_name), pathlen};
    const int paramFormats[] = {0, 0};
    int resultFormat = 0;

    res = PQexecPrepared(connection, dbs[1].statement_name ,dbs[1].param_count, param_values, paramLengths, paramFormats, resultFormat);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("failed to get file info %s, error : %s\n", file_name, PQerrorMessage(connection));
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) > 0) {
        printf("file with same name already exists\n");
        PQclear(res);
        return -1;
    }

    PQclear(res);

    res = PQexecPrepared(connection, dbs[0].statement_name ,dbs[0].param_count, param_values, paramLengths, paramFormats, resultFormat);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("failed to store file %s, error : %s\n", file_name, PQerrorMessage(connection));
        PQclear(res);
        return -1;
    }

    printf("File %s succesfully stored in database\n", file_name);

    PQclear(res);
    return 0;
}


int send_file (char *file_name, char *destination)
{
    int fd = 0;
    int len = 0;
    PGresult *res = NULL;

    len = strlen(file_name);
    if (file_name[len-1] == '\n') {
        file_name[len-1] = '\0';
    }

    len = strlen(destination);
    if (destination[len-1] == '\n') {
        destination[len-1] = '\0';
    }
    
    const char *const param_values[] = {file_name, destination};
    const int paramLengths[] = {strlen(file_name), strlen(destination)};
    const int paramFormats[] = {0, 0};
    int resultFormat = 0;

    // here param count is 1, so it is going to only use 1 parameter, rest will be ignored
    res = PQexecPrepared(connection, dbs[1].statement_name ,dbs[1].param_count, param_values, paramLengths, paramFormats, resultFormat);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("failed to get file info %s, error : %s\n", file_name, PQerrorMessage(connection));
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) > 1) {
        printf("multiple files with same name exists\n");
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        printf("file doesn't exists in database\n");
        PQclear(res);
        return -1;
    }
    
    PQclear(res);

    res = PQexecPrepared(connection, dbs[2].statement_name ,dbs[2].param_count, param_values, paramLengths, paramFormats, resultFormat);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("failed to create job for file sending %s, error : %s\n", file_name, PQerrorMessage(connection));
        PQclear(res);
        return -1;
    }

    printf("Job succesfully created for %s\n", file_name);

    PQclear(res);
    return 0;
}


int export_file (char *path) 
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


int get_input (char *prompt, char *buf, int bsize) 
{
    printf("%s", prompt);
    memset(buf, 0, bsize);

    if (fgets(buf, bsize, stdin) != NULL) {
        return 0;
    }

    printf("Invalid Input\n");
    return -1;    
}


int main (int argc, char *argv[]) 
{
    char choice1[3];
    char buf1[bufsize];
    char buf2[bufsize];    
    char db_conn_command[db_conn_command_size];
    char username[conn_param_size];
    char dbname[conn_param_size];
    int conffd = -1;

    if (argc != 2){
        printf("invalid arguments\n");
        return -1;
    }

    if((conffd = open(argv[1], O_RDONLY)) == -1) {
        printf("failed to open config file\n");
        return -1;
    }

    memset(buf1, 0, bufsize);
    if(read(conffd, buf1, bufsize) > 0){
        sscanf(buf1, "USERNAME=%s\nDBNAME=%s", username, dbname);
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
        printf("\nEnter 1 to import a file into database\n");
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
    
            case '1':   if (get_input("Enter absolute file path\n", buf1, bufsize) == -1) {
                            break;
                        }
                        import_file(buf1);
                        break;
        
            case '2':   if (get_input("Enter file name\n", buf1, bufsize) == -1) {
                            break;
                        }   
                        if (get_input("Enter destination\n", buf2   , bufsize) == -1) {
                            break;
                        }
                        send_file (buf1, buf2);
                        break;
            
            case '3':   if (get_input("Enter absolute file path\n", buf1, bufsize) == -1) {
                            break;
                        }
                        export_file(buf1);
                        break;
            
            default:    printf("Invalid Input\n");        
                        break;
        };

    } while(1);

}