#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shared_memory.h"
#include "partition.h"
#include <unistd.h>
#include <libpq-fe.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "processor.h"

int process_status = 1

int run_process(char *block) {

    sem_unlink(SEM_LOCK);

	sem_t *sem = sem_open(SEM_LOCK, O_CREAT, 0777, 1);
	if (sem ==  SEM_FAILED) {
        printf("unable to create a semaphore");
        return -1;
    }

    while (process_status) {
        sem_wait(sem);         

        sem_post(sem);
    }

    sem_close(sem);
}


int open_sem_locks() {

    sem_unlink(SEM_LOCK_DATAR);
    sem_unlink(SEM_LOCK_COMMR);
    sem_unlink(SEM_LOCK_DATAS);
    sem_unlink(SEM_LOCK_COMMS);

	sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
	if (sem_lock_datar ==  SEM_FAILED) {
        printf("unable to create a semaphore");
        return -1;
    }

	sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);
	if (sem_lock_commr ==  SEM_FAILED) {
        printf("unable to create a semaphore");
        return -1;
    }

	sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
	if (sem_lock_datas ==  SEM_FAILED) {
        printf("unable to create a semaphore");
        return -1;
    }


	sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);
	if (sem_lock_comms ==  SEM_FAILED) {
        printf("unable to create a semaphore");
        return -1;
    }

    return 0;
}

int close_sem_locks() {

    if (sem_close(sem_lock_datar) == -1) {
        perror("error closing clock");
        return -1;
    }

    if (sem_close(sem_lock_commr) == -1) {
        perror("error closing clock");
        return -1;
    }

    if (sem_close(sem_lock_datas) == -1) {
        perror("error closing clock");
        return -1;
    }
    
    if (sem_close(sem_lock_comms) == -1) {
        perror("error closing clock");
        return -1;
    }

}



int main(void) {

    char *block = attach_memory_block(FILENAME, BLOCK_SIZE,);
    if (block == NULL) {
        printf("unable to create block");
        return -1;
    }

    open_sem_lock();
    run_process(block);    
    close_sem_lock();   
    detach_memory_block(block);
        
    return 0;
}