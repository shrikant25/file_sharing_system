
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <syslog.h>


static void skeleton_daemon(){
 
    pid_t pid;

    // make port of parent process
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    // kill parent
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // if >= 0 means child process has become session leader
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    //fork again
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    // again parent dies
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // set file premissions
    umask(0);

}

int main(){
    skeleton_daemon();

    while (1){
        //TODO: Insert daemon code here.
        syslog (LOG_NOTICE, "First daemon started.");

        

        sleep (20);
        break;
    }

    syslog (LOG_NOTICE, "First daemon terminated.");
    closelog();

    return EXIT_SUCCESS;
}

//gcc daemon.c -o daemon  -I/usr/include/postgresql -lpq -std=c99
