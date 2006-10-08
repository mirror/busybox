/* vi: set sw=4 ts=4: */
/*
 * Mini syslogd implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2001 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "busybox.h"
#include <paths.h>
#include <stdbool.h>
#include <sys/un.h>

/* SYSLOG_NAMES defined to pull some extra junk from syslog.h */
#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <sys/uio.h>

/* Path to the unix socket */
static char lfile[MAXPATHLEN];

/* Path for the file where all log messages are written */
static const char *logFilePath = "/var/log/messages";

#ifdef CONFIG_FEATURE_ROTATE_LOGFILE
/* max size of message file before being rotated */
static int logFileSize = 200 * 1024;

/* number of rotated message files */
static int logFileRotate = 1;
#endif

/* interval between marks in seconds */
static int MarkInterval = 20 * 60;

/* level of messages to be locally logged */
static int logLevel = 8;

/* localhost's name */
static char LocalHostName[64];

#ifdef CONFIG_FEATURE_REMOTE_LOG
#include <netinet/in.h>
/* udp socket for logging to remote host */
static int remotefd = -1;
static struct sockaddr_in remoteaddr;

#endif

/* options */
/* Correct regardless of combination of CONFIG_xxx */
enum {
	OPTBIT_mark = 0, // -m
	OPTBIT_nofork, // -n
	OPTBIT_outfile, // -O
	OPTBIT_loglevel, // -l
	OPTBIT_small, // -S
	USE_FEATURE_ROTATE_LOGFILE(OPTBIT_filesize   ,)	// -s
	USE_FEATURE_ROTATE_LOGFILE(OPTBIT_rotatecnt  ,)	// -b
	USE_FEATURE_REMOTE_LOG(    OPTBIT_remote     ,)	// -R
	USE_FEATURE_REMOTE_LOG(    OPTBIT_localtoo   ,)	// -L
	USE_FEATURE_IPC_SYSLOG(    OPTBIT_circularlog,)	// -C

	OPT_mark        = 1 << OPTBIT_mark    ,
	OPT_nofork      = 1 << OPTBIT_nofork  ,
	OPT_outfile     = 1 << OPTBIT_outfile ,
	OPT_loglevel    = 1 << OPTBIT_loglevel,
	OPT_small       = 1 << OPTBIT_small   ,
	OPT_filesize    = USE_FEATURE_ROTATE_LOGFILE((1 << OPTBIT_filesize   )) + 0,
	OPT_rotatecnt   = USE_FEATURE_ROTATE_LOGFILE((1 << OPTBIT_rotatecnt  )) + 0,
	OPT_remotelog   = USE_FEATURE_REMOTE_LOG(    (1 << OPTBIT_remote     )) + 0,
	OPT_locallog    = USE_FEATURE_REMOTE_LOG(    (1 << OPTBIT_localtoo   )) + 0,
	OPT_circularlog = USE_FEATURE_IPC_SYSLOG(    (1 << OPTBIT_circularlog)) + 0,
};
#define OPTION_STR "m:nO:l:S" \
	USE_FEATURE_ROTATE_LOGFILE("s:" ) \
	USE_FEATURE_ROTATE_LOGFILE("b:" ) \
	USE_FEATURE_REMOTE_LOG(    "R:" ) \
	USE_FEATURE_REMOTE_LOG(    "L"  ) \
	USE_FEATURE_IPC_SYSLOG(    "C::")
#define OPTION_DECL *opt_m, *opt_l \
	USE_FEATURE_ROTATE_LOGFILE(,*opt_s) \
	USE_FEATURE_ROTATE_LOGFILE(,*opt_b) \
	USE_FEATURE_REMOTE_LOG(    ,*opt_R) \
	USE_FEATURE_IPC_SYSLOG(    ,*opt_C = NULL)
#define OPTION_PARAM &opt_m, &logFilePath, &opt_l \
	USE_FEATURE_ROTATE_LOGFILE(,&opt_s) \
	USE_FEATURE_ROTATE_LOGFILE(,&opt_b) \
	USE_FEATURE_REMOTE_LOG(    ,&opt_R) \
	USE_FEATURE_IPC_SYSLOG(    ,&opt_C)

#define MAXLINE         1024	/* maximum line length */

/* circular buffer variables/structures */
#ifdef CONFIG_FEATURE_IPC_SYSLOG

