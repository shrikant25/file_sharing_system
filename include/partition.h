#ifndef _PARTN_H
#define _PARTN_H

#define TOTAL_PARTITIONS 80
#define PARTITION_SIZE 131072  
#define DATA_BLOCK_SIZE 10485760 + 3
#define COMM_BLOCK_SIZE 10485760 + 6

// 1024 * 128 * 10 + 3
// 3 bytes extra for bitmap   __
//                           ('')
//                         _/|__|\_
//                           |  |                        


#ifndef GT_PTNS
#define GT_PTNS
int get_partition(char *, int, int);
#endif

#endif