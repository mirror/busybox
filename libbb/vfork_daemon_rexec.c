/*
 * Rexec program for system have fork() as vfork() with foregound option
 * Copyright (C) Vladminr Oleynik and many different people.
 */

#include <unistd.h>
#include "libbb.h"


#if defined(__uClinux__)
void vfork_daemon_rexec(int argc, char **argv, char *foreground_opt)
{
	char **vfork_args;
	int a = 0;

	vfork_args = xcalloc(sizeof(char *), argc + 3);
	while(*argv) {
	    vfork_args[a++] = *argv;
	    argv++;
	}
	vfork_args[a] = foreground_opt;
	execvp("/proc/self/exe", vfork_args);
	vfork_args[0] = "/bin/busybox";
	execv(vfork_args[0], vfork_args);
	bb_perror_msg_and_die("execv %s", vfork_args[0]);
}
#endif /* uClinux */
