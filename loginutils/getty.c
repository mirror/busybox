/* vi: set sw=4 ts=4: */
/* Based on agetty - another getty program for Linux. By W. Z. Venema 1989
 * Ported to Linux by Peter Orbaek <poe@daimi.aau.dk>
 * This program is freely distributable.
 *
 * option added by Eric Rasmussen <ear@usfirst.org> - 12/28/95
 *
 * 1999-02-22 Arkadiusz Mickiewicz <misiek@misiek.eu.org>
 * - added Native Language Support
 *
 * 1999-05-05 Thorsten Kranzkowski <dl8bcu@gmx.net>
 * - enable hardware flow control before displaying /etc/issue
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include <syslog.h>
#if ENABLE_FEATURE_UTMP
# include <utmp.h> /* LOGIN_PROCESS */
#endif
#ifndef IUCLC
# define IUCLC 0
#endif

/*
 * Some heuristics to find out what environment we are in: if it is not
 * System V, assume it is SunOS 4.
 */
#ifdef LOGIN_PROCESS                    /* defined in System V utmp.h */
# include <sys/utsname.h>
#else /* if !sysV style, wtmp/utmp code is off */
# undef ENABLE_FEATURE_UTMP
# undef ENABLE_FEATURE_WTMP
# define ENABLE_FEATURE_UTMP 0
# define ENABLE_FEATURE_WTMP 0
#endif


/* The following is used for understandable diagnostics. */
#ifdef DEBUGGING
static FILE *dbf;
# define DEBUGTERM "/dev/ttyp0"
# define debug(...) do { fprintf(dbf, __VA_ARGS__); fflush(dbf); } while (0)
#else
# define debug(...) ((void)0)
#endif


/*
 * Things you may want to modify.
 *
 * You may disagree with the default line-editing etc. characters defined
 * below. Note, however, that DEL cannot be used for interrupt generation
 * and for line editing at the same time.
 */
#undef  _PATH_LOGIN
#define _PATH_LOGIN "/bin/login"

/* Displayed before the login prompt.
 * If ISSUE is not defined, getty will never display the contents of the
 * /etc/issue file. You will not want to spit out large "issue" files at the
 * wrong baud rate.
 */
#define ISSUE "/etc/issue"

/* Some shorthands for control characters. */
#define CTL(x)          ((x) ^ 0100)    /* Assumes ASCII dialect */
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
#define MAX_SPEED       10              /* max. nr. of baud rates */

struct globals {
	unsigned timeout;               /* time-out period */
	const char *login;              /* login program */
	const char *tty;                /* name of tty */
	const char *initstring;         /* modem init string */
	const char *issue;              /* alternative issue file */
	int numspeed;                   /* number of baud rates to try */
	int speeds[MAX_SPEED];          /* baud rates to be tried */
	struct termios termios;         /* terminal mode bits */
	/* Storage for things detected while the login name was read. */
	unsigned char erase;            /* erase character */
	unsigned char eol;              /* end-of-line character */
	char line_buf[128];
};

#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

//usage:#define getty_trivial_usage
//usage:       "[OPTIONS] BAUD_RATE[,BAUD_RATE]... TTY [TERMTYPE]"
//usage:#define getty_full_usage "\n\n"
//usage:       "Open a tty, prompt for a login name, then invoke /bin/login\n"
//usage:     "\nOptions:"
//usage:     "\n	-h		Enable hardware (RTS/CTS) flow control"
//usage:     "\n	-i		Don't display /etc/issue"
//usage:     "\n	-L		Local line, set CLOCAL on it"
//usage:     "\n	-m		Get baud rate from modem's CONNECT status message"
//usage:     "\n	-w		Wait for CR or LF before sending /etc/issue"
//usage:     "\n	-n		Don't prompt for a login name"
//usage:     "\n	-f ISSUE_FILE	Display ISSUE_FILE instead of /etc/issue"
//usage:     "\n	-l LOGIN	Invoke LOGIN instead of /bin/login"
//usage:     "\n	-t SEC		Terminate after SEC if no username is read"
//usage:     "\n	-I INITSTR	Send INITSTR before anything else"
//usage:     "\n	-H HOST		Log HOST into the utmp file as the hostname"
//usage:     "\n"
//usage:     "\nBAUD_RATE of 0 leaves it unchanged"

