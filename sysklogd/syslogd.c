/*
 * Mini syslogd implementation for busybox
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


/* SYSLOG_NAMES defined to pull some extra junk from syslog.h */
#define SYSLOG_NAMES
#include <sys/syslog.h>

/* Path for the file where all log messages are written */
#define __LOG_FILE		"/var/log/messages"
/* Path to the current console device */
#define __DEV_CONSOLE		"/dev/console"


static char* logFilePath = __LOG_FILE;
/* interval between marks in seconds */
static int MarkInterval = 20*60;
/* localhost's name */
static char LocalHostName[32];

static const char syslogd_usage[] =
    "syslogd [OPTION]...\n\n"
    "Linux system logging utility.\n\n"
    "Options:\n"
    "\t-m\tChange the mark timestamp interval. default=20min. 0=off\n"
    "\t-n\tDo not fork into the background (for when run by init)\n"
    "\t-O\tSpecify an alternate log file.  default=/var/log/messages\n";



/* try to open up the specified device */
static int device_open(char *device, int mode)
{
    int m, f, fd = -1;

    m = mode | O_NONBLOCK;

    /* Retry up to 5 times */
    for (f = 0; f < 5; f++)
	if ((fd = open(device, m)) >= 0)
	    break;
    if (fd < 0)
	return fd;
    /* Reset original flags. */
    if (m != mode)
	fcntl(fd, F_SETFL, mode);
    return fd;
}

/* print a message to the log file */
static void message(char *fmt, ...)
{
    int fd;
    va_list arguments;

    if ((fd = device_open(logFilePath, O_WRONLY|O_CREAT|O_NOCTTY|O_APPEND|O_NONBLOCK)) >= 0) {
	va_start(arguments, fmt);
	vdprintf(fd, fmt, arguments);
	va_end(arguments);
	close(fd);
    } else {
	/* Always send console messages to /dev/console so people will see them. */
	if ((fd = device_open(__DEV_CONSOLE, O_WRONLY|O_NOCTTY|O_NONBLOCK)) >= 0) {
	    va_start(arguments, fmt);
	    vdprintf(fd, fmt, arguments);
	    va_end(arguments);
	    close(fd);
	} else {
	    fprintf(stderr, "Bummer, can't print: ");
	    va_start(arguments, fmt);
	    vfprintf(stderr, fmt, arguments);
	    fflush(stderr);
	    va_end(arguments);
	}
    }
}

