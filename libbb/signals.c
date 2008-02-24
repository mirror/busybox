/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

void bb_signals(int sigs, void (*f)(int))
{
	int sig_no = 0;
	int bit = 1;

	while (sigs) {
		if (sigs & bit) {
			sigs &= ~bit;
			signal(sig_no, f);
		}
		sig_no++;
		bit <<= 1;
	}
}

void bb_signals_recursive(int sigs, void (*f)(int))
{
	int sig_no = 0;
	int bit = 1;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = f;
	/*sa.sa_flags = 0;*/
	/*sigemptyset(&sa.sa_mask); - hope memset did it*/

	while (sigs) {
		if (sigs & bit) {
			sigs &= ~bit;
			sigaction(sig_no, &sa, NULL);
		}
		sig_no++;
		bit <<= 1;
	}
}

void sig_block(int sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, sig);
	sigprocmask(SIG_BLOCK, &ss, NULL);
}

void sig_unblock(int sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, sig);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);
}

#if 0
void sig_blocknone(void)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigprocmask(SIG_SETMASK, &ss, NULL);
}
#endif

void sig_pause(void)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigsuspend(&ss);
}

/* Assuming the sig is fatal */
void kill_myself_with_sig(int sig)
{
	sigset_t set;

	signal(sig, SIG_DFL);

	sigemptyset(&set);
	sigaddset(&set, sig);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	raise(sig);
	_exit(1); /* Should not reach it */
}
