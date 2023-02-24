#ifndef PARTN_H
#define PARTN_H

// todo - increase size of partitions to accomodate some meta info
#define TOTAL_PARTITIONS 80
#define DPARTITION_SIZE 131072 + 8
#define DATA_BLOCK_SIZE DPARTITION_SIZE * TOTAL_PARTITIONS + 3
#define CPARTITION_SIZE 1024
#define COMM_BLOCK_SIZE CPARTITION_SIZE * TOTAL_PARTITIONS + 4

// 1024 * 128 * 80 + 3
// 3 bytes extra for bitmap   __
//                           ('')
//                         _/|__|\_
//                           |  |                        
// 1024 * 64 * 40 + 2
// 1024 * 64 * 40 + 2 


int get_subblock(char *, int);
int get_subblock2(char *, int, int);
void toggle_bit(int, char *, int);
void set_all_bits(char *, int); 
void unset_all_bits(char *, int); 

#endif // PARTN_H
