#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

int main(void) {

    int fd = open("./keys.conf", O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    char strings[7][30];
    int keys[4];
    char buf[500];
    if (read(fd, buf, sizeof(buf)) > 0) {
       
        sscanf(buf, "SEM_LOCK_DATAR=%s\nSEM_LOCK_COMMR=%s\nSEM_LOCK_SIG_R=%s\nSEM_LOCK_COMMS=%s\n\
                    SEM_LOCK_SIG_S=%s\nSEM_LOCK_SIG_PS=%s\n\
                    PROJECT_ID_DATAR=%d\nPROJECT_ID_COMMR=%d\nPROJECT_ID_DATAS=%d\nPROJECT_ID_COMMS=%d",\
                    &strings[0], &strings[1], &strings[2], &strings[3], &strings[4], &strings[5], &strings[6],\
                    &keys[0], &keys[1], &keys[2], &keys[3]);
    }
    
    
    close(fd);
    return 0;
}
/*

int pid = fork();
    if (pid < 0) {
        syslog(LOG_NOTICE, "failed to fork %s", strerror(errno));
        return -1;
    }
    else if(pid == 0) {

        const char *path = "/home/shrikant/Desktop/prj/sender_notif";
        char *params[] = {"sender_notif"};
        int result;
        result = execv(path, params);
        if (result == -1) {
            syslog(LOG_NOTICE, "failed execv sender_notif  %s", strerror(errno));
        }
        exit(0);
    }
    else {*/