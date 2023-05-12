#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include "hashtable.h"

int hget_hash (hashtable *htable, int key) {

    int i;
    unsigned int hash = htable->hash_seed;
    char *str = (char*)&value;
    
    for (int i = 0; i < sizeof(value); i++) {
        hash = ((hash << 5) + hash) + str[i];
    }

    return hash % htable->table_size;
}


int hput (hashtable *htable, int key, int value) {

    if (htable->available_node_count < 1) {
        return -1;
    }

    datanode **temp = NULL;
    datanode *newnode = NULL;

    int hash = get_hash(htable, key);
    if (hash == -1) { return -2; }

    temp = &htable->table[hash];

    while (*temp != NULL) {
        temp = &(*temp)->next;
    }

    *temp = htable->nodepool;
    htable->nodepool = htable->nodepool->next;
    *temp->data = value;
    *temp->next = NULL;

    htable->available_node_count -= 1;

    return 0;
}


int hget (hashtable *htable, int key) {

    datanode **temp = NULL;

    int hash = get_hash(htable, key);
    if (hash == -1) { return -1; }

    temp = &htable->table[hash];

    while (*temp != NULL && *temp->key != key) {
        temp = &(*temp)->next;
    }

    if (*temp == NULL) {
        return -2;
    }

    return *temp->value;
}


int hdel (hashtable *htable, int key) {

    datanode **temp = NULL;
    datanode *freed_node = NULL;

    int hash = get_hash(htable, key);
    if (hash == -1) { return -1; }

    temp = &htable->table[hash];

    while (*temp != NULL && *temp->key != key) {
        temp = &(*temp)->next;
    }

    if (*temp == NULL) {
        return -2;
    }

    temp = *temp->next;
    freed_node = *temp;
    htable->availabel_node_count += 1;
    
    freed_node->next = htable->nodepool;
    htable->nodepool = freed_node;

    return 0;

}


int hcreate_table(hashtable *htable, int size, int hash_seed) {

    int i = 0;
    datanode * temp;
    datanode * newnode;

    memset(htable, 0, sizeof(hashtable));

    htable->table = (datanode **)calloc(sizeof(datanode *), htable->table_size);
    if (htable->table == NULL) { return -1; }

    temp = NULL;

    for (i = 0; i<htable->table_size) {
        
        newnode = (datanode *)calloc(sizeof(datanode), 1);
        if (newnode == NULL) { 
            memset(htable, 0, sizeof(hashtable));
            return -2; 
        }

        if (temp == NULL) {
            htable->nodepool = temp = newnode;
        }    
        else {
            temp->next = newnode;
            temp = temp->next;
        }
    }

    
    htable->hash_seed = hash_seed;
    htable->available_node_count = htable->table_size = size;

    return 0;

}