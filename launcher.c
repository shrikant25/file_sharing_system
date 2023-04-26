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
	
    if (argc !=2) {
        syslog(LOG_NOTICE, "invalid arguments");
        return -1;
    }
    
    struct launch_processes lps[TOTAL_PROCESSES];
    lps[0].path = "./processor_r";
    lps[0].args[0] = "processor_r";
    lps[0].args[1] = argv[1];
    lps[0].args[2] = NULL;

    lps[1].path = "./processor_s";
    lps[1].args[0] = "processor_s";
    lps[1].args[1] = argv[1];
    lps[1].args[2] = NULL;

    lps[2].path = "./receiver";
    lps[2].args[0] = "receiver";
    lps[2].args[1] = argv[1];
    lps[2].args[2] = NULL;

    lps[3].path = "./sender";
    lps[3].args[0] = "sender";
    lps[3].args[1] = argv[1];
    lps[3].args[2] = NULL;

    lps[4].path = "./sender_notif";
    lps[4].args[0] = "sender_notif";
    lps[4].args[1] = argv[1];
    lps[4].args[2] = NULL;

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
