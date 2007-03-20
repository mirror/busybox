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

/* SYSLOG_NAMES defined to pull some extra junk from syslog.h: */
/* prioritynames[] and facilitynames[]. uclibc pulls those in _rwdata_! :( */

#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <sys/uio.h>

#if ENABLE_FEATURE_REMOTE_LOG
#include <netinet/in.h>
#endif

#if ENABLE_FEATURE_IPC_SYSLOG
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#endif


#define DEBUG 0

/* MARK code is not very useful, is bloat, and broken:
 * can deadlock if alarmed to make MARK while writing to IPC buffer
 * (semaphores are down but do_mark routine tries to down them again) */
#undef SYSLOGD_MARK

enum { MAX_READ = 256 };

/* Semaphore operation structures */
struct shbuf_ds {
	int32_t size;   /* size of data written */
	int32_t head;   /* start of message list */
	int32_t tail;   /* end of message list */
	char data[1];   /* data/messages */
};

/* Allows us to have smaller initializer. Ugly. */
#define GLOBALS \
	const char *logFilePath;                \
	int logFD;                              \
	/* interval between marks in seconds */ \
	/*int markInterval;*/                   \
	/* level of messages to be logged */    \
	int logLevel;                           \
USE_FEATURE_ROTATE_LOGFILE( \
	/* max size of file before rotation */  \
	unsigned logFileSize;                   \
	/* number of rotated message files */   \
	unsigned logFileRotate;                 \
	unsigned curFileSize;                   \
	smallint isRegular;                     \
) \
USE_FEATURE_REMOTE_LOG( \
	/* udp socket for remote logging */     \
	int remoteFD;                           \
	len_and_sockaddr* remoteAddr;           \
) \
USE_FEATURE_IPC_SYSLOG( \
	int shmid; /* ipc shared memory id */   \
	int s_semid; /* ipc semaphore id */     \
	int shm_size;                           \
	struct sembuf SMwup[1];                 \
	struct sembuf SMwdn[3];                 \
)

struct init_globals {
	GLOBALS
};

struct globals {
	GLOBALS
#if ENABLE_FEATURE_IPC_SYSLOG
	struct shbuf_ds *shbuf;
#endif
	time_t last_log_time;
	/* localhost's name */
	char localHostName[64];

	/* We recv into recvbuf... */
	char recvbuf[MAX_READ];
	/* ...then copy to parsebuf, escaping control chars */
	/* (can grow x2 max) */
	char parsebuf[MAX_READ*2];
	/* ...then sprintf into printbuf, adding timestamp (15 chars),
	 * host (64), fac.prio (20) to the message */
	/* (growth by: 15 + 64 + 20 + delims = ~110) */
	char printbuf[MAX_READ*2 + 128];
};

static const struct init_globals init_data = {
	.logFilePath = "/var/log/messages",
	.logFD = -1,
#ifdef SYSLOGD_MARK
	.markInterval = 20 * 60,
#endif
	.logLevel = 8,
#if ENABLE_FEATURE_ROTATE_LOGFILE
	.logFileSize = 200 * 1024,
	.logFileRotate = 1,
#endif
#if ENABLE_FEATURE_REMOTE_LOG
	.remoteFD = -1,
#endif
#if ENABLE_FEATURE_IPC_SYSLOG
	.shmid = -1,
	.s_semid = -1,
	.shm_size = ((CONFIG_FEATURE_IPC_SYSLOG_BUFFER_SIZE)*1024), // default shm size
	.SMwup = { {1, -1, IPC_NOWAIT} },
	.SMwdn = { {0, 0}, {1, 0}, {1, +1} },
#endif
};

#define G (*ptr_to_globals)


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
#define OPTION_PARAM &opt_m, &G.logFilePath, &opt_l \
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

/* our shared key */
#define KEY_ID ((long)0x414e4547) /* "GENA" */

static void ipcsyslog_cleanup(void)
{
	if (G.shmid != -1) {
		shmdt(G.shbuf);
	}
	if (G.shmid != -1) {
		shmctl(G.shmid, IPC_RMID, NULL);
	}
	if (G.s_semid != -1) {
		semctl(G.s_semid, 0, IPC_RMID, 0);
	}
}

