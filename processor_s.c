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

// read any incoming data from p2
 

int give_data_to_sender() 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(smlks.sem_lock_datas);         
    subblock_position = get_subblock(dblks.datas_block, 0, 3);
    
    if (subblock_position >= 0) {

        blkptr = dblks.datas_block + (TOTAL_PARTITIONS/8) + subblock_position * DPARTITION_SIZE;
        
        if (retrive_data_from_database(blkptr) != -1) { 
            toggle_bit(subblock_position, dblks.datas_block, 3);
        }
        blkptr = NULL;
    }

    sem_post(smlks.sem_lock_datas);
}


int communicate_with_sender() 
{
    int subblock_position = -1;
    char *blkptr = NULL;

    sem_wait(smlks.sem_lock_comms);         
    subblock_position = get_subblock(dblks.comms_block, 0, 1);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        
        if (retrive_comms_from_database(blkptr) != -1){
            toggle_bit(subblock_position, dblks.comms_block, 1);
        }

        blkptr = NULL;
    }
    
    subblock_position = -1;
    subblock_position = get_subblock(dblks.comms_block, 1, 2);
    
    if (subblock_position >= 0) {

        blkptr = dblks.comms_block + (TOTAL_PARTITIONS/8) + subblock_position*CPARTITION_SIZE;
        store_comms_into_database(blkptr);
        toggle_bit(subblock_position, dblks.comms_block, 2);
    
    }

    sem_post(smlks.sem_lock_comms);   
}


int run_process() 
{
   
    while (process_status) {

        communicate_with_sender();
        give_data_to_sender();
    
    }  
}


int main(void) 
{
    int status = 0;

    dblks.datas_block = attach_memory_block(FILENAME_S, DATA_BLOCK_SIZE, (unsigned char)PROJECT_ID_DATAS);
    dblks.comms_block = attach_memory_block(FILENAME_S, COMM_BLOCK_SIZE, (unsigned char)PROJECT_ID_COMMS);

    if (!( dblks.datas_block && dblks.comms_block)) {
        syslog(LOG_NOTICE,"failed to get shared memory");
        return -1; 
    }

    status = sem_unlink(SEM_LOCK_DATAS);
    status = sem_unlink(SEM_LOCK_COMMS);
    
    smlks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    smlks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (smlks.sem_lock_datas == SEM_FAILED || smlks.sem_lock_comms == SEM_FAILED)
        status = -1;

    connect_to_database();
    prepare_statements();   
    
    unset_all_bits(dblks.comms_block, 2);
    unset_all_bits(dblks.comms_block, 3);
    unset_all_bits(dblks.datas_block, 1);
    
    run_process(); 
    
    close_database_connection();    

    sem_close(smlks.sem_lock_datas);
    sem_close(smlks.sem_lock_comms);

    detach_memory_block(dblks.datas_block);
    detach_memory_block(dblks.comms_block);

    destroy_memory_block(dblks.datas_block, PROJECT_ID_DATAS);
    destroy_memory_block(dblks.comms_block, PROJECT_ID_COMMS);

    return 0;
}   