#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libpq-fe.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include "shared_memory.h"
#include "partition.h"
#include "bitmap.h"
#include "processor.h"

int process_status = 1

int run_process() {

    int parition_positon = -1;
    int drstart_pos = -1; //to keep track of last position
    int dsstart_pos = -1;
    int csstart_pos = -1;
    int crstart_pos = -1;
    char *blkptr = NULL;
    char data[PARTITION_SIZE];
   
    while (process_status) {
        
        sleep(1);
        sem_wait(sem_lock_datar);         
        partition_position = get_partition(datar_block , 1, drstart_pos);
        
		if(partition_position >= 0) {

            drstart_pos = partition_position;
            blkptr = datar_block +(partition_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);

            memcpy(data, blkptr, PARTITION_SIZE); 
            store_in_database(data);
    
            memset(data, 0, PARTITION_SIZE);
            blkptr = NULL;
            toggle_bit(partition_position, datar_block);
        
        }
       
        partition_position = -1;
        sem_post(sem_lock_datar);


        sem_wait(sem_lock_datas);         
        partition_position = get_partition(datas_block , 0, dsstart_pos);
        
		if(partition_position >= 0) {

            dsstart_pos = partition_position;
            blkptr = datas_block +(partition_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);
            
            read_from_database(data);
            memcpy(blkptr, data, PARTITION_SIZE);
            
            memset(data, 0, PARTITION_SIZE);  
            blkptr = NULL;
            toggle_bit(filled_partition_position, datas_block);
        
        }

        sem_post(sem_lock_datas);
        filled_partition_position = -1;


        sem_wait(sem_lock_commr);         
        partition_position = get_partition(commr_block , 1, crstart_pos);
        
		if(partition_position >= 0) {

            crstart_pos = partition_position;
            blkptr = commr_block +(partition_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);
            
            memcpy(data, blkptr, PARTITION_SIZE);
            
            memset(data, 0, PARTITION_SIZE);  
            blkptr = NULL;
            toggle_bit(filled_partition_position, datas_block);
        
        }

        sem_post(sem_lock_commr);
        filled_partition_position = -1;
       
    
    }

    
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


int get_shared_memory() {

    datar_block = attach_memory_block(FILENAME, BLOCK_SIZE, PROJECT_ID_DATAR);
    if (datar_block == NULL) {
        printf("unable to create datar_block");
        return -1;
    };

    commr_block = attach_memory_block(FILENAME, BLOCK_SIZE, PROJECT_ID_COMMR);
    if (commr_block == NULL) {
        printf("unable to create commr_block");
        return -1;
    };

    datas_block = attach_memory_block(FILENAME, BLOCK_SIZE, PROJECT_ID_DATAS);
    if (datas_block == NULL) {
        printf("unable to create datas_block");
        return -1;
    };

    comms_block = attach_memory_block(FILENAME, BLOCK_SIZE, PROJECT_ID_COMMS);
    if (datar_block == NULL) {
        printf("unable to create comms_block");
        return -1;
    };

    return 0;

}


int detach_shared_memory() {

    detach_memory_block(datar_block);
    if (datar_block == NULL) {
        printf("unable to create datar_block");
        return -1;
    };

    detach_memory_block(commr_block);
    if (commr_block == NULL) {
        printf("unable to create commr_block");
        return -1;
    };

    detach_memory_block(datas_block);
    if (datas_block == NULL) {
        printf("unable to create datas_block");
        return -1;
    };

    detach_memory_block(comms_block);
    if (comms_block == NULL) {
        printf("unable to create comms_block");
        return -1;
    };

    return 0;
}


int main(void) {

    get_shared_memory();
    open_sem_lock();
    run_process();    
    close_sem_lock();   
    detach_shared_memory();

    return 0;
}