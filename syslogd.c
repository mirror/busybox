/* userver.c - simple server for Unix domain sockets */

/* Waits for a connection on the ./sample-socket Unix domain
   socket. Once a connection has been established, copy data
   from the socket to stdout until the other end closes the
   connection, and then wait for another connection to the socket. */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define _PATH_LOG       "/dev/log"


/* issue an error message via perror() and terminate the program */
void die(char * message) {
    perror(message);
    exit(1);
}

/* Copies data from file descriptor 'from' to file descriptor 'to'
   until nothing is left to be copied. Exits if an error occurs.   */
void copyData(int from, int to) {
    char buf[1024];
    int amount;
    
    while ((amount = read(from, buf, sizeof(buf))) > 0) {
        if (write(to, buf, amount) != amount) {
            die("write");
            return;
        }
    }
    if (amount < 0)
        die("read");
}


int main(void) {
    struct sockaddr_un address;
    int fd, conn;
    size_t addrLength;

    /* Remove any preexisting socket (or other file) */
    unlink(_PATH_LOG);

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;	/* Unix domain socket */
    strncpy(address.sun_path, _PATH_LOG, sizeof(address.sun_path));

    if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
        die("socket");


    /* The total length of the address includes the sun_family 
       element */
    addrLength = sizeof(address.sun_family) + 
                 strlen(address.sun_path);

    if (bind(fd, (struct sockaddr *) &address, addrLength))
        die("bind");

    if (chmod(_PATH_LOG, 0666) < 0)
        die("chmod");

    if (listen(fd, 5))
        die("listen");


    while ((conn = accept(fd, (struct sockaddr *) &address, 
                          &addrLength)) >= 0) {
        printf("---- getting data\n");
        copyData(conn, fileno(stdout));
        printf("---- done\n");
        close(conn);
    }

    if (conn < 0) 
        die("accept");
    
    close(fd);
    return 0;
}



