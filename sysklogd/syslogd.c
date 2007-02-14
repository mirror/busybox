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
#include <sys/un.h>

/* SYSLOG_NAMES defined to pull some extra junk from syslog.h */
#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <sys/uio.h>

#define DEBUG 0

/* Path for the file where all log messages are written */
static const char *logFilePath = "/var/log/messages";
static int logFD = -1;

/* This is not very useful, is bloat, and broken:
 * can deadlock if alarmed to make MARK while writing to IPC buffer
 * (semaphores are down but do_mark routine tries to down them again) */
#ifdef SYSLOGD_MARK
/* interval between marks in seconds */
static int markInterval = 20 * 60;
#endif

/* level of messages to be locally logged */
static int logLevel = 8;

/* localhost's name */
static char localHostName[64];

#if ENABLE_FEATURE_ROTATE_LOGFILE
/* max size of message file before being rotated */
static unsigned logFileSize = 200 * 1024;
/* number of rotated message files */
static unsigned logFileRotate = 1;
static unsigned curFileSize;
static smallint isRegular;
#endif

#if ENABLE_FEATURE_REMOTE_LOG
#include <netinet/in.h>
/* udp socket for logging to remote host */
static int remoteFD = -1;
static len_and_sockaddr* remoteAddr;
#endif

/* We are using bb_common_bufsiz1 for buffering: */
enum { MAX_READ = (BUFSIZ/6) & ~0xf };
/* We recv into RECVBUF... (size: MAX_READ ~== BUFSIZ/6) */
#define RECVBUF  bb_common_bufsiz1
/* ...then copy to PARSEBUF, escaping control chars */
/* (can grow x2 max ~== BUFSIZ/3) */
#define PARSEBUF (bb_common_bufsiz1 + MAX_READ)
/* ...then sprintf into PRINTBUF, adding timestamp (15 chars),
 * host (64), fac.prio (20) to the message */
/* (growth by: 15 + 64 + 20 + delims = ~110) */
#define PRINTBUF (bb_common_bufsiz1 + 3*MAX_READ)
/* totals: BUFSIZ == BUFSIZ/6 + BUFSIZ/3 + (BUFSIZ/3+BUFSIZ/6)
 * -- we have BUFSIZ/6 extra at the ent of PRINTBUF
 * which covers needed ~110 extra bytes (and much more) */


/* Options */
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


/* circular buffer variables/structures */
#if ENABLE_FEATURE_IPC_SYSLOG


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
	int32_t size;   // size of data written
	int32_t head;   // start of message list
	int32_t tail;   // end of message list
	char data[1];   // data/messages
} *shbuf;               // shared memory pointer

static int shmid = -1;   // ipc shared memory id
static int s_semid = -1; // ipc semaphore id
static int shm_size = ((CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE)*1024);	// default shm size

static void ipcsyslog_cleanup(void)
{
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
	if (DEBUG)
		printf("shmget(%lx, %d,...)\n", KEY_ID, shm_size);

	shmid = shmget(KEY_ID, shm_size, IPC_CREAT | 1023);
	if (shmid == -1) {
		bb_perror_msg_and_die("shmget");
	}

	shbuf = shmat(shmid, NULL, 0);
	if (!shbuf) {
		bb_perror_msg_and_die("shmat");
	}

	shbuf->size = shm_size - offsetof(struct shbuf_ds, data);
	shbuf->head = shbuf->tail = 0;

	// we'll trust the OS to set initial semval to 0 (let's hope)
	s_semid = semget(KEY_ID, 2, IPC_CREAT | IPC_EXCL | 1023);
	if (s_semid == -1) {
		if (errno == EEXIST) {
			s_semid = semget(KEY_ID, 2, 0);
			if (s_semid != -1)
				return;
		}
		bb_perror_msg_and_die("semget");
	}
}

