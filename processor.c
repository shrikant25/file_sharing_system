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
#include "processor_db.h"
#include "shared_memory.h"
#include "partition.h"
#include "processor.h"

int process_status = 1;

datablocks dblks;
semlocks smlks;

// read any incoming data from p2
    
int get_data_from_receiver() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    rcondata rcond;

    sem_wait(smlks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block, 1);
    
    if(subblock_position >= 0) {

        blkptr = dblks.datar_block + 3 + subblock_position * DPARTITION_SIZE;
        
        memset(&rcond, 0, sizeof(rcond));
        memcpy(&rcond, blkptr, sizeof(rcond));

        store_data_in_database(&rcond);

        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 1);
    
    }

    sem_post(smlks.sem_lock_datar);
}

/*
int give_data_to_sender() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char data[DPARTITION_SIZE];

    sem_wait(smlks.sem_lock_datas);         
    subblock_position = get_subblock(dblks.datas_block , 0);
    
    if(subblock_position >= 0) {

        blkptr = dblks.datas_block + 3 + subblock_position * DPARTITION_SIZE;
        memset(data, 0, DPARTITION_SIZE);
        
        if (retrive_data_from_database(data) != -1) {
            memcpy(blkptr, data, DPARTITION_SIZE);
            memset(data, 0, DPARTITION_SIZE);  
            toggle_bit(subblock_position, dblks.datas_block, 1);
        }
        blkptr = NULL;
    }

    sem_post(smlks.sem_lock_datas);
}
*/

int communicate_with_receiver() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char data[CPARTITION_SIZE];
    rconmsg rcvm;

    sem_wait(smlks.sem_lock_commr);         
 /*   subblock_position = get_subblock2(dblks.commr_block, 0, 0);
    
    if(subblock_position >= 0) {

        blkptr = dblks.commr_block  + 2 + subblock_position * CPARTITION_SIZE;
        
        memset(data, 0, CPARTITION_SIZE);
        if (retrive_commr_from_database(data) != -1) {

            memcpy(blkptr, data, CPARTITION_SIZE);
            memset(data, 0, CPARTITION_SIZE);  
            toggle_bit(subblock_position, dblks.commr_block, 2);

        }
        blkptr = NULL;
    }

   */ 
    subblock_position = -1;
    subblock_position = get_subblock2(dblks.commr_block, 1, 1);
    
    if(subblock_position >= 0) {

        blkptr = dblks.commr_block + 4 + subblock_position*CPARTITION_SIZE;
        
        memcpy(&rcvm, blkptr, sizeof(rcvm));
        store_commr_into_database(&rcvm);
          
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.commr_block, 3);
    
    }

    sem_post(smlks.sem_lock_commr);   
}

/*
int communicate_with_sender() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char data[CPARTITION_SIZE];

    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock2(dblks.comms_block, 0, 0);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + 2 + subblock_position*CPARTITION_SIZE;
        
        memset(data, 0, CPARTITION_SIZE);
        if (retrive_comms_from_database(data) == -1){
            memcpy(blkptr, data, CPARTITION_SIZE);
            memset(data, 0, CPARTITION_SIZE);  
            toggle_bit(subblock_position, dblks.comms_block, 2);
        }

        blkptr = NULL;
    }
    
    subblock_position = -1;
    subblock_position = get_subblock2(dblks.comms_block, 1, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + 4 + subblock_position*CPARTITION_SIZE;
        
        memset(data, 0, CPARTITION_SIZE);
        memcpy(data, blkptr, CPARTITION_SIZE);
        store_comms_into_database(data);
        
        memset(data, 0, CPARTITION_SIZE);  
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.comms_block, 3);
    
    }

    sem_post(smlks.sem_lock_comms);   
}
*/

int run_process() 
{
    unset_all_bits(dblks.commr_block, 2);
    unset_all_bits(dblks.commr_block, 3);
    unset_all_bits(dblks.comms_block, 2);
    unset_all_bits(dblks.comms_block, 3);
    unset_all_bits(dblks.datas_block, 1);
    unset_all_bits(dblks.datar_block, 1);

    while (process_status) {

        communicate_with_receiver();
       // communicate_with_sender();
        get_data_from_receiver();
        //give_data_to_sender();
    
    }  
}


int open_sem_locks() 
{
    int status = 0;
    status = sem_unlink(SEM_LOCK_DATAR);
    status = sem_unlink(SEM_LOCK_COMMR);
    status = sem_unlink(SEM_LOCK_DATAS);
    status = sem_unlink(SEM_LOCK_COMMS);
    
    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);
    smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED || 
        smlks.sem_lock_datas == SEM_FAILED || smlks.sem_lock_comms == SEM_FAILED)
        status = -1;

    return status;
}


int close_sem_locks() 
{
    int status = 0;
    status = sem_close(smlks.sem_lock_datar);
    status = sem_close(smlks.sem_lock_commr);
    status = sem_close(smlks.sem_lock_datas);
    status = sem_close(smlks.sem_lock_comms);

    return status; // if any of above call fails, then return -1
}


int get_shared_memory() 
{
    dblks.datar_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, (unsigned char)PROJECT_ID_DATAR);
    dblks.commr_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, (unsigned char)PROJECT_ID_COMMR);
    dblks.datas_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, (unsigned char)PROJECT_ID_DATAS);
    dblks.comms_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, (unsigned char)PROJECT_ID_COMMS);

    if (!(dblks.datar_block && dblks.commr_block && dblks.datas_block && dblks.comms_block)) {
        printf("failed to get shared memory");
        return -1; 
    }
    return 0;
}


int detach_shared_memory() 
{
    int status = 0;
    status = detach_memory_block(dblks.datar_block);
    status = detach_memory_block(dblks.commr_block);
    status = detach_memory_block(dblks.datas_block);
    status = detach_memory_block(dblks.comms_block);
    
    return status; // if any of above call fails, then return -1 
}


int destroy_shared_memory()
{
    int status = 0;
    status = destroy_memory_block(dblks.datar_block, PROJECT_ID_DATAR);
    status = destroy_memory_block(dblks.commr_block, PROJECT_ID_COMMR);
    status = destroy_memory_block(dblks.datas_block, PROJECT_ID_DATAS);
    status = destroy_memory_block(dblks.comms_block, PROJECT_ID_COMMS);
    
    return status; // if any of above call fails, then return -1 
}


int main(void) 
{   
    get_shared_memory();
    open_sem_locks();
    connect_to_database();
    prepare_statements();   
    run_process(); 
    close_database_connection();    
    close_sem_locks();   
    detach_shared_memory();
    destroy_shared_memory();

    return 0;
}