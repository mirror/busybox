/* vi: set sw=4 ts=4: */
/*
 * public domain -- Dave 'Kill a Cop' Cinege <dcinege@psychosis.com>
 * 
 * dutmp
 * Takes utmp formated file on stdin and dumps it's contents 
 * out in colon delimited fields. Easy to 'cut' for shell based 
 * versions of 'who', 'last', etc. IP Addr is output in hex, 
 * little endian on x86.
 * 
 */

/* Mar 13, 2003       Manuel Novoa III
 *
 * 1) Added proper error checking.
 * 2) Allow '-' arg for stdin.
 * 3) For modern libcs, take into account that utmp char[] members
 *    need not be nul-terminated.
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include "busybox.h"

/* Grr... utmp char[] members  do not have to be nul-terminated.
 * Do what we can while still keeping this reasonably small.
 * Note: We are assuming the ut_id[] size is fixed at 4. */

#if (UT_LINESIZE != 32) || (UT_NAMESIZE != 32) || (UT_HOSTSIZE != 256)
#error struct utmp member char[] size(s) have changed!
#endif

extern int dutmp_main(int argc, char **argv)
{
	int file = STDIN_FILENO;
	ssize_t n;
	struct utmp ut;

	if (argc > 2) {
		bb_show_usage();
	}
	++argv;
	if ((argc == 2) && ((argv[0][0] != '-') || argv[0][1])) {
		file = bb_xopen(*argv, O_RDONLY);
	}


	while ((n = safe_read(file, (void*)&ut, sizeof(struct utmp))) != 0) {

		if (n != sizeof(struct utmp)) {
			bb_perror_msg_and_die("short read");
		}

		bb_printf("%d|%d|%.32s|%.4s|%.32s|%.256s|%d|%d|%ld|%ld|%ld|%x\n",
				  ut.ut_type, ut.ut_pid, ut.ut_line,
				  ut.ut_id, ut.ut_user,	ut.ut_host,
				  ut.ut_exit.e_termination, ut.ut_exit.e_exit,
				  ut.ut_session,
				  ut.ut_tv.tv_sec, ut.ut_tv.tv_usec,
				  ut.ut_addr);
	}

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
