#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include "hashtable.h"

int hget_hash (hashtable *htable, char *key) {

    int i;
    unsigned int hash = hash_seed;
    
    for (int i = 0; i < strlen(key); i++) {
        hash = ((hash << 5) + hash) + key[i];
    }

    return (hash % htable->table_size);
}


int hput (hashtable *htable, char *key, int value) {

    if (htable->available_node_count < 1) {
        return -1;
    }

    datanode **temp = NULL;
    datanode *newnode = NULL;

    int hash = hget_hash(htable, key);
    if (hash == -1) { return -2; }

    temp = &htable->table[hash];

    while (*temp != NULL) {
        temp = &(*temp)->next;
    }

    *temp = htable->nodepool;
    htable->nodepool = htable->nodepool->next;
    strncpy((*temp)->key, key, strlen(key));
    (*temp)->value = value;
    (*temp)->next = NULL;

    htable->available_node_count -= 1;

    return 0;
}


int hget (hashtable *htable, char *key) {

    datanode **temp = NULL;

    int hash = hget_hash(htable, key);
    if (hash == -1) { return -1; }

    temp = &htable->table[hash];

    while (*temp != NULL && (strcmp((*temp)->key, key))) {
        temp = &(*temp)->next;
    }

    if (*temp == NULL) {
        return -2;
    }

    return (*temp)->value;
}


int hdel (hashtable *htable, char *key) {

    datanode **temp = NULL;
    datanode *freed_node = NULL;

    int hash = hget_hash(htable, key);
    if (hash == -1) { return -1; }

    temp = &htable->table[hash];

    while (*temp != NULL && strcmp((*temp)->key, key)) {
        temp = &(*temp)->next;
    }

    if (*temp == NULL) {
        return -2;
    }

    freed_node = *temp;
    *temp = (*temp)->next;
    htable->available_node_count += 1;
    
    freed_node->next = htable->nodepool;
    htable->nodepool = freed_node;

    return 0;
}


int hcreate_table(hashtable *htable, int size) {

    int i = 0;
    datanode *temp;
    datanode *newnode;

    htable->available_node_count = htable->table_size = size;
    
    htable->table = (datanode **)calloc(sizeof(datanode *), htable->table_size);
    if (htable->table == NULL) { return -1; }

    temp = NULL;

    for (i = 0; i<htable->table_size; i++) {
        
        newnode = (datanode *)calloc(sizeof(datanode), 1);
        if (newnode == NULL) { 
            htable = NULL;
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
   

    return 0;

}