#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "./include/hashtable.h"

int main (int argc, char *args[]) {
    
    if (argc != 2) {
        printf("%d argc", argc);
        return -1;
    }

    int i, op, status = 0;
    hashtable htable;

    status = hcreate_table(&htable, 10); 
    if (status == -2) {
        printf("failed at pp for table");
        return -1;
    }
    else if(status == -1) {
        printf("failed at pool for table");
        return -1;
    }
    else {
        printf("success creating htable\n");
    }

    printf("%d\n", argc);
    printf("%s\n", args[0]);

    srand(time(0));
    for (i = 0; i < 15; i++) {
        hput(&htable, i, i+1);
        printf("key %d value %d\n",i, i+1 );
    }

    printf("key %d get %d\n", 3, hget(&htable, 3));
    printf("key %d get %d\n", 5, hget(&htable, 5));
    printf("key %d get %d\n", 7, hget(&htable, 7));
    printf("key %d get %d\n", 13, hget(&htable, 13));

    printf("key %d del %d\n", 5,hdel(&htable, 5));
    printf("key %d del %d\n", 6,hdel(&htable, 6));
    printf("key %d del %d\n", 7,hdel(&htable, 7));
    printf("key %d del %d\n", 13,hdel(&htable, 13));

    printf("key %d get %d\n",3,hdel(&htable, 3));
    printf("key %d get %d\n",5,hdel(&htable, 5));
    printf("key %d get %d\n",6,hdel(&htable, 6));
    printf("key %d get %d\n",15,hdel(&htable, 15));
    printf("key %d get %d\n",13, hdel(&htable, 13));
    printf("key %d get %d\n",7, hget(&htable, 7));

    printf("key %d put %d\n",7, hput(&htable, 7, 7+1));
    printf("key %d put %d\n",13, hput(&htable, 13, 13+1));
    printf("key %d put %d\n",5, hput(&htable, 5, 5+1));


    printf("key %d get %d\n",7, hget(&htable, 7));
    printf("key %d get %d\n",13, hget(&htable, 13));
    printf("key %d get %d\n",5, hget(&htable, 5));
    
    return 0;
} 