/*
 * Mini logger implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "internal.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>

#if !defined BB_SYSLOGD

#define SYSLOG_NAMES
#include <sys/syslog.h>

#else
/* We have to do this since the header file defines static
 * structues.  Argh.... bad libc, bad, bad...
 */
#include <sys/syslog.h>
typedef struct _code {
    char    *c_name;
    int     c_val;
} CODE;
extern CODE prioritynames[];
extern CODE facilitynames[];
#endif

static const char logger_usage[] =
    "logger [OPTION]... [MESSAGE]\n\n"
    "Write MESSAGE to the system log.  If MESSAGE is '-', log stdin.\n\n"
    "Options:\n"
    "\t-s\tLog to stderr as well as the system log.\n"
    "\t-p\tEnter the message with the specified priority.\n"
    "\t\tThis may be numerical or a ``facility.level'' pair.\n";


/* Decode a symbolic name to a numeric value 
 * this function is based on code
 * Copyright (c) 1983, 1993
 * The Regents of the University of California.  All rights reserved.
 */
static int 
decode(char* name, CODE* codetab)
{
    CODE *c;

    if (isdigit(*name))
	return (atoi(name));
    for (c = codetab; c->c_name; c++) {
	if (!strcasecmp(name, c->c_name)) {
		return (c->c_val);
	}
    }

    return (-1);
}

/* Decode a symbolic name to a numeric value 
 * this function is based on code
 * Copyright (c) 1983, 1993
 * The Regents of the University of California.  All rights reserved.
 */
static int 
pencode(char* s)
{
    char *save;
    int lev, fac=LOG_USER;

    for (save = s; *s && *s != '.'; ++s);
    if (*s) {
	*s = '\0';
	fac = decode(save, facilitynames);
	if (fac < 0) {
	    fprintf(stderr, "unknown facility name: %s\n", save);
	    exit( FALSE);
	}
	*s++ = '.';
    }
    else {
	s = save;
    }
    lev = decode(s, prioritynames);
    if (lev < 0) {
	fprintf(stderr, "unknown priority name: %s\n", save);
	exit( FALSE);
    }
    return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}


extern int logger_main(int argc, char **argv)
{
    struct sockaddr_un sunx;
    int fd, pri = LOG_USER|LOG_NOTICE;
    int fromStdinFlag=FALSE;
    int toStdErrFlag=FALSE;
    int stopLookingAtMeLikeThat=FALSE;
    char *message, buf[1024], buf1[1024];
    time_t  now;
    size_t addrLength;

    /* Parse any options */
    while (--argc > 0 && **(++argv) == '-') {
	if (*((*argv)+1) == '\0') {
	    fromStdinFlag=TRUE;
	}
	stopLookingAtMeLikeThat=FALSE;
	while (*(++(*argv)) && stopLookingAtMeLikeThat==FALSE) {
	    switch (**argv) {
	    case 's':
		toStdErrFlag = TRUE;
		break;
	    case 'p':
		if (--argc == 0) {
		    usage(logger_usage);
		}
		pri = pencode(*(++argv));
		stopLookingAtMeLikeThat=TRUE;
		break;
	    default:
		usage(logger_usage);
	    }
	}
    }

    if (fromStdinFlag==TRUE) {
	/* read from stdin */
	int i=0;
	char c;
	while ((c = getc(stdin)) != EOF && i<sizeof(buf1)) {
	    buf1[i++]=c;
	}
	message=buf1;
    } else {
	if (argc>=1) {
		message=*argv;
	} else {
	    fprintf(stderr, "No message\n");
	    exit( FALSE);
	}
    }

    memset(&sunx, 0, sizeof(sunx));
    sunx.sun_family = AF_UNIX;	/* Unix domain socket */
    strncpy(sunx.sun_path, _PATH_LOG, sizeof(sunx.sun_path));
    if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0 ) {
        perror("Couldn't obtain descriptor for socket " _PATH_LOG);
	exit( FALSE);
    }

    addrLength = sizeof(sunx.sun_family) + strlen(sunx.sun_path);

    if (connect(fd, (struct sockaddr *) &sunx, addrLength)) {
	perror("Could not connect to socket " _PATH_LOG);
	exit( FALSE);
    }

    time(&now);
    snprintf (buf, sizeof(buf), "<%d>%.15s %s", pri, ctime(&now)+4, message);
    
    if (toStdErrFlag==TRUE)
	fprintf(stderr, "%s\n", buf);

    write( fd, buf, sizeof(buf));

    close(fd);
    exit( TRUE);
}