static void ipcsyslog_init(void)
{
	if (DEBUG)
		printf("shmget(%lx, %d,...)\n", KEY_ID, G.shm_size);

	G.shmid = shmget(KEY_ID, G.shm_size, IPC_CREAT | 1023);
	if (G.shmid == -1) {
		bb_perror_msg_and_die("shmget");
	}

	G.shbuf = shmat(G.shmid, NULL, 0);
	if (!G.shbuf) {
		bb_perror_msg_and_die("shmat");
	}

	G.shbuf->size = G.shm_size - offsetof(struct shbuf_ds, data);
	G.shbuf->head = G.shbuf->tail = 0;

	// we'll trust the OS to set initial semval to 0 (let's hope)
	G.s_semid = semget(KEY_ID, 2, IPC_CREAT | IPC_EXCL | 1023);
	if (G.s_semid == -1) {
		if (errno == EEXIST) {
			G.s_semid = semget(KEY_ID, 2, 0);
			if (G.s_semid != -1)
				return;
		}
		bb_perror_msg_and_die("semget");
	}
}

/* Write message to shared mem buffer */
static void log_to_shmem(const char *msg, int len)
{
	int old_tail, new_tail;
	char *c;

	if (semop(G.s_semid, G.SMwdn, 3) == -1) {
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
	old_tail = G.shbuf->tail;
	new_tail = old_tail + len;
	if (new_tail < G.shbuf->size) {
		/* No need to move head if shbuf->head <= old_tail,
		 * else... */
		if (old_tail < G.shbuf->head && G.shbuf->head <= new_tail) {
			/* ...need to move head forward */
			c = memchr(G.shbuf->data + new_tail, '\0',
					   G.shbuf->size - new_tail);
			if (!c) /* no NUL ahead of us, wrap around */
				c = memchr(G.shbuf->data, '\0', old_tail);
			if (!c) { /* still nothing? point to this msg... */
				G.shbuf->head = old_tail;
			} else {
				/* convert pointer to offset + skip NUL */
				G.shbuf->head = c - G.shbuf->data + 1;
			}
		}
		/* store message, set new tail */
		memcpy(G.shbuf->data + old_tail, msg, len);
		G.shbuf->tail = new_tail;
	} else {
		/* we need to break up the message and wrap it around */
		/* k == available buffer space ahead of old tail */
		int k = G.shbuf->size - old_tail - 1;
		if (G.shbuf->head > old_tail) {
			/* we are going to overwrite head, need to
			 * move it out of the way */
			c = memchr(G.shbuf->data, '\0', old_tail);
			if (!c) { /* nothing? point to this msg... */
				G.shbuf->head = old_tail;
			} else { /* convert pointer to offset + skip NUL */
				G.shbuf->head = c - G.shbuf->data + 1;
			}
		}
		/* copy what fits to the end of buffer, and repeat */
		memcpy(G.shbuf->data + old_tail, msg, k);
		msg += k;
		len -= k;
		G.shbuf->tail = 0;
		goto again;
	}
	if (semop(G.s_semid, G.SMwup, 1) == -1) {
		bb_perror_msg_and_die("SMwup");
	}
	if (DEBUG)
		printf("head:%d tail:%d\n", G.shbuf->head, G.shbuf->tail);
}
#else
void ipcsyslog_cleanup(void);
void ipcsyslog_init(void);
void log_to_shmem(const char *msg);
#endif /* FEATURE_IPC_SYSLOG */


/* Print a message to the log file. */
static void log_locally(char *msg)
{
	struct flock fl;
	int len = strlen(msg);

#if ENABLE_FEATURE_IPC_SYSLOG
	if ((option_mask32 & OPT_circularlog) && G.shbuf) {
		log_to_shmem(msg, len);
		return;
	}
#endif
	if (G.logFD >= 0) {
		time_t cur;
		time(&cur);
		if (G.last_log_time != cur) {
			G.last_log_time = cur; /* reopen log file every second */
			close(G.logFD);
			goto reopen;
		}
	} else {
 reopen:
		G.logFD = device_open(G.logFilePath, O_WRONLY | O_CREAT
					| O_NOCTTY | O_APPEND | O_NONBLOCK);
		if (G.logFD < 0) {
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

		G.isRegular = (fstat(G.logFD, &statf) == 0 && (statf.st_mode & S_IFREG));
		/* bug (mostly harmless): can wrap around if file > 4gb */
		G.curFileSize = statf.st_size;
		}
#endif
	}

	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	fl.l_type = F_WRLCK;
	fcntl(G.logFD, F_SETLKW, &fl);

#if ENABLE_FEATURE_ROTATE_LOGFILE
	if (G.logFileSize && G.isRegular && G.curFileSize > G.logFileSize) {
		if (G.logFileRotate) { /* always 0..99 */
			int i = strlen(G.logFilePath) + 3 + 1;
			char oldFile[i];
			char newFile[i];
			i = G.logFileRotate - 1;
			/* rename: f.8 -> f.9; f.7 -> f.8; ... */
			while (1) {
				sprintf(newFile, "%s.%d", G.logFilePath, i);
				if (i == 0) break;
				sprintf(oldFile, "%s.%d", G.logFilePath, --i);
				rename(oldFile, newFile);
			}
			/* newFile == "f.0" now */
			rename(G.logFilePath, newFile);
			fl.l_type = F_UNLCK;
			fcntl(G.logFD, F_SETLKW, &fl);
			close(G.logFD);
			goto reopen;
		}
		ftruncate(G.logFD, 0);
	}
	G.curFileSize +=
#endif
	                full_write(G.logFD, msg, len);
	fl.l_type = F_UNLCK;
	fcntl(G.logFD, F_SETLKW, &fl);
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
		if (LOG_PRI(pri) < G.logLevel) {
			if (option_mask32 & OPT_small)
				sprintf(G.printbuf, "%s %s\n", timestamp, msg);
			else {
				char res[20];
				parse_fac_prio_20(pri, res);
				sprintf(G.printbuf, "%s %s %s %s\n", timestamp, G.localHostName, res, msg);
			}
			log_locally(G.printbuf);
		}
	}
}

