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
 * Modified to support all sorts of libcs by 
 * Erik Andersen <andersen@lineo.com>
 */

#include "busybox.h"
#include <sys/types.h>
#include <fcntl.h>

#include <errno.h>
#define BB_DECLARE_EXTERN
#define bb_need_io_error
#include "messages.c"
#include <utmp.h>

extern int dutmp_main(int argc, char **argv)
{

	int file;
	struct utmp ut;

	if (argc<2) {
		file = fileno(stdin);
	} else if (*argv[1] == '-' ) {
		usage(dutmp_usage);
	} else  {
		file = open(argv[1], O_RDONLY);
		if (file < 0) {
			error_msg_and_die(io_error, argv[1], strerror(errno));
		}
	}

/* Kludge around the fact that the binary format for utmp has changed, and the
 * fact the stupid libc doesn't have a reliable #define to announce that libc5
 * is being used.  sigh.
 */
#if ! defined __GLIBC__
	while (read(file, (void*)&ut, sizeof(struct utmp))) {
		printf("%d|%d|%s|%s|%s|%s|%s|%lx\n",
				ut.ut_type, ut.ut_pid, ut.ut_line,
				ut.ut_id, ut.ut_user, ut.ut_host,
				ctime(&(ut.ut_time)), 
				(long)ut.ut_addr);
	}
#else
	while (read(file, (void*)&ut, sizeof(struct utmp))) {
		printf("%d|%d|%s|%s|%s|%s|%d|%d|%ld|%ld|%ld|%x\n",
		ut.ut_type, ut.ut_pid, ut.ut_line,
		ut.ut_id, ut.ut_user,	ut.ut_host,
		ut.ut_exit.e_termination, ut.ut_exit.e_exit,
		ut.ut_session,
		ut.ut_tv.tv_sec, ut.ut_tv.tv_usec,
		ut.ut_addr);
	}
#endif
	return EXIT_SUCCESS;
}