#if CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE < 4
#error Sorry, you must set the syslogd buffer size to at least 4KB.
#error Please check CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE
#endif

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

/* our shared key */
#define KEY_ID ((long)0x414e4547) /* "GENA" */

// Semaphore operation structures
static struct shbuf_ds {
	int size;       // size of data written
	int head;       // start of message list
	int tail;       // end of message list
	char data[1];   // data/messages
} *shbuf = NULL;        // shared memory pointer

static struct sembuf SMwup[1] = { {1, -1, IPC_NOWAIT} };	// set SMwup
static struct sembuf SMwdn[3] = { {0, 0}, {1, 0}, {1, +1} };	// set SMwdn

static int shmid = -1;   // ipc shared memory id
static int s_semid = -1; // ipc semaphore id
static int shm_size = ((CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE)*1024);	// default shm size

static void ipcsyslog_cleanup(void)
{
	puts("Exiting syslogd!");
	if (shmid != -1) {
		shmdt(shbuf);
	}

	if (shmid != -1) {
		shmctl(shmid, IPC_RMID, NULL);
	}
	if (s_semid != -1) {
		semctl(s_semid, 0, IPC_RMID, 0);
	}
}

static void ipcsyslog_init(void)
{
	if (shbuf == NULL) {
		shmid = shmget(KEY_ID, shm_size, IPC_CREAT | 1023);
		if (shmid == -1) {
			bb_perror_msg_and_die("shmget");
		}

		shbuf = shmat(shmid, NULL, 0);
		if (!shbuf) {
			bb_perror_msg_and_die("shmat");
		}

		shbuf->size = shm_size - sizeof(*shbuf);
		shbuf->head = shbuf->tail = 0;

		// we'll trust the OS to set initial semval to 0 (let's hope)
		s_semid = semget(KEY_ID, 2, IPC_CREAT | IPC_EXCL | 1023);
		if (s_semid == -1) {
			if (errno == EEXIST) {
				s_semid = semget(KEY_ID, 2, 0);
				if (s_semid == -1) {
					bb_perror_msg_and_die("semget");
				}
			} else {
				bb_perror_msg_and_die("semget");
			}
		}
	} else {
		printf("Buffer already allocated just grab the semaphore?");
	}
}

/* write message to buffer */
static void circ_message(const char *msg)
{
	int l = strlen(msg) + 1;	/* count the whole message w/ '\0' included */
	const char * const fail_msg = "Can't find the terminator token%s?\n";

	if (semop(s_semid, SMwdn, 3) == -1) {
		bb_perror_msg_and_die("SMwdn");
	}

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
	 *      When we reach the end of the buffer we set a mark and start from the beginning.
	 *      Now what about the beginning and end of the buffer? Well we have the "head"
	 *      index/pointer which is the starting point for the messages and we have "tail"
	 *      index/pointer which is the ending point for the messages. When we "display" the
	 *      messages we start from the beginning and continue until we reach "tail". If we
	 *      reach end of buffer, then we just start from the beginning (offset 0). "head" and
	 *      "tail" are actually offsets from the beginning of the buffer.
	 *
	 * Note: This algorithm uses Linux IPC mechanism w/ shared memory and semaphores to provide
	 *       a threadsafe way of handling shared memory operations.
	 */
	if ((shbuf->tail + l) < shbuf->size) {
		/* before we append the message we need to check the HEAD so that we won't
		   overwrite any of the message that we still need and adjust HEAD to point
		   to the next message! */
		if (shbuf->tail < shbuf->head) {
			if ((shbuf->tail + l) >= shbuf->head) {
				/* we need to move the HEAD to point to the next message
				 * Theoretically we have enough room to add the whole message to the
				 * buffer, because of the first outer IF statement, so we don't have
				 * to worry about overflows here!
				 */
				/* we need to know how many bytes we are overwriting to make enough room */
				int k = shbuf->tail + l - shbuf->head;
				char *c =
					memchr(shbuf->data + shbuf->head + k, '\0',
						   shbuf->size - (shbuf->head + k));
				if (c != NULL) { /* do a sanity check just in case! */
					/* we need to convert pointer to offset + skip the '\0'
					   since we need to point to the beginning of the next message */
					shbuf->head = c - shbuf->data + 1;
					/* Note: HEAD is only used to "retrieve" messages, it's not used
					   when writing messages into our buffer */
				} else { /* show an error message to know we messed up? */
					printf(fail_msg,"");
					shbuf->head = 0;
				}
			}
		}

		/* in other cases no overflows have been done yet, so we don't care! */
		/* we should be ok to append the message now */
		strncpy(shbuf->data + shbuf->tail, msg, l);	/* append our message */
		shbuf->tail += l;	/* count full message w/ '\0' terminating char */
	} else {
		/* we need to break up the message and "circle" it around */
		char *c;
		int k = shbuf->tail + l - shbuf->size;	/* count # of bytes we don't fit */

		/* We need to move HEAD! This is always the case since we are going
		 * to "circle" the message. */
		c = memchr(shbuf->data + k, '\0', shbuf->size - k);

		if (c != NULL) {	/* if we don't have '\0'??? weird!!! */
			/* move head pointer */
			shbuf->head = c - shbuf->data + 1;

			/* now write the first part of the message */
			strncpy(shbuf->data + shbuf->tail, msg, l - k - 1);

			/* ALWAYS terminate end of buffer w/ '\0' */
			shbuf->data[shbuf->size - 1] = '\0';

			/* now write out the rest of the string to the beginning of the buffer */
			strcpy(shbuf->data, &msg[l - k - 1]);

			/* we need to place the TAIL at the end of the message */
			shbuf->tail = k + 1;
		} else {
			printf(fail_msg, " from the beginning");
			shbuf->head = shbuf->tail = 0;	/* reset buffer, since it's probably corrupted */
		}

	}
	if (semop(s_semid, SMwup, 1) == -1) {
		bb_perror_msg_and_die("SMwup");
	}

}
#else
void ipcsyslog_cleanup(void);
void ipcsyslog_init(void);
void circ_message(const char *msg);
#endif							/* CONFIG_FEATURE_IPC_SYSLOG */

