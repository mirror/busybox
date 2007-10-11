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

int microcom_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int microcom_main(int argc, char **argv)
{
	struct pollfd pfd[2];
#define sfd (pfd[1].fd)
	char *device_lock_file = NULL;
	const char *s;
	const char *opt_s = "9600";
	unsigned speed;
	int len;
	int exitcode = 1;
	struct termios tio0, tiosfd, tio;

	getopt32(argv, "s:", &opt_s);
	argc -= optind;
	argv += optind;
	if (!argv[0])
		bb_show_usage();
	speed = xatou(opt_s);

	// try to create lock file in /var/lock
	s = bb_basename(argv[0]);
	if (!s[0]) {
		errno = ENODEV;
		bb_perror_msg_and_die("can't lock device");
	}
	device_lock_file = xasprintf("/var/lock/LCK..%s", s);
	sfd = open(device_lock_file, O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 0644);
	if (sfd < 0) {
		if (ENABLE_FEATURE_CLEAN_UP)
			free(device_lock_file);
		device_lock_file = NULL;
		if (errno == EEXIST)
			bb_perror_msg_and_die("can't lock device");
		// We don't abort on other errors: /var/lock can be
		// non-writable or non-existent
	} else {
		// %4d to make mgetty happy. It treats 4-bytes lock files as binary,
		// not text, PID. Making 5+ char file. Brrr...
		s = xasprintf("%4d\n", getpid());
		write(sfd, s, strlen(s));
		if (ENABLE_FEATURE_CLEAN_UP)
			free((char*)s);
		close(sfd);
	}

	// open device
	sfd = open(argv[0], O_RDWR);
	if (sfd < 0) {
		bb_perror_msg("can't open device");
		goto unlock_and_exit;
	}

	// put stdin to "raw mode", handle one character at a time
	tcgetattr(STDIN_FILENO, &tio0);
	tio = tio0;
	tio.c_lflag &= ~(ICANON|ECHO);
	tio.c_iflag &= ~(IXON|ICRNL);
	tio.c_oflag &= ~(ONLCR);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tio)) {
		bb_perror_msg("can't tcsetattr for %s", "stdin");
		goto unlock_and_exit;
	}

	/* same thing for modem (plus: set baud rate) - TODO: make CLI option */
	tcgetattr(sfd, &tiosfd);
	tio = tiosfd;
	tio.c_lflag &= ~(ICANON|ECHO);
	tio.c_iflag &= ~(IXON|ICRNL);
	tio.c_oflag &= ~(ONLCR);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	cfsetispeed(&tio, tty_value_to_baud(speed));
	cfsetospeed(&tio, tty_value_to_baud(speed));
	if (tcsetattr(sfd, TCSANOW, &tio)) {
		bb_perror_msg("can't tcsetattr for %s", "device");
		goto unlock_and_exit;
	}

	// disable SIGINT
	signal(SIGINT, SIG_IGN);

	// drain stdin
	tcflush(STDIN_FILENO, TCIFLUSH);
	printf("connected to '%s' (%d bps), exit with ctrl-X...\r\n", argv[0], speed);

	// main loop: check with poll(), then read/write bytes across
	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;
	/*pfd[1].fd = sfd;*/
	pfd[1].events = POLLIN;
	while (1) {
		int i;
		safe_poll(pfd, 2, -1);
		for (i = 0; i < 2; ++i) {
			if (pfd[i].revents & POLLIN) {
				len = read(pfd[i].fd, bb_common_bufsiz1, COMMON_BUFSIZE);
				if (len > 0) {
					if (!i && 24 == bb_common_bufsiz1[0])
						goto done; // ^X exits
					write(pfd[1-i].fd, bb_common_bufsiz1, len);
				}
			}
		}
	}
 done:
	tcsetattr(sfd, TCSANOW, &tiosfd);
	tcsetattr(STDIN_FILENO, TCSANOW, &tio0);
	tcflush(STDIN_FILENO, TCIFLUSH);

	if (ENABLE_FEATURE_CLEAN_UP)
		close(sfd);
	exitcode = 0;

 unlock_and_exit:
	// delete lock file
	if (device_lock_file) {
		unlink(device_lock_file);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(device_lock_file);
	}
	return exitcode;
}
