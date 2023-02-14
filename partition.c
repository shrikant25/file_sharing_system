#include <string.h>
#include "partition.h"
#include <stdio.h>

// state = 0 means empty position, state = 1 means filled position
int get_subblock(char *block, int state, int start_pos) {

    int i;
    unsigned int bmap = 0;
    memcpy(&bmap, block, 3); // retrive bitmap
    
    for(i = 0; i<TOTAL_PARTITIONS; i++) {

        start_pos++;
        if (start_pos >= TOTAL_PARTITIONS)
            start_pos = 0;
        if (state == ((bmap>>start_pos) & 1) )
            return start_pos;
        
    }
       
    return -1;
}


int get_subblock2(char *block, int state, int start_pos, int sub_block) {

    int i;
    int begin = 0;
    int end = 0;
    unsigned int bmap = 0;
 
    if (sub_block == 0){
        memcpy(&bmap, block, 3); // retrive bitmap
        begin = 0;
        end = 40;
    }
    else{
        memcpy(&bmap, block+COMM_BLOCK_SIZE/2, 3);
        begin = 40;
        end = TOTAL_PARTITIONS;
    }

    for (i = begin; i<end; i++) {

        start_pos++;
        if (start_pos >= end)
            start_pos = begin;
        if (state == ((bmap>>start_pos) & 1) )
            return start_pos;
        
    }
       
    return -1;
}

// 1 = bitmap present at start representing 80 blocks 
// 2 = bitmap present at start representing 40 blocks
// 3 = bitmap present at middle representing 40 blocks
void toggle_bit(int i, char *block, int block_type) {
    
    short int bmap = 0;

    if (block_type == 1) { 

        memcpy(&bmap, block, 3); // retrive bitmap    
        bmap ^= 1<<i;  // toggle bit
        memcpy(block, &bmap, 3);  // store bitmap

    }
    else if (block_type == 2) {

        memcpy(&bmap, block, 2); // retrive bitmap    
        bmap ^= 1<<i;  // toggle bit
        memcpy(block, &bmap, 2);  // store bitmap

    }
    else if (block_type == 3) {

        memcpy(&bmap, block+COMM_BLOCK_SIZE/2, 2); // retrive bitmap    
        bmap ^= 1<<i;  // toggle bit
        memcpy(block+COMM_BLOCK_SIZE/2, &bmap, 2);  // store bitmap

    }
}


void set_all_bits(char *block, int block_type) {

    if (block_type == 1) 
        memset(block, 0xFFFFFFFF, 3);
    else if (block_type == 2)
        memset(block, 0xFFFFFFFF, 2);
    else if (block_type == 3)
        memset(block, 0xFFFFFFFF, 2);
}


void unset_all_bits(char *block, int block_type) {

    if (block_type == 1) 
        memset(block, 0, 3);
    else if (block_type == 2)
        memset(block, 0, 2);
    else if (block_type == 3)
        memset(block, 0, 2);
    
}