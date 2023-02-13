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
        if(start_pos >= TOTAL_PARTITIONS)
            start_pos = 0;
        if(state == ((bmap>>start_pos) & 1) )
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
        memcpy(&bmap, COMM_BLOCK_SIZE/2, 3);
        begin = 40;
        end = TOTAL_PARTITONS;
    }

    for(i = begin; i<end; i++) {

        start_pos++;
        if(start_pos >= end)
            start_pos = begin;
        if(state == ((bmap>>start_pos) & 1) )
            return start_pos;
        
    }
       
    return -1;
}
