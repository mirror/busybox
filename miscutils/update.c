#include "internal.h"
#include <linux/unistd.h>

const char	update_usage[] = "update\n"
"\n"
"\tFlush buffered data to the disk devices every 30 seconds.\n";

#if defined(__GLIBC__)
#include <sys/kdaemon.h>
#else
_syscall2(int, bdflush, int, func, int, data);
#endif /* __GLIBC__ */

extern int
update_main(struct FileInfo * i, int argc, char * * argv)
{
	/*
	 * Update is actually two daemons, bdflush and update.
	 */
	int	pid;

	pid = fork();
	if ( pid < 0 )
		return pid;
	else if ( pid == 0 ) {
		/*
		 * This is no longer necessary since 1.3.5x, but it will harmlessly
		 * exit if that is the case.
		 */
		strcpy(argv[0], "bdflush (update)");
		argv[1] = 0;
		argv[2] = 0;
		bdflush(1, 0);
		_exit(0);
	}
	pid = fork();
	if ( pid < 0 )
		return pid;
	else if ( pid == 0 ) {
		argv[0] = "update";
		for ( ; ; ) {
			sync();
			sleep(30);
		}
	}

	return 0;
}