/* Note: There is also a function called "message()" in init.c */
/* Print a message to the log file. */
static void message(char *fmt, ...) __attribute__ ((format(printf, 1, 2)));
static void message(char *fmt, ...)
{
	int fd = -1;
	struct flock fl;
	va_list arguments;

	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

#ifdef CONFIG_FEATURE_IPC_SYSLOG
	if ((option_mask32 & OPT_circularlog) && shbuf) {
		char b[1024];

		va_start(arguments, fmt);
		vsnprintf(b, sizeof(b) - 1, fmt, arguments);
		va_end(arguments);
		circ_message(b);

	} else
#endif
	fd = device_open(logFilePath, O_WRONLY | O_CREAT
					| O_NOCTTY | O_APPEND | O_NONBLOCK);
	if (fd >= 0) {
		fl.l_type = F_WRLCK;
		fcntl(fd, F_SETLKW, &fl);

#ifdef CONFIG_FEATURE_ROTATE_LOGFILE
		if (ENABLE_FEATURE_ROTATE_LOGFILE && logFileSize > 0 ) {
			struct stat statf;
			int r = fstat(fd, &statf);
			if (!r && (statf.st_mode & S_IFREG)
					&& (lseek(fd,0,SEEK_END) > logFileSize)) {
				if (logFileRotate > 0) {
					int i = strlen(logFilePath) + 4;
					char oldFile[i];
					char newFile[i];
					for (i=logFileRotate-1; i>0; i--) {
						sprintf(oldFile, "%s.%d", logFilePath, i-1);
						sprintf(newFile, "%s.%d", logFilePath, i);
						rename(oldFile, newFile);
					}
					sprintf(newFile, "%s.%d", logFilePath, 0);
					fl.l_type = F_UNLCK;
					fcntl(fd, F_SETLKW, &fl);
					close(fd);
					rename(logFilePath, newFile);
					fd = device_open(logFilePath,
						   O_WRONLY | O_CREAT | O_NOCTTY | O_APPEND |
						   O_NONBLOCK);
					fl.l_type = F_WRLCK;
					fcntl(fd, F_SETLKW, &fl);
				} else {
					ftruncate(fd, 0);
				}
			}
		}
#endif
		va_start(arguments, fmt);
		vdprintf(fd, fmt, arguments);
		va_end(arguments);
		fl.l_type = F_UNLCK;
		fcntl(fd, F_SETLKW, &fl);
		close(fd);
	} else {
		/* Always send console messages to /dev/console so people will see them. */
		fd = device_open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY | O_NONBLOCK);
		if (fd >= 0) {
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

static void logMessage(int pri, char *msg)
{
	time_t now;
	char *timestamp;
	char res[20];
	CODE *c_pri, *c_fac;

	if (pri != 0) {
		c_fac = facilitynames;
		while (c_fac->c_name && !(c_fac->c_val == LOG_FAC(pri) << 3))
			c_fac++;
		c_pri = prioritynames;
		while (c_pri->c_name && !(c_pri->c_val == LOG_PRI(pri)))
			c_pri++;
		if (c_fac->c_name == NULL || c_pri->c_name == NULL) {
			snprintf(res, sizeof(res), "<%d>", pri);
		} else {
			snprintf(res, sizeof(res), "%s.%s", c_fac->c_name, c_pri->c_name);
		}
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
	if (option_mask32 & OPT_remotelog) {
		char line[MAXLINE + 1];
		/* trying connect the socket */
		if (-1 == remotefd) {
			remotefd = socket(AF_INET, SOCK_DGRAM, 0);
		}
		/* if we have a valid socket, send the message */
		if (-1 != remotefd) {
			snprintf(line, sizeof(line), "<%d>%s", pri, msg);
			/* send message to remote logger, ignore possible error */
			sendto(remotefd, line, strlen(line), 0,
					(struct sockaddr *) &remoteaddr, sizeof(remoteaddr));
		}
	}

	if (option_mask32 & OPT_locallog)
#endif
	{
		/* now spew out the message to wherever it is supposed to go */
		if (pri == 0 || LOG_PRI(pri) < logLevel) {
			if (option_mask32 & OPT_small)
				message("%s %s\n", timestamp, msg);
			else
				message("%s %s %s %s\n", timestamp, LocalHostName, res, msg);
		}
	}
}

static void quit_signal(int sig)
{
	logMessage(LOG_SYSLOG | LOG_INFO, "System log daemon exiting.");
	unlink(lfile);
	if (ENABLE_FEATURE_IPC_SYSLOG)
		ipcsyslog_cleanup();

	exit(1);
}

static void domark(int sig)
{
	if (MarkInterval > 0) {
		logMessage(LOG_SYSLOG | LOG_INFO, "-- MARK --");
		alarm(MarkInterval);
	}
}

/* This must be a #define, since when CONFIG_DEBUG and BUFFERS_GO_IN_BSS are
 * enabled, we otherwise get a "storage size isn't constant error. */
static int serveConnection(char *tmpbuf, int n_read)
{
	char *p = tmpbuf;

	while (p < tmpbuf + n_read) {

		int pri = (LOG_USER | LOG_NOTICE);
		int num_lt = 0;
		char line[MAXLINE + 1];
		unsigned char c;
		char *q = line;

		while ((c = *p) && q < &line[sizeof(line) - 1]) {
			if (c == '<' && num_lt == 0) {
				/* Parse the magic priority number. */
				num_lt++;
				pri = 0;
				while (isdigit(*(++p))) {
					pri = 10 * pri + (*p - '0');
				}
				if (pri & ~(LOG_FACMASK | LOG_PRIMASK)) {
					pri = (LOG_USER | LOG_NOTICE);
				}
			} else if (c == '\n') {
				*q++ = ' ';
			} else if (iscntrl(c) && (c < 0177)) {
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
		logMessage(pri, line);
	}
	return n_read;
}

static void doSyslogd(void) ATTRIBUTE_NORETURN;
static void doSyslogd(void)
{
	struct sockaddr_un sunx;
	socklen_t addrLength;

	int sock_fd;
	fd_set fds;

	/* Set up signal handlers. */
	signal(SIGINT, quit_signal);
	signal(SIGTERM, quit_signal);
	signal(SIGQUIT, quit_signal);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#ifdef SIGCLD
	signal(SIGCLD, SIG_IGN);
#endif
	signal(SIGALRM, domark);
	alarm(MarkInterval);

	/* Create the syslog file so realpath() can work. */
	if (realpath(_PATH_LOG, lfile) != NULL) {
		unlink(lfile);
	}

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	strncpy(sunx.sun_path, lfile, sizeof(sunx.sun_path));
	sock_fd = xsocket(AF_UNIX, SOCK_DGRAM, 0);
	addrLength = sizeof(sunx.sun_family) + strlen(sunx.sun_path);
	if (bind(sock_fd, (struct sockaddr *) &sunx, addrLength) < 0) {
		bb_perror_msg_and_die("cannot connect to socket %s", lfile);
	}

	if (chmod(lfile, 0666) < 0) {
		bb_perror_msg_and_die("cannot set permission on %s", lfile);
	}
	if (ENABLE_FEATURE_IPC_SYSLOG && (option_mask32 & OPT_circularlog)) {
		ipcsyslog_init();
	}

	logMessage(LOG_SYSLOG | LOG_INFO, "syslogd started: " "BusyBox v" BB_VER );

	for (;;) {
		FD_ZERO(&fds);
		FD_SET(sock_fd, &fds);

		if (select(sock_fd + 1, &fds, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				/* alarm may have happened. */
				continue;
			}
			bb_perror_msg_and_die("select");
		}

		if (FD_ISSET(sock_fd, &fds)) {
			int i;
#if MAXLINE > BUFSIZ
# define TMP_BUF_SZ BUFSIZ
#else
# define TMP_BUF_SZ MAXLINE
#endif
#define tmpbuf bb_common_bufsiz1

			if ((i = recv(sock_fd, tmpbuf, TMP_BUF_SZ, 0)) > 0) {
				tmpbuf[i] = '\0';
				serveConnection(tmpbuf, i);
			} else {
				bb_perror_msg_and_die("UNIX socket error");
			}
		}				/* FD_ISSET() */
	}					/* for main loop */
}


int syslogd_main(int argc, char **argv)
{
	char OPTION_DECL;
	char *p;

	/* do normal option parsing */
	getopt32(argc, argv, OPTION_STR, OPTION_PARAM);
	if (option_mask32 & OPT_mark) MarkInterval = xatoul_range(opt_m, 0, INT_MAX/60) * 60; // -m
	//if (option_mask32 & OPT_nofork) // -n
	//if (option_mask32 & OPT_outfile) // -O
	if (option_mask32 & OPT_loglevel) { // -l
		logLevel = xatoi_u(opt_l);
		/* Valid levels are between 1 and 8 */
		if (logLevel < 1 || logLevel > 8)
			bb_show_usage();
	}
	//if (option_mask32 & OPT_small) // -S
#if ENABLE_FEATURE_ROTATE_LOGFILE
	if (option_mask32 & OPT_filesize) logFileSize = xatoul_range(opt_s, 0, INT_MAX/1024) * 1024; // -s
	if (option_mask32 & OPT_rotatecnt) { // -b
		logFileRotate = xatoi_u(opt_b);
		if (logFileRotate > 99) logFileRotate = 99; 
	}
#endif
#if ENABLE_FEATURE_REMOTE_LOG
	if (option_mask32 & OPT_remotelog) { // -R
		int port = 514;
		char *host = xstrdup(opt_R);
		p = strchr(host, ':');
		if (p) {
			port = xatou16(p + 1);
			*p = '\0';
		}
		remoteaddr.sin_family = AF_INET;
		/* FIXME: looks ip4-specific. need to do better */
		remoteaddr.sin_addr = *(struct in_addr *) *(xgethostbyname(host)->h_addr_list);
		remoteaddr.sin_port = htons(port);
		free(host);
	}
	//if (option_mask32 & OPT_locallog) // -L
#endif
#if ENABLE_FEATURE_IPC_SYSLOG
	if (option_mask32 & OPT_circularlog) { // -C
		if (opt_C) {
			shm_size = xatoul_range(opt_C, 4, INT_MAX/1024) * 1024;
		}
	}
#endif

	/* If they have not specified remote logging, then log locally */
	if (ENABLE_FEATURE_REMOTE_LOG && !(option_mask32 & OPT_remotelog))
		option_mask32 |= OPT_locallog;

	/* Store away localhost's name before the fork */
	gethostname(LocalHostName, sizeof(LocalHostName));
	p = strchr(LocalHostName, '.');
	if (p) {
		*p = '\0';
	}

	umask(0);

	if (!(option_mask32 & OPT_nofork)) {
#ifdef BB_NOMMU
		vfork_daemon_rexec(0, 1, argc, argv, "-n");
#else
		xdaemon(0, 1);
#endif
	}
	doSyslogd();

	return EXIT_SUCCESS;
}
