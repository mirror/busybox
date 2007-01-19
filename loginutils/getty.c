/* vi: set sw=4 ts=4: */
/* agetty.c - another getty program for Linux. By W. Z. Venema 1989
 * Ported to Linux by Peter Orbaek <poe@daimi.aau.dk>
 * This program is freely distributable. The entire man-page used to
 * be here. Now read the real man-page agetty.8 instead.
 *
 * option added by Eric Rasmussen <ear@usfirst.org> - 12/28/95
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@misiek.eu.org>
 * - added Native Language Support

 * 1999-05-05 Thorsten Kranzkowski <dl8bcu@gmx.net>
 * - enable hardware flow control before displaying /etc/issue
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include "busybox.h"
#include <syslog.h>

#if ENABLE_FEATURE_UTMP
#include <utmp.h>
#endif

/*
 * Some heuristics to find out what environment we are in: if it is not
 * System V, assume it is SunOS 4.
 */
#ifdef LOGIN_PROCESS                    /* defined in System V utmp.h */
#define SYSV_STYLE                      /* select System V style getty */
#include <sys/utsname.h>
#include <time.h>
#if ENABLE_FEATURE_WTMP
extern void updwtmp(const char *filename, const struct utmp *ut);
static void update_utmp(char *line);
#endif
#endif  /* LOGIN_PROCESS */

/*
 * Things you may want to modify.
 *
 * You may disagree with the default line-editing etc. characters defined
 * below. Note, however, that DEL cannot be used for interrupt generation
 * and for line editing at the same time.
 */

/* I doubt there are systems which still need this */
#undef HANDLE_ALLCAPS

#define _PATH_LOGIN "/bin/login"

/* If ISSUE is not defined, getty will never display the contents of the
 * /etc/issue file. You will not want to spit out large "issue" files at the
 * wrong baud rate.
 */
#define ISSUE "/etc/issue"              /* displayed before the login prompt */

/* Some shorthands for control characters. */
#define CTL(x)          (x ^ 0100)      /* Assumes ASCII dialect */
#define CR              CTL('M')        /* carriage return */
#define NL              CTL('J')        /* line feed */
#define BS              CTL('H')        /* back space */
#define DEL             CTL('?')        /* delete */

/* Defaults for line-editing etc. characters; you may want to change this. */
#define DEF_ERASE       DEL             /* default erase character */
#define DEF_INTR        CTL('C')        /* default interrupt character */
#define DEF_QUIT        CTL('\\')       /* default quit char */
#define DEF_KILL        CTL('U')        /* default kill char */
#define DEF_EOF         CTL('D')        /* default EOF char */
#define DEF_EOL         '\n'
#define DEF_SWITCH      0               /* default switch char */

/*
 * When multiple baud rates are specified on the command line, the first one
 * we will try is the first one specified.
 */
#define FIRST_SPEED     0

/* Storage for command-line options. */

#define MAX_SPEED       10              /* max. nr. of baud rates */

struct options {
	int flags;                      /* toggle switches, see below */
	unsigned timeout;               /* time-out period */
	char *login;                    /* login program */
	char *tty;                      /* name of tty */
	char *initstring;               /* modem init string */
	char *issue;                    /* alternative issue file */
	int numspeed;                   /* number of baud rates to try */
	int speeds[MAX_SPEED];          /* baud rates to be tried */
};

static const char opt_string[] = "I:LH:f:hil:mt:wn";
#define F_INITSTRING    (1<<0)          /* initstring is set */
#define F_LOCAL         (1<<1)          /* force local */
#define F_FAKEHOST      (1<<2)          /* force fakehost */
#define F_CUSTISSUE     (1<<3)          /* give alternative issue file */
#define F_RTSCTS        (1<<4)          /* enable RTS/CTS flow control */
#define F_ISSUE         (1<<5)          /* display /etc/issue */
#define F_LOGIN         (1<<6)          /* non-default login program */
#define F_PARSE         (1<<7)          /* process modem status messages */
#define F_TIMEOUT       (1<<8)          /* time out */
#define F_WAITCRLF      (1<<9)          /* wait for CR or LF */
#define F_NOPROMPT      (1<<10)         /* don't ask for login name! */

