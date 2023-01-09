#include <string.h>
#include "partition.h"
#include <stdio.h>

// state = 0 means empty position, state = 1 means filled position
int get_partition(char *block, int state, int start_pos){

    int i;
    short int bmap = 0;
    memcpy(&bmap, block+(BLOCK_SIZE-3), 2); // retrive bitmap
     for(i = 0; i<TOTAL_PARTITIONS; i++){
        start_pos++;
        if(start_pos >= TOTAL_PARTITIONS)
            start_pos = 0;
        if(state == ((bmap>>start_pos) & 1) )
                return start_pos;
        
    }
       
    return -1;
}


/*
    in future if bitmap size stays below 64 just try to find the leftmost bit

*/