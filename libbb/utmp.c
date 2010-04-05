/* vi: set sw=4 ts=4: */
/*
 * utmp/wtmp support routines.
 *
 * Copyright (C) 2010 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <utmp.h>

static void touch(const char *filename)
{
	if (access(filename, R_OK | W_OK) == -1)
		close(open(filename, O_WRONLY | O_CREAT, 0664));
}

/*
 * Read "man utmp" to make sense out of it.
 */
void FAST_FUNC update_utmp(int new_type, const char *short_tty, const char *username, const char *opt_host)
{
	struct utmp utent;
	struct utmp *ut;
	pid_t pid;

	touch(_PATH_UTMP);
	utmpname(_PATH_UTMP);
	setutent();

	pid = getpid();
	/* Did init/getty/telnetd/sshd/... create an entry for us?
	 * It should be (new_type-1), but we'd also reuse
	 * any other potentially stale xxx_PROCESS entry */
	while ((ut = getutent()) != NULL) {
		if (ut->ut_pid == pid
		// && ut->ut_line[0]
		 && ut->ut_id[0] /* must have nonzero id */
		 && (  ut->ut_type == INIT_PROCESS
		    || ut->ut_type == LOGIN_PROCESS
		    || ut->ut_type == USER_PROCESS
		    || ut->ut_type == DEAD_PROCESS
		    )
		) {
			utent = *ut; /* struct copy */
			if (ut->ut_type >= new_type) {
				/* Stale record. Nuke hostname */
				memset(utent.ut_host, 0, sizeof(utent.ut_host));
			}
			/* NB: pututline (see later) searches for matching utent
			 * using getutid(utent) - we must not change ut_id
			 * if we want *exactly this* record to be overwritten!
			 */
			break;
		}
	}
	endutent();

	if (!ut) {
		/* Didn't find anything, create new one */
		memset(&utent, 0, sizeof(utent));
		utent.ut_pid = pid;
		/* Invent our own ut_id. ut_id is only 4 chars wide.
		 * Try to fit something remotely meaningful... */
		if (short_tty[0] == 'p') {
			/* if "ptyXXX", map to "pXXX" */
			/* if "pts/XX", map to "p/XX" */
			utent.ut_id[0] = 'p';
			strncpy(utent.ut_id + 1, short_tty + 3, sizeof(utent.ut_id)-1);
		} else {
			/* assuming it's "ttyXXXX", map to "XXXX" */
			strncpy(utent.ut_id, short_tty + 3, sizeof(utent.ut_id));
		}
	}

	utent.ut_type = new_type;
	safe_strncpy(utent.ut_line, short_tty, sizeof(utent.ut_line));
	safe_strncpy(utent.ut_user, username, sizeof(utent.ut_user));
	if (opt_host)
		safe_strncpy(utent.ut_host, opt_host, sizeof(utent.ut_host));
	utent.ut_tv.tv_sec = time(NULL);

	/* Update, or append new one */
	setutent();
	pututline(&utent);
	endutent();

#if ENABLE_FEATURE_WTMP
	touch(bb_path_wtmp_file);
	updwtmp(bb_path_wtmp_file, &utent);
#endif
}