/* Storage for things detected while the login name was read. */
struct chardata {
	unsigned char erase;    /* erase character */
	unsigned char kill;     /* kill character */
	unsigned char eol;      /* end-of-line character */
	unsigned char parity;   /* what parity did we see */
#ifdef HANDLE_ALLCAPS
	unsigned char capslock; /* upper case without lower case */
#endif
};

/* Initial values for the above. */
static const struct chardata init_chardata = {
	DEF_ERASE,                              /* default erase character */
	DEF_KILL,                               /* default kill character */
	13,                                     /* default eol char */
	0,                                      /* space parity */
#ifdef HANDLE_ALLCAPS
	0,                                      /* no capslock */
#endif
};

/* The following is used for understandable diagnostics. */

/* Fake hostname for ut_host specified on command line. */
static char *fakehost = NULL;

/* ... */
#ifdef DEBUGGING
#define debug(s) fprintf(dbf,s); fflush(dbf)
#define DEBUGTERM "/dev/ttyp0"
static FILE *dbf;
#else
#define debug(s) /* nothing */
#endif


/* bcode - convert speed string to speed code; return 0 on failure */
static int bcode(const char *s)
{
	int r;
	unsigned value = bb_strtou(s, NULL, 10);
	if (errno) {
		return -1;
	}
	r = tty_value_to_baud(value);
	if (r > 0) {
		return r;
	}
	return 0;
}


/* parse_speeds - parse alternate baud rates */
static void parse_speeds(struct options *op, char *arg)
{
	char *cp;

	debug("entered parse_speeds\n");
	for (cp = strtok(arg, ","); cp != 0; cp = strtok((char *) 0, ",")) {
		if ((op->speeds[op->numspeed++] = bcode(cp)) <= 0)
			bb_error_msg_and_die("bad speed: %s", cp);
		if (op->numspeed > MAX_SPEED)
			bb_error_msg_and_die("too many alternate speeds");
	}
	debug("exiting parsespeeds\n");
}


/* parse_args - parse command-line arguments */
static void parse_args(int argc, char **argv, struct options *op)
{
	char *ts;

	op->flags = getopt32(argc, argv, opt_string,
		&(op->initstring), &fakehost, &(op->issue),
		&(op->login), &ts);
	if (op->flags & F_INITSTRING) {
		const char *p = op->initstring;
		char *q;

		q = op->initstring = xstrdup(op->initstring);
		/* copy optarg into op->initstring decoding \ddd
		   octal codes into chars */
		while (*p) {
			if (*p == '\\') {
				p++;
				*q++ = bb_process_escape_sequence(&p);
			} else {
				*q++ = *p++;
			}
		}
		*q = '\0';
	}
	op->flags ^= F_ISSUE;           /* revert flag show /etc/issue */
	if (op->flags & F_TIMEOUT) {
		op->timeout = xatoul_range(ts, 1, INT_MAX);
	}
	argv += optind;
	argc -= optind;
	debug("after getopt loop\n");
	if (argc < 2)          /* check parameter count */
		bb_show_usage();

	/* we loosen up a bit and accept both "baudrate tty" and "tty baudrate" */
	if (isdigit(argv[0][0])) {
		/* a number first, assume it's a speed (BSD style) */
		parse_speeds(op, argv[0]);       /* baud rate(s) */
		op->tty = argv[1]; /* tty name */
	} else {
		op->tty = argv[0];       /* tty name */
		parse_speeds(op, argv[1]); /* baud rate(s) */
	}

	if (argv[2])
		setenv("TERM", argv[2], 1);

	debug("exiting parseargs\n");
}

static void xdup2(int srcfd, int dstfd, const char *tty)
{
	if (dup2(srcfd, dstfd) == -1)
		bb_perror_msg_and_die("%s: dup", tty);
}