static const char opt_string[] ALIGN1 = "I:LH:f:hil:mt:wn";
#define F_INITSTRING    (1 << 0)   /* -I */
#define F_LOCAL         (1 << 1)   /* -L */
#define F_FAKEHOST      (1 << 2)   /* -H */
#define F_CUSTISSUE     (1 << 3)   /* -f */
#define F_RTSCTS        (1 << 4)   /* -h */
#define F_NOISSUE       (1 << 5)   /* -i */
#define F_LOGIN         (1 << 6)   /* -l */
#define F_PARSE         (1 << 7)   /* -m */
#define F_TIMEOUT       (1 << 8)   /* -t */
#define F_WAITCRLF      (1 << 9)   /* -w */
#define F_NOPROMPT      (1 << 10)  /* -n */


/* convert speed string to speed code; return <= 0 on failure */
static int bcode(const char *s)
{
	int value = bb_strtou(s, NULL, 10); /* yes, int is intended! */
	if (value < 0) /* bad terminating char, overflow, etc */
		return value;
	return tty_value_to_baud(value);
}

/* parse alternate baud rates */
static void parse_speeds(char *arg)
{
	char *cp;

	/* NB: at least one iteration is always done */
	debug("entered parse_speeds\n");
	while ((cp = strsep(&arg, ",")) != NULL) {
		G.speeds[G.numspeed] = bcode(cp);
		if (G.speeds[G.numspeed] < 0)
			bb_error_msg_and_die("bad speed: %s", cp);
		/* note: arg "0" turns into speed B0 */
		G.numspeed++;
		if (G.numspeed > MAX_SPEED)
			bb_error_msg_and_die("too many alternate speeds");
	}
	debug("exiting parse_speeds\n");
}

/* parse command-line arguments */
static void parse_args(char **argv, char **fakehost_p)
{
	char *ts;
	int flags;

	opt_complementary = "-2:t+"; /* at least 2 args; -t N */
	flags = getopt32(argv, opt_string,
		&G.initstring, fakehost_p, &G.issue,
		&G.login, &G.timeout
	);
	if (flags & F_INITSTRING) {
		G.initstring = xstrdup(G.initstring);
		/* decode \ddd octal codes into chars */
		strcpy_and_process_escape_sequences((char*)G.initstring, G.initstring);
	}
	argv += optind;
	debug("after getopt\n");

	/* We loosen up a bit and accept both "baudrate tty" and "tty baudrate" */
	G.tty = argv[0];        /* tty name */
	ts = argv[1];           /* baud rate(s) */
	if (isdigit(argv[0][0])) {
		/* A number first, assume it's a speed (BSD style) */
		G.tty = ts;     /* tty name is in argv[1] */
		ts = argv[0];   /* baud rate(s) */
	}
	parse_speeds(ts);
	applet_name = xasprintf("getty: %s", G.tty);

	if (argv[2])
		xsetenv("TERM", argv[2]);

	debug("exiting parse_args\n");
}

/* set up tty as standard input, output, error */
static void open_tty(void)
{
	/* Set up new standard input, unless we are given an already opened port. */
	if (NOT_LONE_DASH(G.tty)) {
		if (G.tty[0] != '/')
			G.tty = xasprintf("/dev/%s", G.tty); /* will leak it */

		/* Open the tty as standard input. */
		debug("open(2)\n");
		close(0);
		xopen(G.tty, O_RDWR | O_NONBLOCK); /* uses fd 0 */

		/* Set proper protections and ownership. */
		fchown(0, 0, 0);        /* 0:0 */
		fchmod(0, 0620);        /* crw--w---- */
	} else {
		/*
		 * Standard input should already be connected to an open port. Make
		 * sure it is open for read/write.
		 */
		if ((fcntl(0, F_GETFL) & (O_RDWR|O_RDONLY|O_WRONLY)) != O_RDWR)
			bb_error_msg_and_die("stdin is not open for read/write");
	}
}

