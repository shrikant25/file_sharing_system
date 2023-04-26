#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h> 
#include <errno.h>
#include <sys/wait.h>

#define TOTAL_PROCESSES 5

struct launch_processes
{
    const char *path;
    char *args[1];
};

struct launch_processes lps[TOTAL_PROCESSES] = {
    {
        .path = "./processor_r",
        .args = {"processor_r"},
    },
    {
        .path = "./receiver",
        .args = {"processor_r"},
    },
    {
        .path = "./processor_s",
        .args = {"processor_s"},
    },
    {
        .path = "./sender",
        .args = {"sender"},
    },
    {
        .path = "./sender_notif",
        .args = {"sender_notif"},
    },
};



int main(void) {

    pid_t pid = -1;
    int i = 0;

    for (i = 0; i<TOTAL_PROCESSES; i++) {
       
        pid = fork();
        if (pid < 0) {
            syslog(LOG_NOTICE, "failed to fork %s", strerror(errno));
        }
        else if (pid == 0){
            if ((execv(lps[i].path, lps[i].args)) == -1) {
                syslog(LOG_NOTICE, "failed execv %s with errro  %s", lps[i].args[0], strerror(errno));
            }
            exit(0);
        }
    }

    return 0;
}