static void logMessage( int pri, char* msg) 
{
    time_t  now;
    char *timestamp;
    static char res[20];
    CODE *c_pri, *c_fac;

    for (c_fac=facilitynames; c_fac->c_name && !(c_fac->c_val==LOG_FAC(pri)<<3); c_fac++);
    for (c_pri=prioritynames; c_pri->c_name && !(c_pri->c_val==LOG_PRI(pri)); c_pri++);
    if (*c_fac->c_name=='\0' || *c_pri->c_name=='\0')
	snprintf (res, sizeof(res), "<%d>", pri);
    else
	snprintf (res, sizeof(res), "%s.%s", c_fac->c_name, c_pri->c_name);

    if (strlen(msg) < 16 || msg[3] != ' ' || msg[6] != ' ' || 
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ') 
    {
	time(&now);
	timestamp = ctime(&now) + 4;
	timestamp[15] = '\0';
    } else {
	timestamp = msg;
	timestamp[15] = '\0';
	msg += 16;
    }

    /* todo: supress duplicates */

    /* now spew out the message to wherever it is supposed to go */
    message( "%s %s %s %s\n", timestamp, LocalHostName, res, msg);
}

static void quit_signal(int sig)
{
    logMessage(LOG_SYSLOG|LOG_INFO, "syslogd exiting");
    exit( TRUE);
}

static void restart_signal(int sig)
{
    /* pretend to restart */
    logMessage(LOG_SYSLOG|LOG_INFO, "syslogd restarting");
}

static void domark(int sig)
{
    if (MarkInterval > 0) {
	logMessage(LOG_SYSLOG|LOG_INFO, "-- MARK --");
	signal(SIGALRM, domark);
	alarm(MarkInterval);
    }
}

static void doSyslogd(void)
{
    struct sockaddr_un sunx;
    int fd, conn;
    size_t addrLength;
    char buf[1024];
    char *q, *p = buf;
    int readSize;

    /* Remove any preexisting socket/file */
    unlink(_PATH_LOG);

    /* Set up sig handlers */
    signal(SIGINT,  quit_signal);
    signal(SIGTERM, quit_signal);
    signal(SIGQUIT, quit_signal);
    signal(SIGHUP,  restart_signal);
    signal(SIGALRM, domark);
    alarm(MarkInterval);

    memset(&sunx, 0, sizeof(sunx));
    sunx.sun_family = AF_UNIX;	/* Unix domain socket */
    strncpy(sunx.sun_path, _PATH_LOG, sizeof(sunx.sun_path));
    if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0 ) {
        perror("Couldn't obtain descriptor for socket " _PATH_LOG);
	exit( FALSE);
    }

    addrLength = sizeof(sunx.sun_family) + strlen(sunx.sun_path);
    if ( (bind(fd, (struct sockaddr *) &sunx, addrLength)) ||
	    (chmod(_PATH_LOG, 0666) < 0) ||
	    (listen(fd, 5)) ) 
    {
	perror("Could not connect to socket " _PATH_LOG);
	exit( FALSE);
    }
    
    
    /* Get localhost's name */
    gethostname(LocalHostName, sizeof(LocalHostName));
    if ( (p = strchr(LocalHostName, '.')) ) {
	*p++ = '\0';
    }
    
    logMessage(LOG_SYSLOG|LOG_INFO, "syslogd started: "
	    "BusyBox v" BB_VER " (" BB_BT ") multi-call binary");


    while ((conn = accept(fd, (struct sockaddr *) &sunx, 
                          &addrLength)) >= 0) 
    {
	while ((readSize=read(conn, buf, sizeof(buf))) > 0)
	{
	    char line[1025];
	    unsigned char c;
	    int pri = (LOG_USER|LOG_NOTICE);

	    memset (line, 0, sizeof(line));
	    p = buf;
	    q = line;
	    while ( p && (c = *p) && q < &line[sizeof(line) - 1]) {
		if (c == '<') {
		    /* Parse the magic priority number */
		    pri = 0;
		    while (isdigit(*(++p))) {
			pri = 10 * pri + (*p - '0');
		    }
		    if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
			pri = (LOG_USER|LOG_NOTICE);
		} else if (c == '\n') {
		    *q++ = ' ';
		} else if (iscntrl(c)&&(c<0177)) {
		    *q++ = '^';
		    *q++ = c ^ 0100;
		} else {
		    *q++ = c;
		}
		p++;
	    }
	    *q = '\0';

	    /* Now log it */
	    logMessage( pri, line);
	}
        close(conn);
    }

    close(fd);
}


extern int syslogd_main(int argc, char **argv)
{
    int	pid;
    int doFork = TRUE;

    while (--argc > 0 && **(++argv) == '-') {
	while (*(++(*argv))) {
	    switch (**argv) {
	    case 'm':
		if (--argc == 0) {
		    usage(syslogd_usage);
		}
		MarkInterval = atoi(*(++argv))*60;
		break;
	    case 'n':
		doFork = FALSE;
		break;
	    case 'O':
		if (--argc == 0) {
		    usage(syslogd_usage);
		}
		logFilePath = *(++argv);
		break;
	    default:
		usage(syslogd_usage);
	    }
	}
    }

    if (doFork == TRUE) {
	pid = fork();
	if ( pid < 0 )
	    exit( pid);
	else if ( pid == 0 ) {
	    doSyslogd();
	}
    } else {
	doSyslogd();
    }
    exit( TRUE);
}


