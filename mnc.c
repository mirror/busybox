/*  mnc: mini-netcat - built from the ground up for LRP
    Copyright (C) 1998  Charles P. Wright

    0.0.1   6K      It works.
    0.0.2   5K      Smaller and you can also check the exit condition if you wish.


    19980918 Busy Boxed! Dave Cinege

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/ioctl.h>

const char mnc_usage[] = 
"mini-netcat 0.0.1 -- Open pipe to IP:port\n"
"\tmnc [IP] [port]\n";

int
mnc_main(struct FileInfo * i, int argc, char **argv)
{
 
        int sfd;
        int result;
        int len;
        int pid;
        char ch;
        
        struct sockaddr_in address;
        struct hostent *hostinfo;

#ifdef SELECT
        fd_set readfds, testfds;
#endif

        sfd = socket(AF_INET, SOCK_STREAM, 0);

        hostinfo = (struct hostent *) gethostbyname(argv[1]);

        if (!hostinfo)
        {
                exit(1);
        }

        address.sin_family = AF_INET;
        address.sin_addr = *(struct in_addr *) *hostinfo->h_addr_list;
        address.sin_port = htons(atoi(argv[2]));

        len = sizeof(address);

        result = connect(sfd, (struct sockaddr *)&address, len);

        if (result < 0) 
        {
                exit(2);
        }

#ifdef SELECT
        FD_ZERO(&readfds);
        FD_SET(sfd, &readfds);
        FD_SET(fileno(stdin), &readfds);

        while(1)
        {
                int fd;
                int nread;

                testfds = readfds;

                result = select(FD_SETSIZE, &testfds, (fd_set *) NULL, (fd_set *) NULL, (struct timeval *) 0);

                if(result < 1) 
                {
                        exit(3);
                }

                for(fd = 0; fd < FD_SETSIZE; fd++)
                {
                        if(FD_ISSET(fd,&testfds))
                        {
                                ioctl(fd, FIONREAD, &nread);

                                if (nread == 0)
                                        exit(0);

                                if(fd == sfd)
                                {
                                        read(sfd, &ch, 1);
                                        write(fileno(stdout), &ch, 1);
                                }
                                else
                                {
                                        read(fileno(stdin), &ch, 1);
                                        write(sfd, &ch, 1);
                                }
                        }
                }
        }
#else
        pid = fork();

        if (!pid)
        {
                int retval;
                retval = 1;
                while(retval == 1)
                {
                        retval = read(fileno(stdin), &ch, 1);
                        write(sfd, &ch, 1);
                }
        }
        else
        {
                int retval;
                retval = 1;
                while(retval == 1)
                {
                        retval = read(sfd, &ch, 1);
                        write(fileno(stdout), &ch, 1);
                }
        }

        exit(0);
#endif
}