/* open_tty - set up tty as standard { input, output, error } */
static void open_tty(char *tty, struct termios *tp, int local)
{
	int chdir_to_root = 0;

	/* Set up new standard input, unless we are given an already opened port. */

	if (NOT_LONE_DASH(tty)) {
		struct stat st;
		int fd;

		/* Sanity checks... */

		xchdir("/dev");
		chdir_to_root = 1;
		xstat(tty, &st);
		if ((st.st_mode & S_IFMT) != S_IFCHR)
			bb_error_msg_and_die("%s: not a character device", tty);

		/* Open the tty as standard input. */

		debug("open(2)\n");
		fd = xopen(tty, O_RDWR | O_NONBLOCK);
		xdup2(fd, 0, tty);
		while (fd > 2) close(fd--);
	} else {
		/*
		 * Standard input should already be connected to an open port. Make
		 * sure it is open for read/write.
		 */

		if ((fcntl(0, F_GETFL, 0) & O_RDWR) != O_RDWR)
			bb_error_msg_and_die("%s: not open for read/write", tty);
	}

	/* Replace current standard output/error fd's with new ones */
	debug("duping\n");
	xdup2(0, 1, tty);
	xdup2(0, 2, tty);

	/*
	 * The following ioctl will fail if stdin is not a tty, but also when
	 * there is noise on the modem control lines. In the latter case, the
	 * common course of action is (1) fix your cables (2) give the modem more
	 * time to properly reset after hanging up. SunOS users can achieve (2)
	 * by patching the SunOS kernel variable "zsadtrlow" to a larger value;
	 * 5 seconds seems to be a good value.
	 */

	if (ioctl(0, TCGETS, tp) < 0)
		bb_perror_msg_and_die("%s: ioctl(TCGETS)", tty);

	/*
	 * It seems to be a terminal. Set proper protections and ownership. Mode
	 * 0622 is suitable for SYSV <4 because /bin/login does not change
	 * protections. SunOS 4 login will change the protections to 0620 (write
	 * access for group tty) after the login has succeeded.
	 */

#ifdef DEBIAN
#warning Debian /dev/vcs[a]NN hack is deprecated and will be removed
	{
		/* tty to root.dialout 660 */
		struct group *gr;
		int id;

		gr = getgrnam("dialout");
		id = gr ? gr->gr_gid : 0;
		chown(tty, 0, id);
		chmod(tty, 0660);

		/* vcs,vcsa to root.sys 600 */
		if (!strncmp(tty, "tty", 3) && isdigit(tty[3])) {
			char *vcs, *vcsa;

			vcs = xstrdup(tty);
			vcsa = xmalloc(strlen(tty) + 2);
			strcpy(vcs, "vcs");
			strcpy(vcs + 3, tty + 3);
			strcpy(vcsa, "vcsa");
			strcpy(vcsa + 4, tty + 3);

			id = (gr = getgrnam("sys")) ? gr->gr_gid : 0;
			chown(vcs, 0, id);
			chmod(vcs, 0600);
			chown(vcsa, 0, id);
			chmod(vcs, 0600);

			free(vcs);
			free(vcsa);
		}
	}
#else
	if (NOT_LONE_DASH(tty)) {
		chown(tty, 0, 0);        /* 0:0 */
		chmod(tty, 0622);        /* crw--w--w- */
	}
#endif
	if (chdir_to_root)
		xchdir("/");
}

/* termios_init - initialize termios settings */
static void termios_init(struct termios *tp, int speed, struct options *op)
{
	/*
	 * Initial termios settings: 8-bit characters, raw-mode, blocking i/o.
	 * Special characters are set after we have read the login name; all
	 * reads will be done in raw mode anyway. Errors will be dealt with
	 * later on.
	 */
#ifdef __linux__
	/* flush input and output queues, important for modems! */
	ioctl(0, TCFLSH, TCIOFLUSH);
#endif

	tp->c_cflag = CS8 | HUPCL | CREAD | speed;
	if (op->flags & F_LOCAL) {
		tp->c_cflag |= CLOCAL;
	}

	tp->c_iflag = tp->c_lflag = tp->c_line = 0;
	tp->c_oflag = OPOST | ONLCR;
	tp->c_cc[VMIN] = 1;
	tp->c_cc[VTIME] = 0;

	/* Optionally enable hardware flow control */

#ifdef  CRTSCTS
	if (op->flags & F_RTSCTS)
		tp->c_cflag |= CRTSCTS;
#endif

	ioctl(0, TCSETS, tp);

	/* go to blocking input even in local mode */
	fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~O_NONBLOCK);

	debug("term_io 2\n");
}

