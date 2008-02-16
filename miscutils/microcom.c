/* vi: set sw=4 ts=4: */
/*
 * bare bones 'talk to modem' program - similar to 'cu -l $device'
 * inspired by mgetty's microcom
 *
 * Copyright (C) 2007 by Vladimir Dronnikov <dronnikov@gmail.ru>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

/* All known arches use small ints for signals */
static volatile smallint signalled;

static void signal_handler(int signo)
{
	signalled = signo;
}

// set raw tty mode
static void xget1(int fd, struct termios *t, struct termios *oldt)
{ 
	tcgetattr(fd, oldt);
	*t = *oldt;
	cfmakeraw(t);
//	t->c_lflag &= ~(ISIG|ICANON|ECHO|IEXTEN);
//	t->c_iflag &= ~(BRKINT|IXON|ICRNL);
//	t->c_oflag &= ~(ONLCR);
//	t->c_cc[VMIN]  = 1;
//	t->c_cc[VTIME] = 0;
}

static int xset1(int fd, struct termios *tio, const char *device)
{
	int ret = tcsetattr(fd, TCSAFLUSH, tio);

	if (ret) {
		bb_perror_msg("can't tcsetattr for %s", device);
	}
	return ret;
}

int microcom_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int microcom_main(int argc, char **argv)
{
	int sfd;
	int nfd;
	struct pollfd pfd[2];
	struct termios tio0, tiosfd, tio;
	char *device_lock_file;
	enum {
		OPT_X = 1 << 0, // do not respect Ctrl-X, Ctrl-@
		OPT_s = 1 << 1, // baudrate
		OPT_d = 1 << 2, // wait for device response, msecs
		OPT_t = 1 << 3, // timeout, ms
	};
	speed_t speed = 9600;
	int delay = -1;
	int timeout = -1;

	// fetch options
	char *opt_s;
	char *opt_d;
	char *opt_t;
	unsigned opts;
	opt_complementary = "=1"; // exactly one arg should be there
	opts = getopt32(argv, "Xs:d:t:", &opt_s, &opt_d, &opt_t);

	// apply options
	if (opts & OPT_s)
		speed = xatoi_u(opt_s);
	if (opts & OPT_d)
		delay = xatoi_u(opt_d);
	if (opts & OPT_t)
		timeout = xatoi_u(opt_t);

//	argc -= optind;
	argv += optind;

	// try to create lock file in /var/lock
	device_lock_file = (char *)bb_basename(argv[0]);
	device_lock_file = xasprintf("/var/lock/LCK..%s", device_lock_file);
	sfd = open(device_lock_file, O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 0644);
	if (sfd < 0) {
		if (errno == EEXIST)
			bb_perror_msg_and_die("can't create %s", device_lock_file);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(device_lock_file);
		device_lock_file = NULL;
	}
	if (sfd > 0) {
		// %4d to make mgetty happy. It treats 4-bytes lock files as binary,
		// not text, PID. Making 5+ char file. Brrr...
		char *s = xasprintf("%4d\n", getpid());
		write(sfd, s, strlen(s));
		if (ENABLE_FEATURE_CLEAN_UP)
			free(s);
		close(sfd);
	}

	// setup signals
	bb_signals_recursive(0
			+ (1 << SIGHUP)
			+ (1 << SIGINT)
			+ (1 << SIGTERM)
			+ (1 << SIGPIPE)
			, signal_handler);

	// error exit code if we fail to open the device
	signalled = 1;

	// open device
	sfd = open_or_warn(argv[0], O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (sfd < 0)
		goto done;
	fcntl(sfd, F_SETFL, O_RDWR | O_NOCTTY);

	// put stdin to "raw mode" (if stdin is a TTY),
	// handle one character at a time
	if (isatty(STDIN_FILENO)) {
		xget1(STDIN_FILENO, &tio, &tio0);
		if (xset1(STDIN_FILENO, &tio, "stdin"))
			goto done;
	}

	// same thing for modem
	xget1(sfd, &tio, &tiosfd);
	// order device to hang up at exit
	tio.c_cflag |= (CREAD|HUPCL);
//	if (!istty)
//		tio.c_iflag |= (IGNCR);
	// set device speed
	cfsetspeed(&tio, tty_value_to_baud(speed));
	if (xset1(sfd, &tio, argv[0]))
		goto restore0_and_done;

	// main loop: check with poll(), then read/write bytes across
	pfd[0].fd = sfd;
	pfd[0].events = POLLIN;
	pfd[1].fd = STDIN_FILENO;
	pfd[1].events = POLLIN;

	signalled = 0;
	nfd = 2;
	while (!signalled && safe_poll(pfd, nfd, timeout) > 0) {
		if (pfd[1].revents) {
			char c;
			// read from stdin -> write to device
			if (safe_read(STDIN_FILENO, &c, 1) < 1) {
				// don't poll stdin anymore if we got EOF/error
				pfd[1].revents = 0;
				nfd--;
				goto check_stdin;
			}
			// do we need special processing?
			if (!(opts & OPT_X)) {
				// ^@ sends Break
				if (VINTR == c) {
					tcsendbreak(sfd, 0);
					goto check_stdin;
				}
				// ^X exits
				if (24 == c)
					break;
			}
			write(sfd, &c, 1);
			if (delay >= 0)
				safe_poll(pfd, 1, delay);
		}
check_stdin:
		if (pfd[0].revents) {
			ssize_t len;
			// read from device -> write to stdout
			len = safe_read(sfd, bb_common_bufsiz1, sizeof(bb_common_bufsiz1));
			if (len > 0)
				full_write(STDOUT_FILENO, bb_common_bufsiz1, len);
			// else { EOF/error - what to do? }
		}
	}

	tcsetattr(sfd, TCSAFLUSH, &tiosfd);

restore0_and_done:
	if (isatty(STDIN_FILENO))
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio0);

done:
	if (device_lock_file)
		unlink(device_lock_file);

	return signalled;
}
