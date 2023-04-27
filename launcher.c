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
    char *args[3];
};

int main(int argc, char *argv[]) {
	
    if (argc !=4) {
        syslog(LOG_NOTICE, "invalid arguments");
        return -1;
    }
    
    struct launch_processes lps[TOTAL_PROCESSES] = {
        {
            .path = "./processor_r",
            .args[0] = "processor_r",
            .args[1] = argv[1],
            .args[2] = NULL,
        },
        {
            .path = "./processor_s",
            .args[0] = "processor_s",
            .args[1] = argv[2],
            .args[2] = NULL,
        },
        {
            .path = "./receiver",
            .args[0] = "receiver",
            .args[1] = argv[1],
            .args[2] = NULL,
        },
        {
            .path = "./sender",
            .args[0] = "sender",
            .args[1] = argv[2],
            .args[2] = NULL,
        },
        {
            .path = "./sender_notif",
            .args[0] = "sender_notif",
            .args[1] = argv[3],
            .args[2] = NULL,
        }
    };
    
    pid_t pid = -1;
    int i = 0;

    for (i = 0; i<TOTAL_PROCESSES; i++) {
       
        pid = fork();
        if (pid < 0) {
            syslog(LOG_NOTICE, "failed to fork %s", strerror(errno));
        }
        else if (pid == 0){

            if ((execv(lps[i].path, lps[i].args)) == -1) {
                syslog(LOG_NOTICE, "failed execv %s with error  %s", lps[i].args[0], strerror(errno));
            }
            exit(0);
        }
    }

    return 0;
}