/* initialize termios settings */
static void termios_init(int speed)
{
	/* Flush input and output queues, important for modems!
	 * Users report losing previously queued output chars, and I hesitate
	 * to use tcdrain here instead of tcflush - I imagine it can block.
	 * Using small sleep instead.
	 */
	usleep(100*1000); /* 0.1 sec */
	tcflush(STDIN_FILENO, TCIOFLUSH);

	/* Set speed if it wasn't specified as "0" on command line. */
	if (speed != B0)
		cfsetspeed(&G.termios, speed);

	/*
	 * Initial termios settings: 8-bit characters, raw-mode, blocking i/o.
	 * Special characters are set after we have read the login name; all
	 * reads will be done in raw mode anyway. Errors will be dealt with
	 * later on.
	 */
	G.termios.c_cflag = CS8 | HUPCL | CREAD;
	if (option_mask32 & F_LOCAL)
		G.termios.c_cflag |= CLOCAL;
	G.termios.c_iflag = 0;
	G.termios.c_lflag = 0;
	G.termios.c_oflag = OPOST | ONLCR;
	G.termios.c_cc[VMIN] = 1;
	G.termios.c_cc[VTIME] = 0;
#ifdef __linux__
	G.termios.c_line = 0;
#endif
#ifdef CRTSCTS
	if (option_mask32 & F_RTSCTS)
		G.termios.c_cflag |= CRTSCTS;
#endif

	tcsetattr_stdin_TCSANOW(&G.termios);

	debug("term_io 2\n");
}

/* extract baud rate from modem status message */
static void auto_baud(void)
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
	iflag = G.termios.c_iflag;
	G.termios.c_iflag |= ISTRIP;    /* enable 8th-bit stripping */
	vmin = G.termios.c_cc[VMIN];
	G.termios.c_cc[VMIN] = 0;       /* don't block if queue empty */
	tcsetattr_stdin_TCSANOW(&G.termios);

	/*
	 * Wait for a while, then read everything the modem has said so far and
	 * try to extract the speed of the dial-in call.
	 */
	sleep(1);
	nread = safe_read(STDIN_FILENO, G.line_buf, sizeof(G.line_buf) - 1);
	if (nread > 0) {
		G.line_buf[nread] = '\0';
		for (bp = G.line_buf; bp < G.line_buf + nread; bp++) {
			if (isdigit(*bp)) {
				speed = bcode(bp);
				if (speed > 0)
					cfsetspeed(&G.termios, speed);
				break;
			}
		}
	}

	/* Restore terminal settings. Errors will be dealt with later on. */
	G.termios.c_iflag = iflag;
	G.termios.c_cc[VMIN] = vmin;
	tcsetattr_stdin_TCSANOW(&G.termios);
}

/* get user name, establish parity, speed, erase, kill, eol;
 * return NULL on BREAK, logname on success */