/* auto_baud - extract baud rate from modem status message */
static void auto_baud(char *buf, unsigned size_buf, struct termios *tp)
{
	int speed;
	int vmin;
	unsigned iflag;
	char *bp;
	int nread;

	/*
	 * This works only if the modem produces its status code AFTER raising
	 * the DCD line, and if the computer is fast enough to set the proper
	 * baud rate before the message has gone by. We expect a message of the
	 * following format:
	 *
	 * <junk><number><junk>
	 *
	 * The number is interpreted as the baud rate of the incoming call. If the
	 * modem does not tell us the baud rate within one second, we will keep
	 * using the current baud rate. It is advisable to enable BREAK
	 * processing (comma-separated list of baud rates) if the processing of
	 * modem status messages is enabled.
	 */

	/*
	 * Use 7-bit characters, don't block if input queue is empty. Errors will
	 * be dealt with later on.
	 */

	iflag = tp->c_iflag;
	tp->c_iflag |= ISTRIP;          /* enable 8th-bit stripping */
	vmin = tp->c_cc[VMIN];
	tp->c_cc[VMIN] = 0;                     /* don't block if queue empty */
	ioctl(0, TCSETS, tp);

	/*
	 * Wait for a while, then read everything the modem has said so far and
	 * try to extract the speed of the dial-in call.
	 */

	sleep(1);
	nread = read(0, buf, size_buf - 1);
	if (nread > 0) {
		buf[nread] = '\0';
		for (bp = buf; bp < buf + nread; bp++) {
			if (isascii(*bp) && isdigit(*bp)) {
				speed = bcode(bp);
				if (speed) {
					tp->c_cflag &= ~CBAUD;
					tp->c_cflag |= speed;
				}
				break;
			}
		}
	}
	/* Restore terminal settings. Errors will be dealt with later on. */

	tp->c_iflag = iflag;
	tp->c_cc[VMIN] = vmin;
	ioctl(0, TCSETS, tp);
}

/* next_speed - select next baud rate */
static void next_speed(struct termios *tp, struct options *op)
{
	static int baud_index = FIRST_SPEED;    /* current speed index */

	baud_index = (baud_index + 1) % op->numspeed;
	tp->c_cflag &= ~CBAUD;
	tp->c_cflag |= op->speeds[baud_index];
	ioctl(0, TCSETS, tp);
}


/* do_prompt - show login prompt, optionally preceded by /etc/issue contents */
static void do_prompt(struct options *op, struct termios *tp)
{
#ifdef ISSUE
	print_login_issue(op->issue, op->tty);
#endif
	print_login_prompt();
}

#ifdef HANDLE_ALLCAPS
/* caps_lock - string contains upper case without lower case */
/* returns 1 if true, 0 if false */
static int caps_lock(const char *s)
{
	while (*s)
		if (islower(*s++))
			return 0;
	return 1;
}
#endif

/* get_logname - get user name, establish parity, speed, erase, kill, eol */
/* return NULL on failure, logname on success */
static char *get_logname(char *logname, unsigned size_logname,
		struct options *op, struct chardata *cp, struct termios *tp)
{
	char *bp;
	char c;				/* input character, full eight bits */
	char ascval;                    /* low 7 bits of input character */
	int bits;                       /* # of "1" bits per character */
	int mask;                       /* mask with 1 bit up */
	static const char erase[][3] = {    /* backspace-space-backspace */
		"\010\040\010",                 /* space parity */
		"\010\040\010",                 /* odd parity */
		"\210\240\210",                 /* even parity */
		"\210\240\210",                 /* no parity */
	};

