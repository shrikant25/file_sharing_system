#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

int main(int argc, char *argv[]) 
{
    int num = atoi(argv[1]);
    int pid, i;
    char val[40];
    int result;
    const char *path = "/home/shrikant/Desktop/prj/temp/client";
    char *params[] = {"client"};
    time_t t;
    srand((unsigned)time(&t));

    for (i = 0; i<num; i++) {
        
        pid = fork();

        result = 0;

        if (pid < 0) {
            perror("failed to fork");
        }
        else if (pid == 0) {
            
            result = execv(path, params);
            if (result == -1) {
                perror("failed to exec");
            }
            exit(0);
        }
    }
    
    // parent waits for all child processes to terminate
    for (i = 0; i < num; i++) {
        wait(NULL);
    }
    
    printf("parent dead");
    return 0;
}
