/* vi: set sw=4 ts=4: */
/*
 * Mini pgrep/pkill implementation for busybox
 *
 * Copyright (C) 2007 Loic Grenie <loic.grenie@gmail.com>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"
#include "xregex.h"

/* Idea taken from kill.c */
#define pgrep (ENABLE_PGREP && applet_name[1] == 'g')
#define pkill (ENABLE_PKILL && applet_name[1] == 'k')

enum {
	/* "vlfxon" */
	PGREPOPTBIT_V = 0, /* must be first, we need OPT_INVERT = 0/1 */
	PGREPOPTBIT_L,
	PGREPOPTBIT_F,
	PGREPOPTBIT_X,
	PGREPOPTBIT_O,
	PGREPOPTBIT_N,
};

#define OPT_INVERT	(opt & (1 << PGREPOPTBIT_V))
#define OPT_LIST	(opt & (1 << PGREPOPTBIT_L))
#define OPT_FULL	(opt & (1 << PGREPOPTBIT_F))
#define OPT_ANCHOR	(opt & (1 << PGREPOPTBIT_X))
#define OPT_FIRST	(opt & (1 << PGREPOPTBIT_O))
#define OPT_LAST	(opt & (1 << PGREPOPTBIT_N))

static void act(unsigned pid, char *cmd, int signo, unsigned opt)
{
	if (pgrep) {
		if (OPT_LIST)
			printf("%d %s\n", pid, cmd);
		else
			printf("%d\n", pid);
	} else
		kill(pid, signo);
}

int pgrep_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int pgrep_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned pid = getpid();
	int signo = SIGTERM;
	unsigned opt;
	int scan_mask = PSSCAN_COMM;
	char *first_arg;
	int first_arg_idx;
	int matched_pid;
	char *cmd_last;
	procps_status_t *proc;
	/* These are initialized to 0 */
	struct {
		regex_t re_buffer;
		regmatch_t re_match[1];
	} Z;
#define re_buffer (Z.re_buffer)
#define re_match  (Z.re_match )

	memset(&Z, 0, sizeof(Z));

	/* We must avoid interpreting -NUM (signal num) as an option */
	first_arg_idx = 1;
	while (1) {
		first_arg = argv[first_arg_idx];
		if (!first_arg)
			break;
		/* not "-<small_letter>..."? */
		if (first_arg[0] != '-' || first_arg[1] < 'a' || first_arg[1] > 'z') {
			argv[first_arg_idx] = NULL; /* terminate argv here */
			break;
		}
		first_arg_idx++;
	}
	opt = getopt32(argv, "vlfxon");
	argv[first_arg_idx] = first_arg;

	argv += optind;
	//argc -= optind; - unused anyway
	if (OPT_FULL)
		scan_mask |= PSSCAN_ARGVN;

	if (pkill) {
		if (OPT_LIST) { /* -l: print the whole signal list */
			print_signames();
			return 0;
		}
		if (first_arg && first_arg[0] == '-') {
			signo = get_signum(&first_arg[1]);
			if (signo < 0) /* || signo > MAX_SIGNUM ? */
				bb_error_msg_and_die("bad signal name '%s'", &first_arg[1]);
			argv++;
		}
	}

	/* One pattern is required */
	if (!argv[0] || argv[1])
		bb_show_usage();

	xregcomp(&re_buffer, argv[0], 0);
	matched_pid = 0;
	cmd_last = NULL;
	proc = NULL;
	while ((proc = procps_scan(proc, scan_mask)) != NULL) {
		char *cmd;
		if (proc->pid == pid)
			continue;
		cmd = proc->argv0;
		if (!cmd) {
			cmd = proc->comm;
		} else {
			int i = proc->argv_len;
			while (i) {
				if (!cmd[i]) cmd[i] = ' ';
				i--;
			}
		}
		/* NB: OPT_INVERT is always 0 or 1 */
		if ((regexec(&re_buffer, cmd, 1, re_match, 0) == 0 /* match found */
		     && (!OPT_ANCHOR || (re_match[0].rm_so == 0 && re_match[0].rm_eo == (regoff_t)strlen(cmd)))) ^ OPT_INVERT
		) {
			matched_pid = proc->pid;
			if (OPT_LAST) {
				free(cmd_last);
				cmd_last = xstrdup(cmd);
				continue;
			}
			act(proc->pid, cmd, signo, opt);
			if (OPT_FIRST)
				break;
		}
	}
	if (cmd_last) {
		act(matched_pid, cmd_last, signo, opt);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(cmd_last);
	}
	return matched_pid == 0; /* return 1 if no processes listed/signaled */
}
