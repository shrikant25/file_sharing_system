#ifndef _C_N_P
#define _C_N_P
#include <libpq-fe.h>

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;

int connect_to_database();
int prepare_statements();

#endif // _C_N_P