	/* Initialize kill, erase, parity etc. (also after switching speeds). */

	*cp = init_chardata;

	/* Flush pending input (esp. after parsing or switching the baud rate). */

	sleep(1);
	ioctl(0, TCFLSH, TCIFLUSH);

	/* Prompt for and read a login name. */

	logname[0] = '\0';
	while (!logname[0]) {

		/* Write issue file and prompt, with "parity" bit == 0. */

		do_prompt(op, tp);

		/* Read name, watch for break, parity, erase, kill, end-of-line. */

		bp = logname;
		cp->eol = '\0';
		while (cp->eol == '\0') {

			/* Do not report trivial EINTR/EIO errors. */
			if (read(0, &c, 1) < 1) {
				if (errno == EINTR || errno == EIO)
					exit(0);
				bb_perror_msg_and_die("%s: read", op->tty);
			}

			/* Do BREAK handling elsewhere. */
			if (c == '\0' && op->numspeed > 1)
				return NULL;

			/* Do parity bit handling. */
			ascval = c & 0177;
			if (c != ascval) {       /* "parity" bit on ? */
				bits = 1;
				mask = 1;
				while (mask & 0177) {
					if (mask & ascval)
						bits++; /* count "1" bits */
					mask <<= 1;
				}
				/* ... |= 2 - even, 1 - odd */
				cp->parity |= 2 - (bits & 1);
			}

			/* Do erase, kill and end-of-line processing. */
			switch (ascval) {
			case CR:
			case NL:
				*bp = '\0';             /* terminate logname */
				cp->eol = ascval;       /* set end-of-line char */
				break;
			case BS:
			case DEL:
			case '#':
				cp->erase = ascval;     /* set erase character */
				if (bp > logname) {
					write(1, erase[cp->parity], 3);
					bp--;
				}
				break;
			case CTL('U'):
			case '@':
				cp->kill = ascval;      /* set kill character */
				while (bp > logname) {
					write(1, erase[cp->parity], 3);
					bp--;
				}
				break;
			case CTL('D'):
				exit(0);
			default:
				if (!isascii(ascval) || !isprint(ascval)) {
					/* ignore garbage characters */
				} else if (bp - logname >= size_logname - 1) {
					bb_error_msg_and_die("%s: input overrun", op->tty);
				} else {
					write(1, &c, 1); /* echo the character */
					*bp++ = ascval; /* and store it */
				}
				break;
			}
		}
	}
	/* Handle names with upper case and no lower case. */

#ifdef HANDLE_ALLCAPS
	cp->capslock = caps_lock(logname);
	if (cp->capslock) {
		for (bp = logname; *bp; bp++)
			if (isupper(*bp))
				*bp = tolower(*bp);     /* map name to lower case */
	}
#endif
	return logname;
}

