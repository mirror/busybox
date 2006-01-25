/* vi: set sw=4 ts=4: */
/*
 * The Rdate command will ask a time server for the RFC 868 time
 *  and optionally set the system time.
 *
 * by Sterling Huxley <sterling@europa.com>
 *
 * Licensed under GPL v2 or later, see file License for details.
*/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "busybox.h"


static const int RFC_868_BIAS = 2208988800UL;

static void socket_timeout(int sig)
{
	bb_error_msg_and_die("timeout connecting to time server");
}

static time_t askremotedate(const char *host)
{
	unsigned long nett;
	struct sockaddr_in s_in;
	int fd;

	bb_lookup_host(&s_in, host);
	s_in.sin_port = bb_lookup_port("time", "tcp", 37);

	/* Add a timeout for dead or inaccessible servers */
	alarm(10);
	signal(SIGALRM, socket_timeout);

	fd = xconnect(&s_in);

	if (safe_read(fd, (void *)&nett, 4) != 4)    /* read time from server */
		bb_error_msg_and_die("%s did not send the complete time", host);
	close(fd);

	/* convert from network byte order to local byte order.
	 * RFC 868 time is the number of seconds
	 *  since 00:00 (midnight) 1 January 1900 GMT
	 *  the RFC 868 time 2,208,988,800 corresponds to 00:00  1 Jan 1970 GMT
	 * Subtract the RFC 868 time  to get Linux epoch
	 */

	return(ntohl(nett) - RFC_868_BIAS);
}

int rdate_main(int argc, char **argv)
{
	time_t remote_time;
	unsigned long flags;

	bb_opt_complementally = "-1";
	flags = bb_getopt_ulflags(argc, argv, "sp");

	remote_time = askremotedate(argv[optind]);

	if (flags & 1) {
		time_t current_time;

		time(&current_time);
		if (current_time == remote_time)
			bb_error_msg("Current time matches remote time.");
		else
			if (stime(&remote_time) < 0)
				bb_perror_msg_and_die("Could not set time of day");

	/* No need to check for the -p flag as it's the only option left */

	} else printf("%s", ctime(&remote_time));

	return EXIT_SUCCESS;
}
