/* vi: set sw=4 ts=4: */
/*
 * Mini syslogd implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2001 by Gennady Feldman <gfeldman@cachier.com>
 *
 * Maintainer: Gennady Feldman <gena01@cachier.com> as of Mar 12, 2001
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

#include "busybox.h"

/* SYSLOG_NAMES defined to pull some extra junk from syslog.h */
#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <sys/uio.h>

/* Path for the file where all log messages are written */
#define __LOG_FILE "/var/log/messages"

/* Path to the unix socket */
static char lfile[BUFSIZ];

static char *logFilePath = __LOG_FILE;

/* interval between marks in seconds */
static int MarkInterval = 20 * 60;

/* localhost's name */
static char LocalHostName[32];

#ifdef CONFIG_FEATURE_REMOTE_LOG
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

/* circular buffer variables/structures */
#ifdef CONFIG_FEATURE_IPC_SYSLOG

#if __GNU_LIBRARY__ < 5
#error Sorry.  Looks like you are using libc5.
#error libc5 shm support isnt good enough.
#error Please disable CONFIG_FEATURE_IPC_SYSLOG
#endif

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

/* our shared key */
static const long KEY_ID = 0x414e4547; /*"GENA"*/

// Semaphore operation structures
static struct shbuf_ds {
	int size;		// size of data written
	int head;		// start of message list
	int tail;		// end of message list
	char data[1];		// data/messages
} *buf = NULL;			// shared memory pointer

static struct sembuf SMwup[1] = {{1, -1, IPC_NOWAIT}}; // set SMwup
static struct sembuf SMwdn[3] = {{0, 0}, {1, 0}, {1, +1}}; // set SMwdn

static int 	shmid = -1;	// ipc shared memory id
static int 	s_semid = -1;	// ipc semaphore id
int	data_size = 16000; // data size
int	shm_size = 16000 + sizeof(*buf); // our buffer size
static int circular_logging = FALSE;

/*
 * sem_up - up()'s a semaphore.
 */
static inline void sem_up(int semid)
{
	if ( semop(semid, SMwup, 1) == -1 )
		perror_msg_and_die("semop[SMwup]");
}

/*
 * sem_down - down()'s a semaphore
 */
static inline void sem_down(int semid)
{
	if ( semop(semid, SMwdn, 3) == -1 )
		perror_msg_and_die("semop[SMwdn]");
}


void ipcsyslog_cleanup(void){
	printf("Exiting Syslogd!\n");
	if (shmid != -1)
		shmdt(buf);

	if (shmid != -1)
		shmctl(shmid, IPC_RMID, NULL);
	if (s_semid != -1)
		semctl(s_semid, 0, IPC_RMID, 0);
}

void ipcsyslog_init(void){
	if (buf == NULL){
	    if ((shmid = shmget(KEY_ID, shm_size, IPC_CREAT | 1023)) == -1)
		    	perror_msg_and_die("shmget");


	    if ((buf = shmat(shmid, NULL, 0)) == NULL)
    			perror_msg_and_die("shmat");


	    buf->size=data_size;
	    buf->head=buf->tail=0;

	    // we'll trust the OS to set initial semval to 0 (let's hope)
	    if ((s_semid = semget(KEY_ID, 2, IPC_CREAT | IPC_EXCL | 1023)) == -1){
	    	if (errno == EEXIST){
		   if ((s_semid = semget(KEY_ID, 2, 0)) == -1)
		    perror_msg_and_die("semget");
		}else
    			perror_msg_and_die("semget");
	    }
	}else{
		printf("Buffer already allocated just grab the semaphore?");
	}
}

