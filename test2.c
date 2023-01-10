#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libpq-fe.h>

int main(void){

    unsigned char value[2*1024*1024];
    PGconn *connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n",
            PQerrorMessage(connection));
        
    }
char mid[10];
    char msubid[10];
    int id  = 1;
    int substrid = 1;
    
	sprintf(mid, "%d", id);
	sprintf(msubid, "%d", substrid);
		
	PGresult* res = PQprepare(connection, "insert_stmt0", "select substring from substrings where msgid = $1 and substrid = $2", 2, NULL);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return 1;
    }

	const char* paramValues[] = {mid, msubid};
    printf("%s  %s\n",mid, msubid);
      res = PQexecPrepared(connection, "insert_stmt0", 2, paramValues, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("Insert failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return 1;
    }
     //PQgetlength
	strcpy(value, PQgetvalue(res, 0, 0));
	//int *v =  PQgetvalue(res, 0, 0);
    //printf("len %ld\n", PQgetlength(res, 0, 0));


    res = PQprepare(connection, "insert_stmt9", "insert into dummy values($2, $1)", 2, NULL);


    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return 1;
    }

	const char* paramValues1[] = {mid, value};
      res = PQexecPrepared(connection, "insert_stmt9", 2, paramValues1, NULL, NULL, 1);
      if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Preparation of statement failed: %s\n", PQerrorMessage(connection));
        PQclear(res);
        PQfinish(connection);
        return 1;
    }
	PQclear(res);
    PQfinish(connection);

    return 0;

}