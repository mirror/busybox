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
 * made against libc6
 */

#include "internal.h"
#include <stdio.h>
#include <errno.h>
#include <utmp.h>
#define BB_DECLARE_EXTERN
#define bb_need_io_error
#include "messages.c"

static const char dutmp_usage[] = "dutmp\n"
	"\n"
	"\tDump file or stdin utmp file format to stdout, pipe delimited.\n"
	"\tdutmp /var/run/utmp\n";

extern int dutmp_main(int argc, char **argv)
{

	FILE *f;
	struct utmp ut;

	if (argc<2) {
		f = stdin;
	} else if (*argv[1] == '-' ) {
		usage(dutmp_usage);
	} else  {
		f = fopen(argv[1], "r");
		if (f == NULL) {
			fatalError(io_error, argv[1], strerror(errno));
		}
	}

	while (fread(&ut, sizeof(struct utmp), 1, f)) {
		printf("%d|%d|%s|%s|%s|%s|%d|%d|%ld|%ld|%ld|%x\n",
			   ut.ut_type, ut.ut_pid, ut.ut_line,
			   ut.ut_id, ut.ut_user, ut.ut_host,
			   ut.ut_exit.e_termination, ut.ut_exit.e_exit,
			   ut.ut_session,
			   ut.ut_tv.tv_sec, ut.ut_tv.tv_usec, ut.ut_addr);
	}

	exit(TRUE);
}
