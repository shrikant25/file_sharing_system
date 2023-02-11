#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H


#define SEM_LOCK_DATAR "sem_lock_datar"
#define SEM_LOCK_COMMR "sem_lock_commr"
#define SEM_LOCK_DATAS "sem_lock_datas"
#define SEM_LOCK_COMMS "sem_lock_comms"

#define PROJECT_ID_DATAR 1001
#define PROJECT_ID_COMMR 1002
#define PROJECT_ID_DATAS 1003
#define PROJECT_ID_COMMS 1004

#define FILENAME "p1"

char * attach_memory_block(char *, int, unsigned char);
int detach_memory_block(char *);
int destroy_memory_block(char *);

#endif