/* write message to buffer */
void circ_message(const char *msg){
	int l=strlen(msg)+1; /* count the whole message w/ '\0' included */

	sem_down(s_semid);

	/*
	 * Circular Buffer Algorithm:
	 * --------------------------
	 *
	 * Start-off w/ empty buffer of specific size SHM_SIZ
	 * Start filling it up w/ messages. I use '\0' as separator to break up messages.
	 * This is also very handy since we can do printf on message.
	 *
	 * Once the buffer is full we need to get rid of the first message in buffer and
	 * insert the new message. (Note: if the message being added is >1 message then
	 * we will need to "remove" >1 old message from the buffer). The way this is done
	 * is the following:
	 *	When we reach the end of the buffer we set a mark and start from the beginning.
	 *	Now what about the beginning and end of the buffer? Well we have the "head"
	 *	index/pointer which is the starting point for the messages and we have "tail"
	 *	index/pointer which is the ending point for the messages. When we "display" the
	 *	messages we start from the beginning and continue until we reach "tail". If we
	 *	reach end of buffer, then we just start from the beginning (offset 0). "head" and
	 *	"tail" are actually offsets from the beginning of the buffer.
	 *
	 * Note: This algorithm uses Linux IPC mechanism w/ shared memory and semaphores to provide
	 * 	 a threasafe way of handling shared memory operations.
	 */
	if ( (buf->tail + l) < buf->size ){
		/* before we append the message we need to check the HEAD so that we won't
		   overwrite any of the message that we still need and adjust HEAD to point
		   to the next message! */
		if ( buf->tail < buf->head){
			if ( (buf->tail + l) >= buf->head ){
			  /* we need to move the HEAD to point to the next message
			   * Theoretically we have enough room to add the whole message to the
			   * buffer, because of the first outer IF statement, so we don't have
			   * to worry about overflows here!
			   */
			   int k= buf->tail + l - buf->head; /* we need to know how many bytes
			   					we are overwriting to make
								enough room */
			   char *c=memchr(buf->data+buf->head + k,'\0',buf->size - (buf->head + k));
			   if (c != NULL) {/* do a sanity check just in case! */
			   	buf->head = c - buf->data + 1; /* we need to convert pointer to
								  offset + skip the '\0' since
								  we need to point to the beginning
								  of the next message */
				/* Note: HEAD is only used to "retrieve" messages, it's not used
					when writing messages into our buffer */
			   }else{ /* show an error message to know we messed up? */
			   	printf("Weird! Can't find the terminator token??? \n");
			   	buf->head=0;
			   }
			}
		} /* in other cases no overflows have been done yet, so we don't care! */

		/* we should be ok to append the message now */
		strncpy(buf->data + buf->tail,msg,l); /* append our message */
		buf->tail+=l; /* count full message w/ '\0' terminating char */
	}else{
		/* we need to break up the message and "circle" it around */
		char *c;
		int k=buf->tail + l - buf->size; /* count # of bytes we don't fit */
		
		/* We need to move HEAD! This is always the case since we are going
		 * to "circle" the message.
		 */
		c=memchr(buf->data + k ,'\0', buf->size - k);
		
		if (c != NULL) /* if we don't have '\0'??? weird!!! */{
			/* move head pointer*/
			buf->head=c-buf->data+1; 
			
			/* now write the first part of the message */			
			strncpy(buf->data + buf->tail, msg, l - k - 1);
			
			/* ALWAYS terminate end of buffer w/ '\0' */
			buf->data[buf->size-1]='\0'; 
			
			/* now write out the rest of the string to the beginning of the buffer */
			strcpy(buf->data, &msg[l-k-1]);

			/* we need to place the TAIL at the end of the message */
			buf->tail = k + 1;
		}else{
			printf("Weird! Can't find the terminator token from the beginning??? \n");
			buf->head = buf->tail = 0; /* reset buffer, since it's probably corrupted */
		}
		
	}
	sem_up(s_semid);
}
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

#ifdef CONFIG_FEATURE_IPC_SYSLOG
	if ((circular_logging) && (buf != NULL)){
			char b[1024];
			va_start (arguments, fmt);
			vsprintf (b, fmt, arguments);
			va_end (arguments);
			circ_message(b);

	}else
#endif
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

#ifdef CONFIG_FEATURE_REMOTE_LOG
	/* send message to remote logger */
	if ( -1 != remotefd){
static const int IOV_COUNT = 2;
		struct iovec iov[IOV_COUNT];
		struct iovec *v = iov;

		memset(&res, 0, sizeof(res));
		snprintf(res, sizeof(res), "<%d>", pri);
		v->iov_base = res ;
		v->iov_len = strlen(res);          
		v++;

		v->iov_base = msg;
		v->iov_len = strlen(msg);          

writev_retry:
		if ( -1 == writev(remotefd,iov, IOV_COUNT)){
			if (errno == EINTR) goto writev_retry;
			error_msg_and_die("syslogd: cannot write to remote file handle on"
					"%s:%d",RemoteHost,RemotePort);
		}
	}
	if (local_logging)
#endif
		/* now spew out the message to wherever it is supposed to go */
		message("%s %s %s %s\n", timestamp, LocalHostName, res, msg);


}

static void quit_signal(int sig)
{
	logMessage(LOG_SYSLOG | LOG_INFO, "System log daemon exiting.");
	unlink(lfile);
#ifdef CONFIG_FEATURE_IPC_SYSLOG
	ipcsyslog_cleanup();
#endif

	exit(TRUE);
}

static void domark(int sig)
{
	if (MarkInterval > 0) {
		logMessage(LOG_SYSLOG | LOG_INFO, "-- MARK --");
		alarm(MarkInterval);
	}
}

