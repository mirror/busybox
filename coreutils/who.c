/* vi: set sw=4 ts=4: */
/*----------------------------------------------------------------------
 * Mini who is used to display user name, login time, 
 * idle time and host name.
 *
 * Author: Da Chen  <dchen@ayrnetworks.com>
 *
 * This is a free document; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation:
 *    http://www.gnu.org/copyleft/gpl.html
 *
 * Copyright (c) 2002 AYR Networks, Inc. 
 *----------------------------------------------------------------------
 */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <utmp.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "busybox.h"

extern int who_main(int argc, char **argv)
{
    struct utmp *ut;
    struct stat st;
    int         devlen, len;
    time_t      now, idle; 

    if (argc > 1) 
        bb_show_usage();

    setutent();
    devlen = sizeof("/dev/") - 1;
    printf("USER       TTY      IDLE      FROM           HOST\n"); 

    while ((ut = getutent()) != NULL) {
        char name[40];

        if (ut->ut_user[0] && ut->ut_type == USER_PROCESS) { 
            len = strlen(ut->ut_line);
            if (ut->ut_line[0] == '/') { 
               strncpy(name, ut->ut_line, len);
               name[len] = '\0';
               strcpy(ut->ut_line, ut->ut_line + devlen);
            } else {
               strcpy(name, "/dev/");
               strncpy(name+devlen, ut->ut_line, len);
               name[devlen+len] = '\0';
            }
           
            printf("%-10s %-8s ", ut->ut_user, ut->ut_line);

            if (stat(name, &st) == 0) {
                now = time(NULL);        
                idle = now -  st.st_atime;
            
                if (idle < 60)
                    printf("00:00m    ");
                else if (idle < (60 * 60)) 
                    printf("00:%02dm    ", (int)(idle / 60));
                else if (idle < (24 * 60 * 60)) 
                    printf("%02d:%02dm    ", (int)(idle / (60 * 60)),
                           (int)(idle % (60 * 60)) / 60);
                else if (idle < (24 * 60 * 60 * 365)) 
                    printf("%03ddays   ", (int)(idle / (24 * 60 * 60)));
                else 
                    printf("%02dyears   ", (int) (idle / (24 * 60 * 60 * 365)));
            } else 
                printf("%-8s  ", "?");       
      
            printf("%-12.12s   %s\n", ctime(&(ut->ut_tv.tv_sec)) + 4, ut->ut_host);
        }
    }
    endutent();

    return 0;
}