static void split_escape_and_log(char *tmpbuf, int len)
{
	char *p = tmpbuf;

	tmpbuf += len;
	while (p < tmpbuf) {
		char c;
		char *q = G.parsebuf;
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
		timestamp_and_log(pri, G.parsebuf, q - G.parsebuf);
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
	if (G.markInterval) {
		timestamp_and_log(LOG_SYSLOG | LOG_INFO, (char*)"-- MARK --", 0);
		alarm(G.markInterval);
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
	alarm(G.markInterval);
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
			i = recv(sock_fd, G.recvbuf, MAX_READ - 1, 0);
			if (i <= 0)
				bb_perror_msg_and_die("UNIX socket error");
			/* TODO: maybe suppress duplicates? */
#if ENABLE_FEATURE_REMOTE_LOG
			/* We are not modifying log messages in any way before send */
			/* Remote site cannot trust _us_ anyway and need to do validation again */
			if (G.remoteAddr) {
				if (-1 == G.remoteFD) {
					G.remoteFD = socket(G.remoteAddr->sa.sa_family, SOCK_DGRAM, 0);
				}
				if (-1 != G.remoteFD) {
					/* send message to remote logger, ignore possible error */
					sendto(G.remoteFD, G.recvbuf, i, MSG_DONTWAIT,
						&G.remoteAddr->sa, G.remoteAddr->len);
				}
			}
#endif
			G.recvbuf[i] = '\0';
			split_escape_and_log(G.recvbuf, i);
		} /* FD_ISSET() */
	} /* for */
}

int syslogd_main(int argc, char **argv);
int syslogd_main(int argc, char **argv)
{
	char OPTION_DECL;
	char *p;

	PTR_TO_GLOBALS = memcpy(xzalloc(sizeof(G)), &init_data, sizeof(init_data));

	/* do normal option parsing */
	opt_complementary = "=0"; /* no non-option params */
	getopt32(argc, argv, OPTION_STR, OPTION_PARAM);
#ifdef SYSLOGD_MARK
	if (option_mask32 & OPT_mark) // -m
		G.markInterval = xatou_range(opt_m, 0, INT_MAX/60) * 60;
#endif
	//if (option_mask32 & OPT_nofork) // -n
	//if (option_mask32 & OPT_outfile) // -O
	if (option_mask32 & OPT_loglevel) // -l
		G.logLevel = xatou_range(opt_l, 1, 8);
	//if (option_mask32 & OPT_small) // -S
#if ENABLE_FEATURE_ROTATE_LOGFILE
	if (option_mask32 & OPT_filesize) // -s
		G.logFileSize = xatou_range(opt_s, 0, INT_MAX/1024) * 1024;
	if (option_mask32 & OPT_rotatecnt) // -b
		G.logFileRotate = xatou_range(opt_b, 0, 99);
#endif
#if ENABLE_FEATURE_REMOTE_LOG
	if (option_mask32 & OPT_remotelog) { // -R
		G.remoteAddr = xhost2sockaddr(opt_R, 514);
	}
	//if (option_mask32 & OPT_locallog) // -L
#endif
#if ENABLE_FEATURE_IPC_SYSLOG
	if (opt_C) // -Cn
		G.shm_size = xatoul_range(opt_C, 4, INT_MAX/1024) * 1024;
#endif

	/* If they have not specified remote logging, then log locally */
	if (ENABLE_FEATURE_REMOTE_LOG && !(option_mask32 & OPT_remotelog))
		option_mask32 |= OPT_locallog;

	/* Store away localhost's name before the fork */
	gethostname(G.localHostName, sizeof(G.localHostName));
	p = strchr(G.localHostName, '.');
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
