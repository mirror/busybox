/* vi: set sw=4 ts=4: */
/*
 * Mini syslogd implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
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

#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/param.h>

#if ! defined __GLIBC__ && ! defined __UCLIBC__
#include <sys/syscall.h>
#include <linux/unistd.h>
typedef unsigned int socklen_t;

#ifndef __alpha__
# define __NR_klogctl __NR_syslog
static inline _syscall3(int, klogctl, int, type, char *, b, int, len);
#else							/* __alpha__ */
#define klogctl syslog
#endif

#else
# include <sys/klog.h>
#endif



/* SYSLOG_NAMES defined to pull some extra junk from syslog.h */
#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <sys/uio.h>

/* Path for the file where all log messages are written */
#define __LOG_FILE "/var/log/messages"

/* Path to the unix socket */
char lfile[BUFSIZ] = "";

static char *logFilePath = __LOG_FILE;

/* interval between marks in seconds */
static int MarkInterval = 20 * 60;

/* localhost's name */
static char LocalHostName[32];

#ifdef BB_FEATURE_REMOTE_LOG
#include <netinet/in.h>
/* udp socket for logging to remote host */
static int remotefd = -1;
/* where do we log? */
static char *RemoteHost;
/* what port to log to? */
static int RemotePort = 514;
/* To remote log or not to remote log, that is the question. */
static int doRemoteLog = FALSE;
static int local_logging = FALSE;
#endif

