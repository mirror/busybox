#include <signal.h>
#include "busybox.h"

#include "init_shared.h"


extern int kill_init(int sig)
{
#ifdef CONFIG_FEATURE_INITRD
	/* don't assume init's pid == 1 */
	long *pid = find_pid_by_name("init");
	if (!pid || *pid<=0) {
		pid = find_pid_by_name("linuxrc");
		if (!pid || *pid<=0)
			bb_error_msg_and_die("no process killed");
	}
	return(kill(*pid, sig));
#else
	return(kill(1, sig));
#endif
}
