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
char key[10];
    srand(time(0));
    for (i = 0; i < 15; i++) {
        sprintf(key, "val%d", i);
        hput(&htable, key, i+1);
        printf("key %s value %d\n", key , i+1 );
    }

    printf("key %s get %d\n", "val3", hget(&htable, "val3"));
    printf("key %s get %d\n", "val5", hget(&htable, "val5"));
    printf("key %s get %d\n", "val7", hget(&htable, "val7"));
    printf("key %s get %d\n", "val13", hget(&htable, "val13"));

    printf("key %s del %d\n", "val5",hdel(&htable, "val5"));
    printf("key %s del %d\n", "val6",hdel(&htable, "val6"));
    printf("key %s del %d\n", "val7",hdel(&htable, "val7"));
    printf("key %s del %d\n", "val13",hdel(&htable, "val13"));

    printf("key %s get %d\n","val3",hdel(&htable, "val3"));
    printf("key %s get %d\n","val5",hdel(&htable, "val5"));
    printf("key %s get %d\n","val6",hdel(&htable, "val6"));
    printf("key %s get %d\n","val15",hdel(&htable, "val15"));
    printf("key %s get %d\n","val13", hdel(&htable, "val13"));
    printf("key %s get %d\n","val7", hget(&htable, "val7"));

    printf("key %s put %d\n","val7", hput(&htable, "val7", 7+1));
    printf("key %s put %d\n","val13", hput(&htable, "val13", 13+1));
    printf("key %s put %d\n","val5", hput(&htable, "val5", 5+1));


    printf("key %s get %d\n","val7", hget(&htable, "val7"));
    printf("key %s get %d\n","val13", hget(&htable, "val13"));
    printf("key %s get %d\n","val5", hget(&htable, "val5"));
    
    return 0;
} 