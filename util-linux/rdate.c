/* vi: set sw=4 ts=4: */
/*
 * The Rdate command will ask a time server for the RFC 868 time
 *  and optionally set the system time.
 *
 * by Sterling Huxley <sterling@europa.com>
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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"


static const int RFC_868_BIAS = 2208988800UL;

static time_t askremotedate(const char *host)
{
	struct hostent *h;
	struct sockaddr_in s_in;
	struct servent *tserv;
	unsigned long int nett, localt;
	int fd;

	h = xgethostbyname(host);         /* get the IP addr */

	if ((tserv = getservbyname("time", "tcp")) == NULL)   /* find port # */
		perror_msg_and_die("time");

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)    /* get net connection */
		perror_msg_and_die("socket");

	memcpy(&s_in.sin_addr, h->h_addr, sizeof(s_in.sin_addr));
	s_in.sin_port= tserv->s_port;
	s_in.sin_family = AF_INET;

	if (connect(fd, (struct sockaddr *)&s_in, sizeof(s_in)) < 0)      /* connect to time server */
		perror_msg_and_die("%s", host);

	if (read(fd, (void *)&nett, 4) != 4)    /* read time from server */
		error_msg_and_die("%s did not send the complete time", host);

	close(fd);

	/* convert from network byte order to local byte order.
	 * RFC 868 time is the number of seconds
	 *  since 00:00 (midnight) 1 January 1900 GMT
	 *  the RFC 868 time 2,208,988,800 corresponds to 00:00  1 Jan 1970 GMT
	 * Subtract the RFC 868 time  to get Linux epoch
	 */
	localt= ntohl(nett) - RFC_868_BIAS;

	return(localt);
}

int rdate_main(int argc, char **argv)
{
	time_t remote_time;
	int opt;
	int setdate = 1;
	int printdate = 1;

	/* Interpret command line args */
	while ((opt = getopt(argc, argv, "sp")) > 0) {
		switch (opt) {
			case 's':
				printdate = 0;
				setdate = 1;
				break;
			case 'p':
				printdate = 1;
				setdate = 0;
				break;
			default:
				show_usage();
		}
	}

	if (optind == argc)
		show_usage();

	remote_time = askremotedate(argv[optind]);

	if (setdate) {
		if (stime(&remote_time) < 0)
			perror_msg_and_die("Could not set time of day");
	}

	if (printdate)
		printf("%s", ctime(&remote_time));

	return EXIT_SUCCESS;
}
