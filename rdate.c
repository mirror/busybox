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

#include "busybox.h"
#define BB_DECLARE_EXTERN
#include "messages.c"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>


static const int RFC_868_BIAS = 2208988800UL;

int setdate= 0;
int printdate= 0;

time_t askremotedate(char *host)
{
	struct hostent *h;
	struct sockaddr_in sin;
	struct servent *tserv;
	unsigned long int nett, localt;
	int fd;

	if (!(h = gethostbyname(host))) {	/* get the IP addr */
		perror_msg("%s", host);
		return(-1);
	}
	if ((tserv = getservbyname("time", "tcp")) == NULL) { /* find port # */
		perror_msg("%s", "time");
		return(-1);
	}
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {  /* get net connection */
		perror_msg("%s", "socket");
		return(-1);
	}

	memcpy(&sin.sin_addr, h->h_addr, sizeof(sin.sin_addr));
	sin.sin_port= tserv->s_port;
	sin.sin_family = AF_INET;

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {	/* connect to time server */
		perror_msg("%s", host);
		close(fd);
		return(-1);
	}
	if (read(fd, (void *)&nett, 4) != 4) {	/* read time from server */
		close(fd);
		error_msg("%s did not send the complete time\n", host);
	}
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
	time_t time;
	int opt;

	/* Interpret command line args */
	/* do special-case option parsing */
	if (argv[1] && (strcmp(argv[1], "--help") == 0))
		usage(rdate_usage);

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "Hsp")) > 0) {
		switch (opt) {
			default:
			case 'H':
				usage(rdate_usage);
				break;
			case 's':
				setdate++;
				break;
			case 'p':
				printdate++;
				break;
		}
	}

	/* the default action is to set the date */
	if (printdate==0 && setdate==0) setdate++;

	if (optind == argc) {
		usage(rdate_usage);
	}

	if ((time= askremotedate(argv[optind])) == (time_t)-1) {
		return EXIT_FAILURE;
	}
	if (setdate) {
		if (stime(&time) < 0)
			perror_msg_and_die("Could not set time of day");
	}
	if (printdate) {
		printf("%s", ctime(&time));
	}

	return EXIT_SUCCESS;
}