/* Write message to shared mem buffer */
static void log_to_shmem(const char *msg, int len)
{
	/* Why libc insists on these being rw? */
	static struct sembuf SMwup[1] = { {1, -1, IPC_NOWAIT} };
	static struct sembuf SMwdn[3] = { {0, 0}, {1, 0}, {1, +1} };

	int old_tail, new_tail;
	char *c;

	if (semop(s_semid, SMwdn, 3) == -1) {
		bb_perror_msg_and_die("SMwdn");
	}

	/* Circular Buffer Algorithm:
	 * --------------------------
	 * tail == position where to store next syslog message.
	 * head == position of next message to retrieve ("print").
	 * if head == tail, there is no "unprinted" messages left.
	 * head is typically advanced by separate "reader" program,
	 * but if there isn't one, we have to do it ourself.
	 * messages are NUL-separated.
	 */
	len++; /* length with NUL included */
 again:
	old_tail = shbuf->tail;
	new_tail = old_tail + len;
	if (new_tail < shbuf->size) {
		/* No need to move head if shbuf->head <= old_tail,
		 * else... */
		if (old_tail < shbuf->head && shbuf->head <= new_tail) {
			/* ...need to move head forward */
			c = memchr(shbuf->data + new_tail, '\0',
					   shbuf->size - new_tail);
			if (!c) /* no NUL ahead of us, wrap around */
				c = memchr(shbuf->data, '\0', old_tail);
			if (!c) { /* still nothing? point to this msg... */
				shbuf->head = old_tail;
			} else {
				/* convert pointer to offset + skip NUL */
				shbuf->head = c - shbuf->data + 1;
			}
		}
		/* store message, set new tail */
		memcpy(shbuf->data + old_tail, msg, len);
		shbuf->tail = new_tail;
	} else {
		/* we need to break up the message and wrap it around */
		/* k == available buffer space ahead of old tail */
		int k = shbuf->size - old_tail - 1;
		if (shbuf->head > old_tail) {
			/* we are going to overwrite head, need to
			 * move it out of the way */
			c = memchr(shbuf->data, '\0', old_tail);
			if (!c) { /* nothing? point to this msg... */
				shbuf->head = old_tail;
			} else { /* convert pointer to offset + skip NUL */
				shbuf->head = c - shbuf->data + 1;
			}
		}
		/* copy what fits to the end of buffer, and repeat */
		memcpy(shbuf->data + old_tail, msg, k);
		msg += k;
		len -= k;
		shbuf->tail = 0;
		goto again;
	}
	if (semop(s_semid, SMwup, 1) == -1) {
		bb_perror_msg_and_die("SMwup");
	}
	if (DEBUG)
		printf("head:%d tail:%d\n", shbuf->head, shbuf->tail);
}
#else
void ipcsyslog_cleanup(void);
void ipcsyslog_init(void);
void log_to_shmem(const char *msg);
#endif /* FEATURE_IPC_SYSLOG */


/* Print a message to the log file. */
static void log_locally(char *msg)
{
	static time_t last;
	struct flock fl;
	int len = strlen(msg);

#if ENABLE_FEATURE_IPC_SYSLOG
	if ((option_mask32 & OPT_circularlog) && shbuf) {
		log_to_shmem(msg, len);
		return;
	}
#endif
	if (logFD >= 0) {
		time_t cur;
		time(&cur);
		if (last != cur) {
			last = cur; /* reopen log file every second */
			close(logFD);
			goto reopen;
		}
	} else {
 reopen:
		logFD = device_open(logFilePath, O_WRONLY | O_CREAT
					| O_NOCTTY | O_APPEND | O_NONBLOCK);
		if (logFD < 0) {
			/* cannot open logfile? - print to /dev/console then */
			int fd = device_open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY | O_NONBLOCK);
			if (fd < 0)
				fd = 2; /* then stderr, dammit */
			full_write(fd, msg, len);
			if (fd != 2)
				close(fd);
			return;
		}
#if ENABLE_FEATURE_ROTATE_LOGFILE
		{
		struct stat statf;

		isRegular = (fstat(logFD, &statf) == 0 && (statf.st_mode & S_IFREG));
		/* bug (mostly harmless): can wrap around if file > 4gb */
		curFileSize = statf.st_size;
		}
#endif
	}

	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	fl.l_type = F_WRLCK;
	fcntl(logFD, F_SETLKW, &fl);

#if ENABLE_FEATURE_ROTATE_LOGFILE
	if (logFileSize && isRegular && curFileSize > logFileSize) {
		if (logFileRotate) { /* always 0..99 */
			int i = strlen(logFilePath) + 3 + 1;
			char oldFile[i];
			char newFile[i];
			i = logFileRotate - 1;
			/* rename: f.8 -> f.9; f.7 -> f.8; ... */
			while (1) {
				sprintf(newFile, "%s.%d", logFilePath, i);
				if (i == 0) break;
				sprintf(oldFile, "%s.%d", logFilePath, --i);
				rename(oldFile, newFile);
			}
			/* newFile == "f.0" now */
			rename(logFilePath, newFile);
			fl.l_type = F_UNLCK;
			fcntl(logFD, F_SETLKW, &fl);
			close(logFD);
			goto reopen;
		}
		ftruncate(logFD, 0);
	}
	curFileSize +=