/* termios_final - set the final tty mode bits */
static void termios_final(struct options *op, struct termios *tp, struct chardata *cp)
{
	/* General terminal-independent stuff. */

	tp->c_iflag |= IXON | IXOFF;    /* 2-way flow control */
	tp->c_lflag |= ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHOKE;
	/* no longer| ECHOCTL | ECHOPRT */
	tp->c_oflag |= OPOST;
	/* tp->c_cflag = 0; */
	tp->c_cc[VINTR] = DEF_INTR;     /* default interrupt */
	tp->c_cc[VQUIT] = DEF_QUIT;     /* default quit */
	tp->c_cc[VEOF] = DEF_EOF;       /* default EOF character */
	tp->c_cc[VEOL] = DEF_EOL;
	tp->c_cc[VSWTC] = DEF_SWITCH;   /* default switch character */

	/* Account for special characters seen in input. */

	if (cp->eol == CR) {
		tp->c_iflag |= ICRNL;   /* map CR in input to NL */
		tp->c_oflag |= ONLCR;   /* map NL in output to CR-NL */
	}
	tp->c_cc[VERASE] = cp->erase;   /* set erase character */
	tp->c_cc[VKILL] = cp->kill;     /* set kill character */

	/* Account for the presence or absence of parity bits in input. */

	switch (cp->parity) {
	case 0:                                 /* space (always 0) parity */
		break;
	case 1:                                 /* odd parity */
		tp->c_cflag |= PARODD;
		/* FALLTHROUGH */
	case 2:                                 /* even parity */
		tp->c_cflag |= PARENB;
		tp->c_iflag |= INPCK | ISTRIP;
		/* FALLTHROUGH */
	case (1 | 2):                           /* no parity bit */
		tp->c_cflag &= ~CSIZE;
		tp->c_cflag |= CS7;
		break;
	}
	/* Account for upper case without lower case. */

#ifdef HANDLE_ALLCAPS
	if (cp->capslock) {
		tp->c_iflag |= IUCLC;
		tp->c_lflag |= XCASE;
		tp->c_oflag |= OLCUC;
	}
#endif
	/* Optionally enable hardware flow control */

#ifdef  CRTSCTS
	if (op->flags & F_RTSCTS)
		tp->c_cflag |= CRTSCTS;
#endif

	/* Finally, make the new settings effective */

	if (ioctl(0, TCSETS, tp) < 0)
		bb_perror_msg_and_die("%s: ioctl(TCSETS)", op->tty);
}


#ifdef SYSV_STYLE
#if ENABLE_FEATURE_UTMP
/* update_utmp - update our utmp entry */
static void update_utmp(char *line)
{
	struct utmp ut;
	struct utmp *utp;
	time_t t;
	int mypid = getpid();

	/*
	 * The utmp file holds miscellaneous information about things started by
	 * /sbin/init and other system-related events. Our purpose is to update
	 * the utmp entry for the current process, in particular the process type
	 * and the tty line we are listening to. Return successfully only if the
	 * utmp file can be opened for update, and if we are able to find our
	 * entry in the utmp file.
	 */
	if (access(_PATH_UTMP, R_OK|W_OK) == -1) {
		close(creat(_PATH_UTMP, 0664));
	}
	utmpname(_PATH_UTMP);
	setutent();
	while ((utp = getutent())
		   && !(utp->ut_type == INIT_PROCESS && utp->ut_pid == mypid))
		/* nothing */;

	if (utp) {
		memcpy(&ut, utp, sizeof(ut));
	} else {
		/* some inits don't initialize utmp... */
		memset(&ut, 0, sizeof(ut));
		safe_strncpy(ut.ut_id, line + 3, sizeof(ut.ut_id));
	}
	/* endutent(); */

	strcpy(ut.ut_user, "LOGIN");
	safe_strncpy(ut.ut_line, line, sizeof(ut.ut_line));
	if (fakehost)
		safe_strncpy(ut.ut_host, fakehost, sizeof(ut.ut_host));
	time(&t);
	ut.ut_time = t;
	ut.ut_type = LOGIN_PROCESS;
	ut.ut_pid = mypid;

	pututline(&ut);
	endutent();

#if ENABLE_FEATURE_WTMP
	if (access(bb_path_wtmp_file, R_OK|W_OK) == -1)
		close(creat(bb_path_wtmp_file, 0664));
	updwtmp(bb_path_wtmp_file, &ut);
#endif
}

#endif /* CONFIG_FEATURE_UTMP */
#endif /* SYSV_STYLE */


