/* vi: set sw=4 ts=4: */
/*
 * Copyright 2008 Walter Harms (WHarms@bfs.de)
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */
#include "libbb.h"
#include "lpr.h"

/*
  this is a *very* resticted form of lpq
  since we do not read /etc/printcap (and never will)
  we can only do things rfc1179 allows:
  - show print jobs for a given queue long/short form
  - remove a job from a given queue

  -P <queue>
  -s short
  -d delete job
  -f force any waiting job to be printed
*/
enum {
	LPQ_SHORT = 1 << 0,
	LPQ_DELETE = 1 << 1,
	LPQ_FORCE = 1 << 2,
	LPQ_P = 1 << 3,
	LPQ_t = 1 << 4,
};

/*
  print everthing that comes
*/
static void get_answer(int sockfd)
{
	char buf[80];
	int n;

	buf[0] = '\n';
	while (1) {
		n = safe_read(sockfd, buf, sizeof(buf));
		if (n <= 0)
			break;
		full_write(STDOUT_FILENO, buf, n);
		buf[0] = buf[n-1]; /* last written char */
	}
	
	/* Don't leave last output line unterminated */
	if (buf[0] != '\n')
		full_write(STDOUT_FILENO, "\n", 1);
}

/*
  is this too simple ?
  should we support more ENV ?
  PRINTER, LPDEST, NPRINTER, NGPRINTER
*/
int lpq_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int lpq_main(int argc, char *argv[])
{
	struct netprint netprint;
	const char *netopt;
	const char *delopt = "0";
	int sockfd = sockfd; /* for compiler */
	unsigned opt;
	int delay; /* delay in [s] */

	netopt = NULL;
	opt = getopt32(argv, "sdfP:t:", &netopt, &delopt);
	argv += optind;
	delay = xatoi_u(delopt);
	parse_prt(netopt, &netprint);

	/* do connect */
	if (opt & (LPQ_FORCE|LPQ_DELETE))
		sockfd = xconnect_stream(netprint.lsa);

	/* force printing of every job still in queue */
	if (opt & LPQ_FORCE) {
		fdprintf(sockfd, "\x1" "%s", netprint.queue);
		get_answer(sockfd);
		return EXIT_SUCCESS;
	}

	/* delete job (better a list of jobs). username is now LOGNAME */
	if (opt & LPQ_DELETE) {
		while (*argv) {
			fdprintf(sockfd, "\x5" "%s %s %s",
					netprint.queue,
					getenv("LOGNAME"), /* FIXME - may be NULL? */
					*argv);
			get_answer(sockfd);
			argv++;
		}
		return EXIT_SUCCESS;
	}

	do {
		sockfd = xconnect_stream(netprint.lsa);
		fdprintf(sockfd, "%c%s\n", (opt & LPQ_SHORT) ? 3 : 4,
				netprint.queue);

		get_answer(sockfd);
		close(sockfd);
		sleep(delay);
	} while (delay != 0);

	return EXIT_SUCCESS;
}
