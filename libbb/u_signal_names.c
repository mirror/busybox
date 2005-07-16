/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.  If you wrote this, please
 * acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct signal_name {
	const char *name;
	int number;
};

static const struct signal_name signames[] = {
	/* POSIX signals */
	{ "EXIT",       0 },            /* 0 */
	{ "HUP",        SIGHUP },       /* 1 */
	{ "INT",        SIGINT },       /* 2 */
	{ "QUIT",       SIGQUIT },      /* 3 */
	{ "ILL",        SIGILL },       /* 4 */
	{ "ABRT",       SIGABRT },      /* 6 */
	{ "FPE",        SIGFPE },       /* 8 */
	{ "KILL",       SIGKILL },      /* 9 */
	{ "SEGV",       SIGSEGV },      /* 11 */
	{ "PIPE",       SIGPIPE },      /* 13 */
	{ "ALRM",       SIGALRM },      /* 14 */
	{ "TERM",       SIGTERM },      /* 15 */
	{ "USR1",       SIGUSR1 },      /* 10 (arm,i386,m68k,ppc), 30 (alpha,sparc*), 16 (mips) */
	{ "USR2",       SIGUSR2 },      /* 12 (arm,i386,m68k,ppc), 31 (alpha,sparc*), 17 (mips) */
	{ "CHLD",       SIGCHLD },      /* 17 (arm,i386,m68k,ppc), 20 (alpha,sparc*), 18 (mips) */
	{ "CONT",       SIGCONT },      /* 18 (arm,i386,m68k,ppc), 19 (alpha,sparc*), 25 (mips) */
	{ "STOP",       SIGSTOP },      /* 19 (arm,i386,m68k,ppc), 17 (alpha,sparc*), 23 (mips) */
	{ "TSTP",       SIGTSTP },      /* 20 (arm,i386,m68k,ppc), 18 (alpha,sparc*), 24 (mips) */
	{ "TTIN",       SIGTTIN },      /* 21 (arm,i386,m68k,ppc,alpha,sparc*), 26 (mips) */
	{ "TTOU",       SIGTTOU },      /* 22 (arm,i386,m68k,ppc,alpha,sparc*), 27 (mips) */
	/* Miscellaneous other signals */
#ifdef SIGTRAP
	{ "TRAP",       SIGTRAP },      /* 5 */
#endif
#ifdef SIGIOT
	{ "IOT",        SIGIOT },       /* 6, same as SIGABRT */
#endif
#ifdef SIGEMT
	{ "EMT",        SIGEMT },       /* 7 (mips,alpha,sparc*) */
#endif
#ifdef SIGBUS
	{ "BUS",        SIGBUS },       /* 7 (arm,i386,m68k,ppc), 10 (mips,alpha,sparc*) */
#endif
#ifdef SIGSYS
	{ "SYS",        SIGSYS },       /* 12 (mips,alpha,sparc*) */
#endif
#ifdef SIGSTKFLT
	{ "STKFLT",     SIGSTKFLT },    /* 16 (arm,i386,m68k,ppc) */
#endif
#ifdef SIGURG
	{ "URG",        SIGURG },       /* 23 (arm,i386,m68k,ppc), 16 (alpha,sparc*), 21 (mips) */
#endif
#ifdef SIGIO
	{ "IO",         SIGIO },        /* 29 (arm,i386,m68k,ppc), 23 (alpha,sparc*), 22 (mips) */
#endif
#ifdef SIGPOLL
	{ "POLL",       SIGPOLL },      /* same as SIGIO */
#endif
#ifdef SIGCLD
	{ "CLD",        SIGCLD },       /* same as SIGCHLD (mips) */
#endif
#ifdef SIGXCPU
	{ "XCPU",       SIGXCPU },      /* 24 (arm,i386,m68k,ppc,alpha,sparc*), 30 (mips) */
#endif
#ifdef SIGXFSZ
	{ "XFSZ",       SIGXFSZ },      /* 25 (arm,i386,m68k,ppc,alpha,sparc*), 31 (mips) */
#endif
#ifdef SIGVTALRM
	{ "VTALRM",     SIGVTALRM },    /* 26 (arm,i386,m68k,ppc,alpha,sparc*), 28 (mips) */
#endif
#ifdef SIGPROF
	{ "PROF",       SIGPROF },      /* 27 (arm,i386,m68k,ppc,alpha,sparc*), 29 (mips) */
#endif
#ifdef SIGPWR
	{ "PWR",        SIGPWR },       /* 30 (arm,i386,m68k,ppc), 29 (alpha,sparc*), 19 (mips) */
#endif
#ifdef SIGINFO
	{ "INFO",       SIGINFO },      /* 29 (alpha) */
#endif
#ifdef SIGLOST
	{ "LOST",       SIGLOST },      /* 29 (arm,i386,m68k,ppc,sparc*) */
#endif
#ifdef SIGWINCH
	{ "WINCH",      SIGWINCH },     /* 28 (arm,i386,m68k,ppc,alpha,sparc*), 20 (mips) */
#endif
#ifdef SIGUNUSED
	{ "UNUSED",     SIGUNUSED },    /* 31 (arm,i386,m68k,ppc) */
#endif
	{0, 0}
};

/*
	if str_sig == NULL returned signal name [*signo],
	if str_sig != NULL - set *signo from signal_name,
		findings with digit number or with or without SIG-prefix name

	if startnum=0 flag for support finding zero signal,
		but str_sig="0" always found, (hmm - standart or realize?)
	if startnum<0 returned reverse signal_number  <-> signal_name
	if found error - returned NULL

*/

const char *
u_signal_names(const char *str_sig, int *signo, int startnum)
{
	static char retstr[16];
	const struct signal_name *s = signames;
	static const char prefix[] = "SIG";
	const char *sptr;

	if(startnum)
		s++;
	if(str_sig==NULL) {
		while (s->name != 0) {
			if(s->number == *signo)
				break;
			s++;
		}
	} else {
		if (isdigit(((unsigned char)*str_sig))) {
			char *endp;
			long int sn = strtol(str_sig, &endp, 10);
			/* test correct and overflow */
			if(*endp == 0 && sn >= 0 && sn < NSIG) {
				*signo = (int)sn;
				/* test for unnamed */
				sptr = u_signal_names(0, signo, 0);
				if(sptr==NULL)
					return NULL;
				if(sn!=0)
					sptr += 3;
				return sptr;
			}
		} else {
			sptr = str_sig;
			while (s->name != 0) {
				if (strcasecmp(s->name, sptr) == 0) {
					*signo = s->number;
					if(startnum<0) {
						sprintf(retstr, "%d", *signo);
						return retstr;
					}
					break;
				}
				if(s!=signames && sptr == str_sig &&
						strncasecmp(sptr, prefix, 3) == 0) {
					sptr += 3;      /* strlen(prefix) */
					continue;
				}
				sptr = str_sig;
				s++;
			}
		}
	}
	if(s->name==0)
		return NULL;
	if(s!=signames)
		strcpy(retstr, prefix);
	 else
		retstr[0] = 0;
	return strcat(retstr, s->name);
}
