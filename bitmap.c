#include <string.h>
#include <stdio.h>
#include "partition.h"

void toggle_bit(int i, char *block){
    
    short int bmap = 0;
    memcpy(&bmap, block+(BLOCK_SIZE-3), 2); // retrive bitmap
   
    bmap ^= 1<<i;  // toggle bit
   
    memcpy(block+(BLOCK_SIZE-3), &bmap,2);  // store bitmap  

}

void set_all_bits(char *block){

    memset(block+(BLOCK_SIZE-3), 0x10, 2);

}

void unset_all_bits(char *block){

    memset(block+(BLOCK_SIZE-3), 0, 2);

}