static char *get_logname(void)
{
	char *bp;
	char c;

	/* Flush pending input (esp. after parsing or switching the baud rate). */
	usleep(100*1000); /* 0.1 sec */
	tcflush(STDIN_FILENO, TCIOFLUSH);

	/* Prompt for and read a login name. */
	G.line_buf[0] = '\0';
	while (!G.line_buf[0]) {
		/* Write issue file and prompt. */
#ifdef ISSUE
		if (!(option_mask32 & F_NOISSUE))
			print_login_issue(G.issue, G.tty);
#endif
		print_login_prompt();

		/* Read name, watch for break, parity, erase, kill, end-of-line. */
		bp = G.line_buf;
		G.eol = '\0';
		while (1) {
			/* Do not report trivial EINTR/EIO errors. */
			errno = EINTR; /* make read of 0 bytes be silent too */
			if (read(STDIN_FILENO, &c, 1) < 1) {
				if (errno == EINTR || errno == EIO)
					exit(EXIT_SUCCESS);
				bb_perror_msg_and_die(bb_msg_read_error);
			}

			/* BREAK. If we have speeds to try,
			 * return NULL (will switch speeds and return here) */
			if (c == '\0' && G.numspeed > 1)
				return NULL;

			/* Do erase, kill and end-of-line processing. */
			switch (c) {
			case CR:
			case NL:
				*bp = '\0';
				G.eol = c;
				goto got_logname;
			case BS:
			case DEL:
				G.erase = c;
				if (bp > G.line_buf) {
					full_write(STDOUT_FILENO, "\010 \010", 3);
					bp--;
				}
				break;
			case CTL('U'):
				while (bp > G.line_buf) {
					full_write(STDOUT_FILENO, "\010 \010", 3);
					bp--;
				}
				break;
			case CTL('D'):
				exit(EXIT_SUCCESS);
			default:
				if ((unsigned char)c < ' ') {
					/* ignore garbage characters */
				} else if ((int)(bp - G.line_buf) < sizeof(G.line_buf) - 1) {
					/* echo and store the character */
					full_write(STDOUT_FILENO, &c, 1);
					*bp++ = c;
				}
				break;
			}
		} /* end of get char loop */
 got_logname: ;
	} /* while logname is empty */

	return G.line_buf;
}

/* set the final tty mode bits */
static void termios_final(void)
{
	/* General terminal-independent stuff. */
	G.termios.c_iflag |= IXON | IXOFF;    /* 2-way flow control */
	G.termios.c_lflag |= ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHOKE;
	/* no longer in lflag: | ECHOCTL | ECHOPRT */
	G.termios.c_oflag |= OPOST;
	/* G.termios.c_cflag = 0; */
	G.termios.c_cc[VINTR] = DEF_INTR;     /* default interrupt */
	G.termios.c_cc[VQUIT] = DEF_QUIT;     /* default quit */
	G.termios.c_cc[VEOF] = DEF_EOF;       /* default EOF character */
	G.termios.c_cc[VEOL] = DEF_EOL;
#ifdef VSWTC
	G.termios.c_cc[VSWTC] = DEF_SWITCH;   /* default switch character */
#endif

	/* Account for special characters seen in input. */
	if (G.eol == CR) {
		G.termios.c_iflag |= ICRNL;   /* map CR in input to NL */
		G.termios.c_oflag |= ONLCR;   /* map NL in output to CR-NL */
	}
	G.termios.c_cc[VERASE] = G.erase;     /* set erase character */
	G.termios.c_cc[VKILL] = DEF_KILL;     /* set kill character */

#ifdef CRTSCTS
	/* Optionally enable hardware flow control */
	if (option_mask32 & F_RTSCTS)
		G.termios.c_cflag |= CRTSCTS;
#endif

	/* Finally, make the new settings effective */
	if (tcsetattr_stdin_TCSANOW(&G.termios) < 0)
		bb_perror_msg_and_die("tcsetattr");
}

