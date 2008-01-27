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

// set canonical tty mode
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
	int ret = tcsetattr(fd, TCSANOW, tio);

	if (ret) {
		bb_perror_msg("can't tcsetattr for %s", device);
	}
	return ret;
}

int microcom_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int microcom_main(int argc, char **argv)
{
	struct pollfd pfd[2];
	int nfd;
	//int sfd;
#define sfd (pfd[0].fd)
	char *device_lock_file;
	const char *opt_s = "9600";
	speed_t speed;
	const char *opt_t = "100"; // 0.1 sec timeout
	unsigned timeout;
	struct termios tio0, tiosfd, tio;
	bool istty;

	// fetch options
	opt_complementary = "-1"; /* at least one arg should be there */
	getopt32(argv, "s:t:", &opt_s, &opt_t);
	argc -= optind;
	argv += optind;

	// check sanity
	speed = xatou(opt_s);
	timeout = xatou(opt_t);
	device_lock_file = (char *)bb_basename(argv[0]);

	// try to create lock file in /var/lock
	device_lock_file = xasprintf("/var/lock/LCK..%s", device_lock_file);
	sfd = open(device_lock_file, O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 0644);
	if (sfd < 0) {
		if (errno == EEXIST)
			bb_perror_msg_and_die("can't lock device");
		// We don't abort on other errors: /var/lock can be
		// non-writable or non-existent
		if (ENABLE_FEATURE_CLEAN_UP)
			free(device_lock_file);
		device_lock_file = NULL;
	} else {
		// %4d to make mgetty happy. It treats 4-bytes lock files as binary,
		// not text, PID. Making 5+ char file. Brrr...
		char *s = xasprintf("%4d\n", getpid());
		write(sfd, s, strlen(s));
		if (ENABLE_FEATURE_CLEAN_UP)
			free(s);
		close(sfd);
	}

	// setup signals
	sig_catch(SIGHUP,  signal_handler);
	sig_catch(SIGINT,  signal_handler);
	sig_catch(SIGTERM, signal_handler);
	sig_catch(SIGPIPE, signal_handler);

	// error exit code if we fail to open the device
	signalled = 1;

	// open device
	sfd = open_or_warn(argv[0], O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (sfd < 0)
		goto unlock_and_exit;
	fcntl(sfd, F_SETFL, O_RDWR | O_NOCTTY);

	/* put stdin to "raw mode" (if stdin is a TTY),
		handle one character at a time */
	istty = isatty(STDIN_FILENO);
	if (istty) {
		xget1(STDIN_FILENO, &tio, &tio0);
		if (xset1(STDIN_FILENO, &tio, "stdin"))
			goto close_unlock_and_exit;
//		tcflush(STDIN_FILENO, TCIFLUSH);
		timeout = -1; // tty input? -> set infinite timeout for poll()
	}

	// same thing for modem
	xget1(sfd, &tio, &tiosfd);
	// order device to hang up at exit
	tio.c_cflag |= (CREAD|HUPCL);
	// set device speed
	cfsetspeed(&tio, tty_value_to_baud(speed));
	if (xset1(sfd, &tio, argv[0]))
		goto restore0_close_unlock_and_exit;

	// main loop: check with poll(), then read/write bytes across
	/* pfd[0].fd = sfd; - they are the same */
	pfd[0].events = POLLIN;
	pfd[1].fd = STDIN_FILENO;
	pfd[1].events = POLLIN;

	// TODO: on piped input should we treat NL as CRNL?!

	signalled = 0;
	// initially we have to poll() both stdin and device
	nfd = 2;
	while (!signalled && nfd && safe_poll(pfd, nfd, timeout) > 0) {
		int i;

		for (i = 0; i < nfd; ++i) {
			if (pfd[i].revents & (POLLIN | POLLHUP)) {
//		int fd;
				char c;
				// read a byte
				if (safe_read(pfd[i].fd, &c, 1) < 1) {
					// this can occur at the end of piped input
					// from now we only poll() for device
					nfd--;
					continue;
				}
//		fd = STDOUT_FILENO;
				// stdin requires additional processing
				// (TODO: must be controlled by command line switch)
				if (i) {
					// ^@ sends Break
					if (0 == c) {
						tcsendbreak(sfd, 0);
						continue;
					}
					// ^X exits
					if (24 == c)
						goto done;
//		fd = sfd;
				}
				// write the byte
				write(i ? sfd : STDOUT_FILENO, &c, 1);
//		write(fd, &c, 1);
				// give device a chance to get data
				// wait 0.01 msec
				// (TODO: seems to be arbitrary to me)
				if (i)
					usleep(10);
			}
		}
	}
done:
	tcsetattr(sfd, TCSANOW, &tiosfd);

restore0_close_unlock_and_exit:
	if (istty) {
//		tcflush(STDIN_FILENO, TCIFLUSH);
		tcsetattr(STDIN_FILENO, TCSANOW, &tio0);
	}

close_unlock_and_exit:
	if (ENABLE_FEATURE_CLEAN_UP)
		close(sfd);

unlock_and_exit:
	// delete lock file
	if (device_lock_file) {
		unlink(device_lock_file);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(device_lock_file);
	}
	return signalled;
}
