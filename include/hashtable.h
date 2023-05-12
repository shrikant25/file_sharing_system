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

int get_hash(int);
int put(hashtable *, int);
int get(hashtable *, int);
int del(hashtable *, int);
int create_table(hashtable *, int, int);

#endif // H_HASH_TABLE