int getty_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int getty_main(int argc UNUSED_PARAM, char **argv)
{
	int n;
	pid_t pid;
	char *fakehost = NULL;    /* Fake hostname for ut_host */
	char *logname;

	INIT_G();
	G.login = _PATH_LOGIN;    /* default login program */
	G.initstring = "";        /* modem init string */
#ifdef ISSUE
	G.issue = ISSUE;          /* default issue file */
#endif
	G.erase = DEF_ERASE;
	G.eol = CR;

	/* Parse command-line arguments. */
	parse_args(argv, &fakehost);

	logmode = LOGMODE_NONE;

	/* Create new session, lose controlling tty, if any */
	/* docs/ctty.htm says:
	 * "This is allowed only when the current process
	 *  is not a process group leader" - is this a problem? */
	setsid();
	/* close stdio, and stray descriptors, just in case */
	n = xopen(bb_dev_null, O_RDWR);
	/* dup2(n, 0); - no, we need to handle "getty - 9600" too */
	xdup2(n, 1);
	xdup2(n, 2);
	while (n > 2)
		close(n--);

	/* Logging. We want special flavor of error_msg_and_die */
	die_sleep = 10;
	msg_eol = "\r\n";
	/* most likely will internally use fd #3 in CLOEXEC mode: */
	openlog(applet_name, LOG_PID, LOG_AUTH);
	logmode = LOGMODE_BOTH;

#ifdef DEBUGGING
	dbf = xfopen_for_write(DEBUGTERM);
	for (n = 1; argv[n]; n++) {
		debug(argv[n]);
		debug("\n");
	}
#endif

	/* Open the tty as standard input, if it is not "-" */
	/* If it's not "-" and not taken yet, it will become our ctty */
	debug("calling open_tty\n");
	open_tty();
	ndelay_off(0);
	debug("duping\n");
	xdup2(0, 1);
	xdup2(0, 2);

	/*
	 * The following ioctl will fail if stdin is not a tty, but also when
	 * there is noise on the modem control lines. In the latter case, the
	 * common course of action is (1) fix your cables (2) give the modem more
	 * time to properly reset after hanging up. SunOS users can achieve (2)
	 * by patching the SunOS kernel variable "zsadtrlow" to a larger value;
	 * 5 seconds seems to be a good value.
	 */
	if (tcgetattr(STDIN_FILENO, &G.termios) < 0)
		bb_perror_msg_and_die("tcgetattr");

	pid = getpid();
#ifdef __linux__
// FIXME: do we need this? Otherwise "-" case seems to be broken...
	// /* Forcibly make fd 0 our controlling tty, even if another session
	//  * has it as a ctty. (Another session loses ctty). */
	// ioctl(STDIN_FILENO, TIOCSCTTY, (void*)1);
	/* Make ourself a foreground process group within our session */
	tcsetpgrp(STDIN_FILENO, pid);
#endif

	/* Update the utmp file. This tty is ours now! */
	update_utmp(pid, LOGIN_PROCESS, G.tty, "LOGIN", fakehost);

	/* Initialize the termios settings (raw mode, eight-bit, blocking i/o). */
	debug("calling termios_init\n");
	termios_init(G.speeds[0]);

	/* Write the modem init string and DON'T flush the buffers */
	if (option_mask32 & F_INITSTRING) {
		debug("writing init string\n");
		full_write1_str(G.initstring);
	}

	/* Optionally detect the baud rate from the modem status message */
	debug("before autobaud\n");
	if (option_mask32 & F_PARSE)
		auto_baud();

	/* Set the optional timer */
	alarm(G.timeout); /* if 0, alarm is not set */

	/* Optionally wait for CR or LF before writing /etc/issue */
	if (option_mask32 & F_WAITCRLF) {
		char ch;

		debug("waiting for cr-lf\n");
		while (safe_read(STDIN_FILENO, &ch, 1) == 1) {
			debug("read %x\n", (unsigned char)ch);
			if (ch == '\n' || ch == '\r')
				break;
		}
	}

	logname = NULL;
	if (!(option_mask32 & F_NOPROMPT)) {
		/* NB: termios_init already set line speed
		 * to G.speeds[0] */
		int baud_index = 0;

		while (1) {
			/* Read the login name. */
			debug("reading login name\n");
			logname = get_logname();
			if (logname)
				break;
			/* We are here only if G.numspeed > 1. */
			baud_index = (baud_index + 1) % G.numspeed;
			cfsetspeed(&G.termios, G.speeds[baud_index]);
			tcsetattr_stdin_TCSANOW(&G.termios);
		}
	}

	/* Disable timer. */
	alarm(0);

	/* Finalize the termios settings. */
	termios_final();

	/* Now the newline character should be properly written. */
	full_write(STDOUT_FILENO, "\n", 1);

	/* Let the login program take care of password validation. */
	/* We use PATH because we trust that root doesn't set "bad" PATH,
	 * and getty is not suid-root applet. */
	/* With -n, logname == NULL, and login will ask for username instead */
	BB_EXECLP(G.login, G.login, "--", logname, NULL);
	bb_error_msg_and_die("can't execute '%s'", G.login);
}