#endif
	                full_write(logFD, msg, len);
	fl.l_type = F_UNLCK;
	fcntl(logFD, F_SETLKW, &fl);
}

static void parse_fac_prio_20(int pri, char *res20)
{
	CODE *c_pri, *c_fac;

	if (pri != 0) {
		c_fac = facilitynames;
		while (c_fac->c_name) {
			if (c_fac->c_val != (LOG_FAC(pri) << 3)) {
				c_fac++; continue;
			}
			/* facility is found, look for prio */
			c_pri = prioritynames;
			while (c_pri->c_name) {
				if (c_pri->c_val != LOG_PRI(pri)) {
					c_pri++; continue;
				}
				snprintf(res20, 20, "%s.%s",
						c_fac->c_name, c_pri->c_name);
				return;
			}
			/* prio not found, bail out */
			break;
		}
		snprintf(res20, 20, "<%d>", pri);
	}
}

/* len parameter is used only for "is there a timestamp?" check.
 * NB: some callers cheat and supply 0 when they know
 * that there is no timestamp, short-cutting the test. */
static void timestamp_and_log(int pri, char *msg, int len)
{
	char *timestamp;

	if (len < 16 || msg[3] != ' ' || msg[6] != ' '
	 || msg[9] != ':' || msg[12] != ':' || msg[15] != ' '
	) {
		time_t now;
		time(&now);
		timestamp = ctime(&now) + 4;
	} else {
		timestamp = msg;
		msg += 16;
	}
	timestamp[15] = '\0';

	/* Log message locally (to file or shared mem) */
	if (!ENABLE_FEATURE_REMOTE_LOG || (option_mask32 & OPT_locallog)) {
		if (LOG_PRI(pri) < logLevel) {
			if (option_mask32 & OPT_small)
				sprintf(PRINTBUF, "%s %s\n", timestamp, msg);
			else {
				char res[20];
				parse_fac_prio_20(pri, res);
				sprintf(PRINTBUF, "%s %s %s %s\n", timestamp, localHostName, res, msg);
			}
			log_locally(PRINTBUF);
		}
	}
}

static void split_escape_and_log(char *tmpbuf, int len)
{
	char *p = tmpbuf;

	tmpbuf += len;
	while (p < tmpbuf) {
		char c;
		char *q = PARSEBUF;
		int pri = (LOG_USER | LOG_NOTICE);

		if (*p == '<') {
			/* Parse the magic priority number */
			pri = bb_strtou(p + 1, &p, 10);
			if (*p == '>') p++;
			if (pri & ~(LOG_FACMASK | LOG_PRIMASK)) {
				pri = (LOG_USER | LOG_NOTICE);
			}
		}

		while ((c = *p++)) {
			if (c == '\n')
				c = ' ';
			if (!(c & ~0x1f)) {
				*q++ = '^';
				c += '@'; /* ^@, ^A, ^B... */
			}
			*q++ = c;
		}
		*q = '\0';
		/* Now log it */
		timestamp_and_log(pri, PARSEBUF, q - PARSEBUF);
	}
}

static void quit_signal(int sig)
{
	timestamp_and_log(LOG_SYSLOG | LOG_INFO, (char*)"syslogd exiting", 0);
	puts("syslogd exiting");
	if (ENABLE_FEATURE_IPC_SYSLOG)
		ipcsyslog_cleanup();
	exit(1);
}

#ifdef SYSLOGD_MARK
static void do_mark(int sig)
{
	if (markInterval) {
		timestamp_and_log(LOG_SYSLOG | LOG_INFO, (char*)"-- MARK --", 0);
		alarm(markInterval);
	}
}
#endif

