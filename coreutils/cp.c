#include "internal.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <errno.h>

const char  cp_usage[] = "cp [-r] source-file destination-file\n"
"\t\tcp [-r] source-file [source-file ...] destination-directory\n"
"\n"
"\tCopy the source files to the destination.\n"
"\n"
"\t-r:\tRecursively copy all files and directories\n"
"\t\tunder the argument directory.";

extern int
cp_fn(const struct FileInfo * i)
{
    int         sourceFd;
    int         destinationFd;
    const char * destination = i->destination;
    struct stat destination_stat;
    int         status;
    char        buf[8192];
    char        d[PATH_MAX];

    if ( (i->stat.st_mode & S_IFMT) == S_IFDIR ) {
        if ( mkdir(destination, i->stat.st_mode & ~S_IFMT)
         != 0 && errno != EEXIST ) {
            name_and_error(destination);
            return 1;
        }
        return 0;
    }
    if ( (sourceFd = open(i->source, O_RDONLY)) < 0 ) {
        name_and_error(i->source);
        return 1;
    }
    if ( stat(destination, &destination_stat) == 0 ) {
        if ( i->stat.st_ino == destination_stat.st_ino
         &&  i->stat.st_dev == destination_stat.st_dev ) {
            fprintf(stderr
            ,"copy of %s to %s would copy file to itself.\n"
            ,i->source
            ,destination);
            close(sourceFd);
            return 1;
        }
    }
    /*
     * If the destination is a directory, create a file within it.
     */
    if ( (destination_stat.st_mode & S_IFMT) == S_IFDIR ) {
		destination = join_paths(
		 d
		,i->destination
		,&i->source[i->directoryLength]);

        if ( stat(destination, &destination_stat) == 0 ) {
            if ( i->stat.st_ino == destination_stat.st_ino
             &&  i->stat.st_dev == destination_stat.st_dev ) {
                fprintf(stderr
                ,"copy of %s to %s would copy file to itself.\n"
                ,i->source
                ,destination);
                close(sourceFd);
                return 1;
            }
        }
    }

    destinationFd = creat(destination, i->stat.st_mode & 07777);

    while ( (status = read(sourceFd, buf, sizeof(buf))) > 0 ) {
        if ( write(destinationFd, buf, status) != status ) {
            name_and_error(destination);
            close(sourceFd);
            close(destinationFd);
            return 1;
        }
    }
    close(sourceFd);
    close(destinationFd);
    if ( status < 0 ) {
        name_and_error(i->source);
        return 1;
    }
    return 0;
}
