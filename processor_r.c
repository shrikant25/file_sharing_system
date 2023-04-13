#include <syslog.h>
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
    
int get_data_from_receiver() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    newmsg_data rcond;

    sem_wait(smlks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block, 1, 3);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datar_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        memset(&rcond, 0, sizeof(rcond));
        memcpy(&rcond, blkptr, sizeof(rcond));

        store_data_in_database(&rcond);

        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 3);
    
    }

    sem_post(smlks.sem_lock_datar);
}


int communicate_with_receiver() 
{
    int subblock_position = -1;
    char *blkptr = NULL;
    char data[CPARTITION_SIZE];
    receivers_message rcvm;

    sem_wait(smlks.sem_lock_commr);         
    subblock_position = get_subblock(dblks.commr_block, 0, 1);
    
    if(subblock_position >= 0) {

        blkptr = dblks.commr_block  + (TOTAL_PARTITIONS/8) + subblock_position * CPARTITION_SIZE;
        
        memset(data, 0, CPARTITION_SIZE);
        if (retrive_commr_from_database(data) != -1) {

            memcpy(blkptr, data, CPARTITION_SIZE);
            memset(data, 0, CPARTITION_SIZE);  
            toggle_bit(subblock_position, dblks.commr_block, 1);

        }
        blkptr = NULL;
    }

 
    subblock_position = -1;
    subblock_position = get_subblock(dblks.commr_block, 1, 2);
    
    if (subblock_position >= 0) {

        blkptr = dblks.commr_block + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        
        memcpy(&rcvm, blkptr, sizeof(rcvm));
        store_commr_into_database(&rcvm);
          
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.commr_block, 2);
    
    }

    sem_post(smlks.sem_lock_commr);   
}



int run_process() 
{
   
    while (process_status) {
        communicate_with_receiver();
        get_data_from_receiver();
    }  
}


int main(void) 
{
    int status = 0;

    dblks.datar_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, (unsigned char)PROJECT_ID_DATAR);
    dblks.commr_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, (unsigned char)PROJECT_ID_COMMR);
 
    if (!(dblks.datar_block && dblks.commr_block)) {
        syslog(LOG_NOTICE,"failed to get shared memory");
        return -1; 
    }

    status = sem_unlink(SEM_LOCK_DATAR);
    status = sem_unlink(SEM_LOCK_COMMR);
    
    smlks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    smlks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datar == SEM_FAILED || smlks.sem_lock_commr == SEM_FAILED)
        status = -1;

    connect_to_database();
    prepare_statements();   
    
    unset_all_bits(dblks.commr_block, 2);
    unset_all_bits(dblks.commr_block, 3);
    unset_all_bits(dblks.datar_block, 1);
    
    run_process(); 
    
    close_database_connection();    

    sem_close(smlks.sem_lock_datar);
    sem_close(smlks.sem_lock_commr);
    
    detach_memory_block(dblks.datar_block);
    detach_memory_block(dblks.commr_block);
    
    destroy_memory_block(dblks.datar_block, PROJECT_ID_DATAR);
    destroy_memory_block(dblks.commr_block, PROJECT_ID_COMMR);
    
    return 0;
}   