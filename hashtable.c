#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include "hashtable.h"

int get_hash (hashtable *htable, int key) {

    int i;
    unsigned int hash = htable->hash_seed;
    char *str = (char*)&value;
    
    for (int i = 0; i < sizeof(value); i++) {
        hash = ((hash << 5) + hash) + str[i];
    }

    return hash % htable->table_size;
}


int put (hashtable *htable, int key, int value) {

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

    htable->curnode += 1;
    htable->available_node_count -= 1;

    return 0;
}


int get (hashtable *htable, int key) {

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


int del (hashtable *htable, int key) {

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
    htable->nodepool = freed_node->next;

    return 0;

}


int create_table(hashtable *htable, int size, int hash_seed) {

    int i = 0;
    datanode * temp;
    datanode * newnode;

    memset(htable, 0, sizeof(hashtable));

    htable->table = (datanode **)calloc(sizeof(datanode *), htable->table_size);
    if (htable->table == NULL) { return -1; }

    temp = htable->table;

    for (i = 0; i<htable->table_size) {
        
        newnode = (datanode *)calloc(sizeof(datanode), 1);
        if (newnode == NULL) { 
            htable->table = NULL;
            return -2; 
        }

        if (temp == NULL) {
            temp = newnode;
        }    
        else {
            temp->next = newnode;
            temp = temp->next;
        }
    }

    htable->hash_seed = hash_seed;
    htable->curnode = 0;
    htable->table_size = size;
    htable->available_node_count = size;

    return 0;

}