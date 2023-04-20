#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H


#define SEM_LOCK_DATAR "sem_lock_datar"
#define SEM_LOCK_COMMR "sem_lock_commr"
#define SEM_LOCK_DATAS "sem_lock_datas"
#define SEM_LOCK_COMMS "sem_lock_comms"
#define SEM_LOCK_SIG_R "sem_lock_sigr"
#define SEM_LOCK_SIG_D "sem_lock_sigd"

#define PROJECT_ID_DATAR 11
#define PROJECT_ID_COMMR 12
#define PROJECT_ID_DATAS 13
#define PROJECT_ID_COMMS 14
#define PROJECT_ID_SIG_R 15
#define PROJECT_ID_SIG_D 16
 
#define FILENAME_R "processor_r"
#define FILENAME_S "processor_s"

char *attach_memory_block(char *, int, unsigned char);
int detach_memory_block(char *);
int destroy_memory_block(char *,unsigned char);

#endif