static void do_syslogd(void) ATTRIBUTE_NORETURN;
static void do_syslogd(void)
{
	struct sockaddr_un sunx;
	int sock_fd;
	fd_set fds;
	char *dev_log_name;

	/* Set up signal handlers */
	signal(SIGINT, quit_signal);
	signal(SIGTERM, quit_signal);
	signal(SIGQUIT, quit_signal);
	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#ifdef SIGCLD
	signal(SIGCLD, SIG_IGN);
#endif
#ifdef SYSLOGD_MARK
	signal(SIGALRM, do_mark);
	alarm(markInterval);
#endif

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	strcpy(sunx.sun_path, "/dev/log");

	/* Unlink old /dev/log or object it points to. */
	/* (if it exists, bind will fail) */
	logmode = LOGMODE_NONE;
	dev_log_name = xmalloc_readlink_or_warn("/dev/log");
	logmode = LOGMODE_STDIO;
	if (dev_log_name) {
		int fd = xopen(".", O_NONBLOCK);
		xchdir("/dev");
		/* we do not check whether this is a link also */
		unlink(dev_log_name);
		fchdir(fd);
		close(fd);
		safe_strncpy(sunx.sun_path, dev_log_name, sizeof(sunx.sun_path));
		free(dev_log_name);
	} else {
		unlink("/dev/log");
	}

	sock_fd = xsocket(AF_UNIX, SOCK_DGRAM, 0);
	xbind(sock_fd, (struct sockaddr *) &sunx, sizeof(sunx));

	if (chmod("/dev/log", 0666) < 0) {
		bb_perror_msg_and_die("cannot set permission on /dev/log");
	}
	if (ENABLE_FEATURE_IPC_SYSLOG && (option_mask32 & OPT_circularlog)) {
		ipcsyslog_init();
	}

	timestamp_and_log(LOG_SYSLOG | LOG_INFO,
			(char*)"syslogd started: BusyBox v" BB_VER, 0);

	for (;;) {
		FD_ZERO(&fds);
		FD_SET(sock_fd, &fds);

		if (select(sock_fd + 1, &fds, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				/* alarm may have happened */
				continue;
			}
			bb_perror_msg_and_die("select");
		}

		if (FD_ISSET(sock_fd, &fds)) {
			int i;
			i = recv(sock_fd, RECVBUF, MAX_READ - 1, 0);
			if (i <= 0)
				bb_perror_msg_and_die("UNIX socket error");
			/* TODO: maybe suppress duplicates? */
#if ENABLE_FEATURE_REMOTE_LOG
			/* We are not modifying log messages in any way before send */
			/* Remote site cannot trust _us_ anyway and need to do validation again */
			if (remoteAddr) {
				if (-1 == remoteFD) {
					remoteFD = socket(remoteAddr->sa.sa_family, SOCK_DGRAM, 0);
				}
				if (-1 != remoteFD) {
					/* send message to remote logger, ignore possible error */
					sendto(remoteFD, RECVBUF, i, MSG_DONTWAIT,
						&remoteAddr->sa, remoteAddr->len);
				}
			}
#endif
			RECVBUF[i] = '\0';
			split_escape_and_log(RECVBUF, i);
		} /* FD_ISSET() */
	} /* for */
}

int syslogd_main(int argc, char **argv);
int syslogd_main(int argc, char **argv)
{
	char OPTION_DECL;
	char *p;

	/* do normal option parsing */
	opt_complementary = "=0"; /* no non-option params */
	getopt32(argc, argv, OPTION_STR, OPTION_PARAM);
#ifdef SYSLOGD_MARK
	if (option_mask32 & OPT_mark) // -m
		markInterval = xatou_range(opt_m, 0, INT_MAX/60) * 60;
#endif
	//if (option_mask32 & OPT_nofork) // -n
	//if (option_mask32 & OPT_outfile) // -O
	if (option_mask32 & OPT_loglevel) // -l
		logLevel = xatou_range(opt_l, 1, 8);
	//if (option_mask32 & OPT_small) // -S
#if ENABLE_FEATURE_ROTATE_LOGFILE
	if (option_mask32 & OPT_filesize) // -s
		logFileSize = xatou_range(opt_s, 0, INT_MAX/1024) * 1024;
	if (option_mask32 & OPT_rotatecnt) // -b
		logFileRotate = xatou_range(opt_b, 0, 99);
#endif
#if ENABLE_FEATURE_REMOTE_LOG
	if (option_mask32 & OPT_remotelog) { // -R
		remoteAddr = xhost2sockaddr(opt_R, 514);
	}
	//if (option_mask32 & OPT_locallog) // -L
#endif
#if ENABLE_FEATURE_IPC_SYSLOG
	if (opt_C) // -Cn
		shm_size = xatoul_range(opt_C, 4, INT_MAX/1024) * 1024;
#endif

	/* If they have not specified remote logging, then log locally */
	if (ENABLE_FEATURE_REMOTE_LOG && !(option_mask32 & OPT_remotelog))
		option_mask32 |= OPT_locallog;

	/* Store away localhost's name before the fork */
	gethostname(localHostName, sizeof(localHostName));
	p = strchr(localHostName, '.');
	if (p) {
		*p = '\0';
	}

	if (!(option_mask32 & OPT_nofork)) {
#ifdef BB_NOMMU
		vfork_daemon_rexec(0, 1, argc, argv, "-n");
#else
		bb_daemonize();
#endif
	}
	umask(0);
	do_syslogd();
	/* return EXIT_SUCCESS; */
}
