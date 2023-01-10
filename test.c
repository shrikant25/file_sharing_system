
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>

int main(void){


 PGconn *conn = PQconnectdb("user=shrikant dbname=shrikant");

    if (PQstatus(conn) == CONNECTION_BAD) {

        fprintf(stderr, "Connection to database failed: %s\n",
            PQerrorMessage(conn));
        
    }

    int         lobj_fd;
    char        buf[1024];
    int         nbytes= 0;
    int tmp;
    int         fd;



    lobj_fd = lo_open(conn, 11, INV_READ);
    if (lobj_fd < 0)
        fprintf(stderr, "cannot open large object %u", 4001);

    fd = open("vd_1.mkv", O_CREAT | O_WRONLY | O_TRUNC, 0777);
    if (fd < 0){                  
        fprintf(stderr, "cannot open unix file\"%s\"",
                "vd_1.mkv");
    }

    int end = lo_lseek(conn,  fd, 0, SEEK_END);

    printf("madit %d\n", end);
    /*while ((nbytes = lo_read(conn, lobj_fd, buf, 1)) > 0){
        printf("1 %s\n", buf);
        tmp = write(fd, buf, nbytes);
        if (tmp < nbytes)
        {
            fprintf(stderr, "error while writing \"%s\"",
                    "vd_1.mkv");
        }
    }
    printf(" v %d\n", nbytes);
*/
    lo_close(conn, lobj_fd);
    close(fd);

    return 0;
}