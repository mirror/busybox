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
#include <utmp.h>

const char      dutmp_usage[] = "dutmp\n"
"\n"
"\tDump file or stdin utmp file format to stdout, pipe delimited.\n"
"\tdutmp /var/run/utmp\n";

extern int
dutmp_fn(const struct FileInfo * i)
{

FILE *	f = stdin;
struct utmp * ut = (struct utmp *) malloc(sizeof(struct utmp) );

	if ( i ) 
		if (! (f = fopen(i->source, "r"))) {
			name_and_error(i->source);
			return 1;
		}

	while (fread (ut, 1, sizeof(struct utmp), f)) {
		//printf("%d:%d:%s:%s:%s:%s:%d:%d:%ld:%ld:%ld:%x\n", 
		printf("%d|%d|%s|%s|%s|%s|%d|%d|%ld|%ld|%ld|%x\n",
		ut->ut_type, ut->ut_pid, ut->ut_line,
		ut->ut_id, ut->ut_user,	ut->ut_host,
		ut->ut_exit.e_termination, ut->ut_exit.e_exit,
		ut->ut_session,
		ut->ut_tv.tv_sec, ut->ut_tv.tv_usec,
		ut->ut_addr);
	}

return 0;	
}
