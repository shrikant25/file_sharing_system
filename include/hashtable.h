#ifndef H_HASH_TABLE
#define H_HASH_TABLE

typedef struct datanode {
    unsigned int key;
    unsigned int value;
    struct datanode *next;
} datanode;

typedef struct hashtable {
    datanode **table;
    datanode *nodepool;
    unsigned int available_node_count;
    unsigned int table_size;
    unsigned int hash_seed;
} hashtable;

int hdel (hashtable *, int)
int hcreate_table(hashtable *, int, int);
int hget (hashtable *, int);
int hput (hashtable *, int, int);
int hget_hash (hashtable *, int);

#endif // H_HASH_TABLE