int getty_main(int argc, char **argv)
{
	int nullfd;
	char *logname = NULL;           /* login name, given to /bin/login */
	/* Merging these into "struct local" may _seem_ to reduce
	 * parameter passing, but today's gcc will inline
	 * statics which are called once anyway, so don't do that */
	struct chardata chardata;       /* set by get_logname() */
	struct termios termios;           /* terminal mode bits */
	struct options options = {
		0,                      /* show /etc/issue (SYSV_STYLE) */
		0,                      /* no timeout */
		_PATH_LOGIN,            /* default login program */
		"tty1",                 /* default tty line */
		"",                     /* modem init string */
#ifdef ISSUE
		ISSUE,                  /* default issue file */
#else
		NULL,
#endif
		0,                      /* no baud rates known yet */
	};

	/* Already too late because of theoretical
	 * possibility of getty --help somehow triggered
	 * inadvertently before we reach this. Oh well. */
	logmode = LOGMODE_NONE;
	setsid();
	nullfd = xopen(bb_dev_null, O_RDWR);
	/* dup2(nullfd, 0); - no, because of possible "getty - 9600" */
	/* open_tty() will take care of fd# 0 anyway */
	dup2(nullfd, 1);
	dup2(nullfd, 2);
	while (nullfd > 2) close(nullfd--);
	/* We want special flavor of error_msg_and_die */
	die_sleep = 10;
	msg_eol = "\r\n";
	openlog(applet_name, LOG_PID, LOG_AUTH);
	logmode = LOGMODE_BOTH;

#ifdef DEBUGGING
	dbf = xfopen(DEBUGTERM, "w");

	{
		int i;

		for (i = 1; i < argc; i++) {
			debug(argv[i]);
			debug("\n");
		}
	}
#endif

	/* Parse command-line arguments. */
	parse_args(argc, argv, &options);

#ifdef SYSV_STYLE
#if ENABLE_FEATURE_UTMP
	/* Update the utmp file. */
	update_utmp(options.tty);
#endif
#endif

	debug("calling open_tty\n");
	/* Open the tty as standard { input, output, error }. */
	open_tty(options.tty, &termios, options.flags & F_LOCAL);

#ifdef __linux__
	{
		int iv;

		iv = getpid();
		ioctl(0, TIOCSPGRP, &iv);
	}
#endif
	/* Initialize the termios settings (raw mode, eight-bit, blocking i/o). */
	debug("calling termios_init\n");
	termios_init(&termios, options.speeds[FIRST_SPEED], &options);

	/* write the modem init string and DON'T flush the buffers */
	if (options.flags & F_INITSTRING) {
		debug("writing init string\n");
		write(1, options.initstring, strlen(options.initstring));
	}

	if (!(options.flags & F_LOCAL)) {
		/* go to blocking write mode unless -L is specified */
		fcntl(1, F_SETFL, fcntl(1, F_GETFL, 0) & ~O_NONBLOCK);
	}

	/* Optionally detect the baud rate from the modem status message. */
	debug("before autobaud\n");
	if (options.flags & F_PARSE)
		auto_baud(bb_common_bufsiz1, sizeof(bb_common_bufsiz1), &termios);

	/* Set the optional timer. */
	if (options.timeout)
		alarm(options.timeout);

	/* optionally wait for CR or LF before writing /etc/issue */
	if (options.flags & F_WAITCRLF) {
		char ch;

		debug("waiting for cr-lf\n");
		while (read(0, &ch, 1) == 1) {
			ch &= 0x7f;                     /* strip "parity bit" */
#ifdef DEBUGGING
			fprintf(dbf, "read %c\n", ch);
#endif
			if (ch == '\n' || ch == '\r')
				break;
		}
	}

	chardata = init_chardata;
	if (!(options.flags & F_NOPROMPT)) {
		/* Read the login name. */
		debug("reading login name\n");
		logname = get_logname(bb_common_bufsiz1, sizeof(bb_common_bufsiz1),
				&options, &chardata, &termios);
		while (logname == NULL)
			next_speed(&termios, &options);
	}

	/* Disable timer. */

	if (options.timeout)
		alarm(0);

	/* Finalize the termios settings. */

	termios_final(&options, &termios, &chardata);

	/* Now the newline character should be properly written. */

	write(1, "\n", 1);

	/* Let the login program take care of password validation. */

	execl(options.login, options.login, "--", logname, (char *) 0);
	bb_error_msg_and_die("%s: can't exec %s", options.tty, options.login);
}