/* This must be a #define, since when DODEBUG and BUFFERS_GO_IN_BSS are
 * enabled, we otherwise get a "storage size isn't constant error. */
#define BUFSIZE 1023
static int serveConnection (int conn)
{
	RESERVE_CONFIG_BUFFER(tmpbuf, BUFSIZE + 1);
	int    n_read;
	char *p = tmpbuf;

	n_read = read (conn, tmpbuf, BUFSIZE );

	while (p < tmpbuf + n_read) {

		int           pri = (LOG_USER | LOG_NOTICE);
		char          line[ BUFSIZE + 1 ];
		unsigned char c;

		char *q = line;

		tmpbuf[ n_read - 1 ] = '\0';

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
		p++;
		/* Now log it */
		logMessage (pri, line);
	}
	RELEASE_CONFIG_BUFFER (tmpbuf);
	return n_read;
}


#ifdef CONFIG_FEATURE_REMOTE_LOG
static void init_RemoteLog (void){

  struct sockaddr_in remoteaddr;
  struct hostent *hostinfo;
  int len = sizeof(remoteaddr);

  memset(&remoteaddr, 0, len);

  remotefd = socket(AF_INET, SOCK_DGRAM, 0);

  if (remotefd < 0) {
    error_msg_and_die("syslogd: cannot create socket");
  }

  hostinfo = xgethostbyname(RemoteHost);

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
	if (realpath (_PATH_LOG, lfile) != NULL)
		unlink (lfile);

	memset (&sunx, 0, sizeof (sunx));
	sunx.sun_family = AF_UNIX;
	strncpy (sunx.sun_path, lfile, sizeof (sunx.sun_path));
	if ((sock_fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
		perror_msg_and_die ("Couldn't get file descriptor for socket " _PATH_LOG);

	addrLength = sizeof (sunx.sun_family) + strlen (sunx.sun_path);
	if ((bind (sock_fd, (struct sockaddr *) &sunx, addrLength)) || (listen (sock_fd, 5)))
		perror_msg_and_die ("Could not connect to socket " _PATH_LOG);

	if (chmod (lfile, 0666) < 0)
		perror_msg_and_die ("Could not set permission on " _PATH_LOG);

	FD_ZERO (&fds);
	FD_SET (sock_fd, &fds);

#ifdef CONFIG_FEATURE_IPC_SYSLOG
	if (circular_logging ){
	   ipcsyslog_init();
	}
#endif

        #ifdef CONFIG_FEATURE_REMOTE_LOG
        if (doRemoteLog){
          init_RemoteLog();
        }
        #endif

	logMessage (LOG_SYSLOG | LOG_INFO, "syslogd started: " BB_BANNER);

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

					//printf("New Connection request.\n");
					if ((conn = accept (sock_fd, (struct sockaddr *) &sunx, &addrLength)) < 0) {
						perror_msg_and_die ("accept error");
					}

					FD_SET(conn, &fds);
					//printf("conn: %i, set_size: %i\n",conn,FD_SETSIZE);
			  	} else {
					//printf("Serving connection: %i\n",fd);
					  if ( serveConnection(fd) <= 0 ) {
					    close (fd);
					    FD_CLR(fd, &fds);
            }
				} /* fd == sock_fd */
			}/* FD_ISSET() */
		}/* for */
	} /* for main loop */
}

extern int syslogd_main(int argc, char **argv)
{
	int opt;
	int doFork = TRUE;

	char *p;

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "m:nO:R:LC")) > 0) {
		switch (opt) {
			case 'm':
				MarkInterval = atoi(optarg) * 60;
				break;
			case 'n':
				doFork = FALSE;
				break;
			case 'O':
				logFilePath = xstrdup(optarg);
				break;
#ifdef CONFIG_FEATURE_REMOTE_LOG
			case 'R':
				RemoteHost = xstrdup(optarg);
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
#ifdef CONFIG_FEATURE_IPC_SYSLOG
			case 'C':
				circular_logging = TRUE;
				break;
#endif
			default:
				show_usage();
		}
	}

#ifdef CONFIG_FEATURE_REMOTE_LOG
	/* If they have not specified remote logging, then log locally */
	if (! doRemoteLog)
		local_logging = TRUE;
#endif


	/* Store away localhost's name before the fork */
	gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.'))) {
		*p++ = '\0';
	}

	umask(0);

	if (doFork) {
#if !defined(__UCLIBC__) || defined(__UCLIBC_HAS_MMU__)
		if (daemon(0, 1) < 0)
			perror_msg_and_die("daemon");
#else
			error_msg_and_die("daemon not supported");
#endif
	}
	doSyslogd();

	return EXIT_SUCCESS;
}

/*
Local Variables
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
