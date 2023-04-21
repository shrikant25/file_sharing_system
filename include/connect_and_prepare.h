#ifndef _C_N_P
#define _C_N_P
#include <libpq-fe.h>

typedef struct db_statements {
    char statement_name[20];
    char *statement;
    int param_count;
}db_statements;

int connect_to_database(PGconn *);
int prepare_statements(PGconn *c, int, db_statements *);

#endif // _C_N_P