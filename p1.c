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

PGconn *connect_to_database() {
   
   
    PGconn *connection;
    connection = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(connection) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(connection));
        do_exit(connection);
    }

	return connection

}



int run_process() {

    int subblock_positon = -1;
    int drstart_pos = -1; //to keep track of last position
    int dsstart_pos = -1;
    int csstart_pos1 = -1; //to keep track of position from first partition
    int csstart_pos2 = -1; //to keep track of postion from second partition
    int crstart_pos1 = -1;
    int crstart_pos2 = -1;
    char *blkptr = NULL;
    char data[PARTITION_SIZE];
   
    while (process_status) {
        
        sleep(1);

        // read any incoming data from p2
        sem_wait(sem_lock_datar);         
        subblock_position = get_subblock(datar_block , 1, drstart_pos);
        
		if(subblock_position >= 0) {

            drstart_pos = subblock_position;
            blkptr = datar_block +(subblock_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);

            memcpy(data, blkptr, PARTITION_SIZE); 
            store_in_database(data);
    
            memset(data, 0, PARTITION_SIZE);
            blkptr = NULL;
            toggle_bit(subblock_position, datar_block);
        
        }
       
        subblock_position = -1;
        sem_post(sem_lock_datar);

//--------------------------------------------------------------------------------
        // send data to p3
        sem_wait(sem_lock_datas);         
        subblock_position = get_subblock(datas_block , 0, dsstart_pos);
        
		if(subblock_position >= 0) {

            dsstart_pos = subblock_position;
            blkptr = datas_block +(subblock_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);
            
            read_from_database(data);
            memcpy(blkptr, data, PARTITION_SIZE);
            
            memset(data, 0, PARTITION_SIZE);  
            blkptr = NULL;
            toggle_bit(subblock_position, datas_block);
        
        }

        sem_post(sem_lock_datas);
        subblock_position = -1;

//---------------------------------------------------------------------------------
        //send and receive messages from p2
        sem_wait(sem_lock_commr);         
        subblock_position = get_subblock2(commr_block, 0, crstart_pos1, 0);
        
		if(subblock_position >= 0) {

            crstart_pos1 = subblock_position;
            blkptr = commr_block +(partition_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);

            read_from_database(data);
            memcpy(blkptr, data, PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);  
            
            blkptr = NULL;
            toggle_bit(subblock_position, commr_block);
        
        }

        subblock_position = -1;
        subblock_position = get_subblock2(commr_block, 1, crstart_pos2, 1);
        
		if(subblock_position >= 0) {

            crstart_pos2 = subblock_position;
            blkptr = commr_block +(partition_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);

            memcpy(data, blkptr, PARTITION_SIZE);
            write_to_database(data);
            memset(data, 0, PARTITION_SIZE);  
            
            blkptr = NULL;
            toggle_bit(subblock_position, commr_block);
        
        }

        sem_post(sem_lock_commr);
        subblock_position = -1;

//---------------------------------------------------------

        sem_wait(sem_lock_comms);         
        subblock_position = get_subblock2(comms_block, 0, csstart_pos1, 0);
        
		if(subblock_position >= 0) {

            csstart_pos1 = subblock_position;
            blkptr = comms_block +(partition_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);

            read_from_database(data);
            memcpy(blkptr, data, PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);  
            
            blkptr = NULL;
            toggle_bit(subblock_position, comms_block);
        
        }

        subblock_position = -1;
        subblock_position = get_subblock2(comms_block, 1, csstart_pos2, 1);
        
		if(subblock_position >= 0) {

            crstart_pos2 = subblock_position;
            blkptr = comms_block +(partition_position*PARTITION_SIZE);
            memset(data, 0, PARTITION_SIZE);

            memcpy(data, blkptr, PARTITION_SIZE);
            write_to_database(data);
            memset(data, 0, PARTITION_SIZE);  
            
            blkptr = NULL;
            toggle_bit(subblock_position, comms_block);
        
        }

        sem_post(sem_lock_commr);
        subblock_position = -1;
    
    }
   
}


int open_sem_locks() {

    int status = 0;
    status = sem_unlink(SEM_LOCK_DATAR);
    status = sem_unlink(SEM_LOCK_COMMR);
    status = sem_unlink(SEM_LOCK_DATAS);
    status = sem_unlink(SEM_LOCK_COMMS);
    
    sem_lock_datar = sem_open(SEM_LOCK_DATAR, O_CREAT, 0777, 1);
    sem_lock_commr = sem_open(SEM_LOCK_COMMR, O_CREAT, 0777, 1);
    sem_lock_datas = sem_open(SEM_LOCK_DATAS, O_CREAT, 0777, 1);
    sem_lock_comms = sem_open(SEM_LOCK_COMMS, O_CREAT, 0777, 1);

    if (sem_lock_datar == SEM_FAILED || sem_lock_commr == SEM_FAILED || sem_lock_datas == SEM_FAILED || sem_lock_comms = SEM_FAILED)
        status = -1;

    return status;
}


int close_sem_locks() {

    int status = 0;
    status = sem_close(sem_lock_datar);
    status = sem_close(sem_lock_commr);
    status = sem_close(sem_lock_datas);
    status = sem_close(sem_lock_comms);

    return status; // if any of above call fails, then return -1
}


int get_shared_memory() {

    datar_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAR);
    commr_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMR);
    datas_block = attach_memory_block(FILENAME, DATA_BLOCK_SIZE, PROJECT_ID_DATAS);
    comms_block = attach_memory_block(FILENAME, COMM_BLOCK_SIZE, PROJECT_ID_COMMS);

    if (!(datar_block && commr_block && datas_block && comms_block)) 
        return -1; 
    return 0;
}


int detach_shared_memory() {

    int status = 0;
    status = detach_memory_block(datar_block);
    status = detach_memory_block(commr_block);
    status = detach_memory_block(datas_block);
    status = detach_memory_block(comms_block);
    
    return status; // if any of above call fails, then return -1 
}


int main(void) {

    PGconn *connection = NULL;
    
    get_shared_memory();
    open_sem_lock();
    connection = connect_to_database();
    run_process(connection);    
    PQfinish(connection);
    close_sem_lock();   
    detach_shared_memory();


    return 0;
}