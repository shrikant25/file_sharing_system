#include <string.h>
#include "partition.h"

// state = 0 means empty position, state = 1 means filled position
void get_partition(char *block, int state){

    int i;
    short int bmap = 0;
    memcpy(bmap, block+(BLOCK_SIZE-3), 2); // retrive bitmap
    
    for(i = 0; i<TOTAL_PARTITIONS; i++)
        if(state == ((bmap>>i) & 1) )
                return i;
        
    return -1;
}


/*
    in future if bitmap size stays below 64 just try to find the leftmost bit

*/