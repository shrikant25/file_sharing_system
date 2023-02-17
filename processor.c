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
#include "bitmap.h"
#include "processor.h"

int process_status = 1;

datablocks dblks;
semlocks slks;

// read any incoming data from p2
    
int get_data_from_receiver(int *drstart_pos) {

    int subblock_position = -1;
    char *blkptr = NULL;
    char data[PARTITION_SIZE];

    sem_wait(slks.sem_lock_datar);         
    subblock_position = get_subblock(dblks.datar_block , 1, drstart_pos);
    
    if(subblock_position >= 0) {

        drstart_pos = subblock_position;
        blkptr = dblks.datar_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);

        memcpy(data, blkptr, PARTITION_SIZE); 
        store_data_in_database(data);

        memset(data, 0, PARTITION_SIZE);
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datar_block, 1);
    
    }

    sem_post(slks.sem_lock_datar);

}


int give_data_to_sender(int *dsstart_pos) {

    int subblock_position = -1;
    char *blkptr = NULL;
    char data[PARTITION_SIZE];

    sem_wait(slks.sem_lock_datas);         
    subblock_position = get_subblock(dblks.datas_block , 0, dsstart_pos);
    
    if(subblock_position >= 0) {

        dsstart_pos = subblock_position;
        blkptr = dblks.datas_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);
        
        retrive_data_from_database(data);
        memcpy(blkptr, data, PARTITION_SIZE);
        
        memset(data, 0, PARTITION_SIZE);  
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.datas_block, 1);
    
    }

    sem_post(slks.sem_lock_datas);

}


int communicate_with_receiver(int *crstart_pos1, int *crstart_pos2) {

    int subblock_position = -1;
    char *blkptr = NULL;
    char data[PARTITION_SIZE];

    subblock_position = get_subblock2(dblks.commr_block, 0, crstart_pos1, 0);
    
    if(subblock_position >= 0) {

        crstart_pos1 = subblock_position;
        blkptr = dblks.commr_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);

        retrive_comm_from_database(data);
        memcpy(blkptr, data, PARTITION_SIZE);

        memset(data, 0, PARTITION_SIZE);  
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.commr_block, 2);
    
    }

    subblock_position = -1;
    subblock_position = get_subblock2(dblks.commr_block, 1, crstart_pos2, 1);
    
    if(subblock_position >= 0) {

        crstart_pos2 = subblock_position;
        blkptr = dblks.commr_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);

        memcpy(data, blkptr, PARTITION_SIZE);
        store_comm_from_database(data);
        memset(data, 0, PARTITION_SIZE);  
        
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.commr_block, 3);
    
    }

    sem_post(slks.sem_lock_commr);
    
}


int communicate_with_sender(int *csstart_pos1, int *csstart_pos2) {

    int subblock_position = -1;
    char *blkptr = NULL;
    char data[PARTITION_SIZE];

    sem_wait(slks.sem_lock_comms);         
    subblock_position = get_subblock2(dblks.comms_block, 0, csstart_pos1, 0);
    
    if(subblock_position >= 0) {

        csstart_pos1 = subblock_position;
        blkptr = dblks.comms_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);

        read_from_database(data);
        memcpy(blkptr, data, PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);  
        
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.comms_block, 1);
    
    }

    subblock_position = -1;
    subblock_position = get_subblock2(dblks.comms_block, 1, csstart_pos2, 1);
    
    if(subblock_position >= 0) {

        csstart_pos2 = subblock_position;
        blkptr = dblks.comms_block +(subblock_position*PARTITION_SIZE);
        memset(data, 0, PARTITION_SIZE);

        memcpy(data, blkptr, PARTITION_SIZE);
        write_to_database(data);
        memset(data, 0, PARTITION_SIZE);  
        
        blkptr = NULL;
        toggle_bit(subblock_position, dblks.comms_block, 2);
    
    }

    sem_post(slks.sem_lock_commr);
    
}


int run_process() {

    int drstart_pos = -1; //to keep track of last position
    int dsstart_pos = -1;
    int csstart_pos1 = -1; //to keep track of position from first partition
    int csstart_pos2 = -1; //to keep track of postion from second partition
    int crstart_pos1 = -1;
    int crstart_pos2 = -1;
    
    unset_all_bits(dblks.commr_block);
    unset_all_bits(dblks.comms_block);
    unset_all_bits(dblks.datas_block);
    unset_all_bits(dblks.datar_block);

    while (process_status) {
    
        sleep(1);
        
        communicate_with_receiver(&crstart_pos1, &crstart_pos2);
        communicate_with_sender(&csstart_pos1, &csstart_pos2);
        get_data_from_receiver(&drstart_pos);
        give_data_to_sender(&dsstart_pos);
        
    }
   
}


int open_sem_locks() {

    int status = 0;
    status = sem_unlink(SEM_LOCK_DATAR);
    status = sem_unlink(SEM_LOCK_COMMR);
    status = sem_unlink(SEM_LOCK_DATAS);
    status = sem_unlink(SEM_LOCK_COMMS);
    
    slks.sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    slks.sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);
    slks.sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    slks.sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (slks.sem_lock_datar == SEM_FAILED || slks.sem_lock_commr == SEM_FAILED || slks.sem_lock_datas == SEM_FAILED || slks.sem_lock_comms = SEM_FAILED)
        status = -1;

    return status;
}


int close_sem_locks() {

    int status = 0;
    status = sem_close(slks.sem_lock_datar);
    status = sem_close(slks.sem_lock_commr);
    status = sem_close(slks.sem_lock_datas);
    status = sem_close(slks.sem_lock_comms);

    return status; // if any of above call fails, then return -1
}


int get_shared_memory() {

    dblks.datar_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAR);
    dblks.commr_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMR);
    dblks.datas_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAS);
    dblks.comms_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMS);

    if (!(dblks.datar_block && dblks.commr_block && dblks.datas_block && dblks.comms_block)) 
        return -1; 
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


int destroy_shared_memoery()
{
    int status = 0;
    status = destroy_memory_block(dblks.datar_block);
    status = destroy_memory_block(dblks.commr_block);
    status = destroy_memory_block(dblks.datas_block);
    status = destroy_memory_block(dblks.comms_block);
    
    return status; // if any of above call fails, then return -1 
}

int main(void) 
{
    
    get_shared_memory();
    open_sem_lock();
    connect_to_database();
    run_process(); 
    close_database_connection();    
    close_sem_lock();   
    detach_shared_memory();
    destroy_shared_memory();

    return 0;
}