/* Note: There is also a function called "message()" in init.c */
/* Print a message to the log file. */
static void message (char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void message (char *fmt, ...)
{
	int fd;
	struct flock fl;
	va_list arguments;

	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len    = 1;

	if ((fd = device_open (logFilePath,
						   O_WRONLY | O_CREAT | O_NOCTTY | O_APPEND |
						   O_NONBLOCK)) >= 0) {
		fl.l_type = F_WRLCK;
		fcntl (fd, F_SETLKW, &fl);
		va_start (arguments, fmt);
		vdprintf (fd, fmt, arguments);
		va_end (arguments);
		fl.l_type = F_UNLCK;
		fcntl (fd, F_SETLKW, &fl);
		close (fd);
	} else {
		/* Always send console messages to /dev/console so people will see them. */
		if ((fd = device_open (_PATH_CONSOLE,
							   O_WRONLY | O_NOCTTY | O_NONBLOCK)) >= 0) {
			va_start (arguments, fmt);
			vdprintf (fd, fmt, arguments);
			va_end (arguments);
			close (fd);
		} else {
			fprintf (stderr, "Bummer, can't print: ");
			va_start (arguments, fmt);
			vfprintf (stderr, fmt, arguments);
			fflush (stderr);
			va_end (arguments);
		}
	}
}

static void logMessage (int pri, char *msg)
{
	time_t now;
	char *timestamp;
	static char res[20] = "";
	CODE *c_pri, *c_fac;

	if (pri != 0) {
		for (c_fac = facilitynames;
				c_fac->c_name && !(c_fac->c_val == LOG_FAC(pri) << 3); c_fac++);
		for (c_pri = prioritynames;
				c_pri->c_name && !(c_pri->c_val == LOG_PRI(pri)); c_pri++);
		if (c_fac->c_name == NULL || c_pri->c_name == NULL)
			snprintf(res, sizeof(res), "<%d>", pri);
		else
			snprintf(res, sizeof(res), "%s.%s", c_fac->c_name, c_pri->c_name);
	}

	if (strlen(msg) < 16 || msg[3] != ' ' || msg[6] != ' ' ||
			msg[9] != ':' || msg[12] != ':' || msg[15] != ' ') {
		time(&now);
		timestamp = ctime(&now) + 4;
		timestamp[15] = '\0';
	} else {
		timestamp = msg;
		timestamp[15] = '\0';
		msg += 16;
	}

	/* todo: supress duplicates */

#ifdef BB_FEATURE_REMOTE_LOG
	/* send message to remote logger */
	if ( -1 != remotefd){
static const int IOV_COUNT = 2;
		struct iovec iov[IOV_COUNT];
		struct iovec *v = iov;

		bzero(&res, sizeof(res));
		snprintf(res, sizeof(res), "<%d>", pri);
		v->iov_base = res ;
		v->iov_len = strlen(res);          
		v++;

		v->iov_base = msg;
		v->iov_len = strlen(msg);          

		if ( -1 == writev(remotefd,iov, IOV_COUNT)){
			error_msg_and_die("syslogd: cannot write to remote file handle on" 
					"%s:%d",RemoteHost,RemotePort);
		}
	}
	if (local_logging == TRUE)
#endif
		/* now spew out the message to wherever it is supposed to go */
		message("%s %s %s %s\n", timestamp, LocalHostName, res, msg);


}

static void quit_signal(int sig)
{
	logMessage(0, "System log daemon exiting.");
	unlink(lfile);
	exit(TRUE);
}

static void domark(int sig)
{
	if (MarkInterval > 0) {
		logMessage(LOG_SYSLOG | LOG_INFO, "-- MARK --");
		alarm(MarkInterval);
	}
}

static const int BUFSIZE = 1023;
static int serveConnection (int conn)
{
	RESERVE_BB_BUFFER(buf, BUFSIZE + 1);
	int    n_read;

	while ((n_read = read (conn, buf, BUFSIZE )) > 0) {

		int           pri = (LOG_USER | LOG_NOTICE);
		char          line[ BUFSIZE + 1 ];
		unsigned char c;

		char *p = buf, *q = line;

		buf[ n_read - 1 ] = '\0';

		while (p && (c = *p) && q < &line[ sizeof (line) - 1 ]) {
			if (c == '<') {
			/* Parse the magic priority number. */
				pri = 0;
				while (isdigit (*(++p))) {
					pri = 10 * pri + (*p - '0');
				}
				if (pri & ~(LOG_FACMASK | LOG_PRIMASK)){
					pri = (LOG_USER | LOG_NOTICE);
				}
			} else if (c == '\n') {
				*q++ = ' ';
			} else if (iscntrl (c) && (c < 0177)) {
				*q++ = '^';
				*q++ = c ^ 0100;
			} else {
				*q++ = c;
			}
			p++;
		}
		*q = '\0';
		/* Now log it */
		logMessage (pri, line);
	}
	return (0);
}


#ifdef BB_FEATURE_REMOTE_LOG
static void init_RemoteLog (void){

  struct sockaddr_in remoteaddr;
  struct hostent *hostinfo;
  int len = sizeof(remoteaddr);

  bzero(&remoteaddr, len);
  
  remotefd = socket(AF_INET, SOCK_DGRAM, 0);

  if (remotefd < 0) {
    error_msg_and_die("syslogd: cannot create socket");
  }

  hostinfo = (struct hostent *) gethostbyname(RemoteHost);

  if (!hostinfo) {
    error_msg_and_die("syslogd: cannot resolve remote host name [%s]", RemoteHost);
  }

  remoteaddr.sin_family = AF_INET;
  remoteaddr.sin_addr = *(struct in_addr *) *hostinfo->h_addr_list;
  remoteaddr.sin_port = htons(RemotePort);

  /* 
     Since we are using UDP sockets, connect just sets the default host and port 
     for future operations
  */
  if ( 0 != (connect(remotefd, (struct sockaddr *) &remoteaddr, len))){
    error_msg_and_die("syslogd: cannot connect to remote host %s:%d", RemoteHost, RemotePort);
  }

}
#endif

static void doSyslogd (void) __attribute__ ((noreturn));
static void doSyslogd (void)
{
	struct sockaddr_un sunx;
	socklen_t addrLength;


	int sock_fd;
	fd_set fds;

	RESERVE_BB_BUFFER(lfile, BUFSIZ);

	/* Set up signal handlers. */
	signal (SIGINT,  quit_signal);
	signal (SIGTERM, quit_signal);
	signal (SIGQUIT, quit_signal);
	signal (SIGHUP,  SIG_IGN);
	signal (SIGCHLD,  SIG_IGN);
#ifdef SIGCLD
	signal (SIGCLD,  SIG_IGN);
#endif
	signal (SIGALRM, domark);
	alarm (MarkInterval);

	/* Create the syslog file so realpath() can work. */
	close (open (_PATH_LOG, O_RDWR | O_CREAT, 0644));
	if (realpath (_PATH_LOG, lfile) == NULL)
		perror_msg_and_die ("Could not resolve path to " _PATH_LOG);

	unlink (lfile);

	memset (&sunx, 0, sizeof (sunx));
	sunx.sun_family = AF_UNIX;
	strncpy (sunx.sun_path, lfile, sizeof (sunx.sun_path));
	if ((sock_fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
		perror_msg_and_die ("Couldn't obtain descriptor for socket " _PATH_LOG);

	addrLength = sizeof (sunx.sun_family) + strlen (sunx.sun_path);
	if ((bind (sock_fd, (struct sockaddr *) &sunx, addrLength)) || (listen (sock_fd, 5)))
		perror_msg_and_die ("Could not connect to socket " _PATH_LOG);

	if (chmod (lfile, 0666) < 0)
		perror_msg_and_die ("Could not set permission on " _PATH_LOG);

	FD_ZERO (&fds);
	FD_SET (sock_fd, &fds);

        #ifdef BB_FEATURE_REMOTE_LOG
        if (doRemoteLog == TRUE){
          init_RemoteLog();
        }
        #endif

	logMessage (0, "syslogd started: BusyBox v" BB_VER " (" BB_BT ")");

	for (;;) {

		fd_set readfds;
		int    n_ready;
		int    fd;

		memcpy (&readfds, &fds, sizeof (fds));

		if ((n_ready = select (FD_SETSIZE, &readfds, NULL, NULL, NULL)) < 0) {
			if (errno == EINTR) continue; /* alarm may have happened. */
			perror_msg_and_die ("select error");
		}

		for (fd = 0; (n_ready > 0) && (fd < FD_SETSIZE); fd++) {
			if (FD_ISSET (fd, &readfds)) {

				--n_ready;

				if (fd == sock_fd) {

					int   conn;
					pid_t pid;

					if ((conn = accept (sock_fd, (struct sockaddr *) &sunx, &addrLength)) < 0) {
						perror_msg_and_die ("accept error");
					}

					pid = fork();

					if (pid < 0) {
						perror ("syslogd: fork");
						close (conn);
						continue;
					}

					if (pid == 0) {
						serveConnection (conn);
						close (conn);
						exit( TRUE);
					}
					close (conn);
				}
			}
		}
	}
}

#ifdef BB_FEATURE_KLOGD

static void klogd_signal(int sig)
{
	klogctl(7, NULL, 0);
	klogctl(0, 0, 0);
	logMessage(0, "Kernel log daemon exiting.");
	exit(TRUE);
}

static void doKlogd (void) __attribute__ ((noreturn));
static void doKlogd (void)
{
	int priority = LOG_INFO;
	char log_buffer[4096];
	int i, n, lastc;
	char *start;

	/* Set up sig handlers */
	signal(SIGINT, klogd_signal);
	signal(SIGKILL, klogd_signal);
	signal(SIGTERM, klogd_signal);
	signal(SIGHUP, SIG_IGN);

#ifdef BB_FEATURE_REMOTE_LOG
        if (doRemoteLog == TRUE){
          init_RemoteLog();
        }
#endif

	logMessage(0, "klogd started: "
			   "BusyBox v" BB_VER " (" BB_BT ")");

	/* "Open the log. Currently a NOP." */
	klogctl(1, NULL, 0);

	while (1) {
		/* Use kernel syscalls */
		memset(log_buffer, '\0', sizeof(log_buffer));
		n = klogctl(2, log_buffer, sizeof(log_buffer));
		if (n < 0) {
			char message[80];

			if (errno == EINTR)
				continue;
			snprintf(message, 79, "klogd: Error return from sys_sycall: " \
					 "%d - %s.\n", errno, strerror(errno));
			logMessage(LOG_SYSLOG | LOG_ERR, message);
			exit(1);
		}

		/* klogctl buffer parsing modelled after code in dmesg.c */
		start=&log_buffer[0];
		lastc='\0';
		for (i=0; i<n; i++) {
			if (lastc == '\0' && log_buffer[i] == '<') {
				priority = 0;
				i++;
				while (isdigit(log_buffer[i])) {
					priority = priority*10+(log_buffer[i]-'0');
					i++;
				}
				if (log_buffer[i] == '>') i++;
				start = &log_buffer[i];
			}
			if (log_buffer[i] == '\n') {
				log_buffer[i] = '\0';  /* zero terminate this message */
				logMessage(LOG_KERN | priority, start);
				start = &log_buffer[i+1];
				priority = LOG_INFO;
			}
			lastc = log_buffer[i];
		}
	}

}

#endif

static void daemon_init (char **argv, char *dz, void fn (void))
{
	setsid();
	chdir ("/");
	strncpy(argv[0], dz, strlen(argv[0]));
	fn();
	exit(0);
}

extern int syslogd_main(int argc, char **argv)
{
	int opt, pid, klogd_pid;
	int doFork = TRUE;

#ifdef BB_FEATURE_KLOGD
	int startKlogd = TRUE;
#endif
	char *p;

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "m:nKO:R:L")) > 0) {
		switch (opt) {
			case 'm':
				MarkInterval = atoi(optarg) * 60;
				break;
			case 'n':
				doFork = FALSE;
				break;
#ifdef BB_FEATURE_KLOGD
			case 'K':
				startKlogd = FALSE;
				break;
#endif
			case 'O':
				logFilePath = strdup(optarg);
				break;
#ifdef BB_FEATURE_REMOTE_LOG
			case 'R':
				RemoteHost = strdup(optarg);
				if ( (p = strchr(RemoteHost, ':'))){
					RemotePort = atoi(p+1);
					*p = '\0';
				}          
				doRemoteLog = TRUE;
				break;
			case 'L':
				local_logging = TRUE;
				break;
#endif
			default:
				usage(syslogd_usage);
		}
	}

#ifdef BB_FEATURE_REMOTE_LOG
	/* If they have not specified remote logging, then log locally */
	if (doRemoteLog == FALSE)
		local_logging = TRUE;
#endif

	/* Store away localhost's name before the fork */
	gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.'))) {
		*p++ = '\0';
	}

	umask(0);

#ifdef BB_FEATURE_KLOGD
	/* Start up the klogd process */
	if (startKlogd == TRUE) {
		klogd_pid = fork();
		if (klogd_pid == 0) {
			daemon_init (argv, "klogd", doKlogd);
		}
	}
#endif

	if (doFork == TRUE) {
		pid = fork();
		if (pid < 0)
			exit(pid);
		else if (pid == 0) {
			daemon_init (argv, "syslogd", doSyslogd);
		}
	} else {
		doSyslogd();
	}

	return EXIT_SUCCESS;
}

/*
Local Variables
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
