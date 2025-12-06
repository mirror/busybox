/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2025 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-y += poll_with_signals.o

#include "libbb.h"

/* Shells, for example, need their line input and "read" builtin
 * to be interruptible, and the naive handling of it a-la:
 *	if (bb_got_signal) {
 *		errno = EINTR;
 *		return -1;
 *	}
 *	poll(pfd, 1, -1); // signal here would set EINTR
 * is racy.
 * This is a bit heavy-handed, but safe wrt races:
 */
int FAST_FUNC check_got_signal_and_poll(struct pollfd pfd[1], int timeout)
{
	int n;
	struct timespec tv;
	sigset_t orig_mask;

	if (bb_got_signal) /* optimization */
		goto eintr;

	if (timeout >= 0) {
		tv.tv_sec = timeout / 1000;
		tv.tv_nsec = (timeout % 1000) * 1000000;
	}
	/* test bb_got_signal, then poll(), atomically wrt signals */
	sigfillset(&orig_mask);
	sigprocmask2(SIG_BLOCK, &orig_mask);
	if (bb_got_signal) {
		sigprocmask2(SIG_SETMASK, &orig_mask);
 eintr:
		errno = EINTR; /* inform the caller that we got a signal */
		return -1;
	}
	n = ppoll(pfd, 1, timeout >= 0 ? &tv : NULL, &orig_mask);
	sigprocmask2(SIG_SETMASK, &orig_mask);
	return n;
}
