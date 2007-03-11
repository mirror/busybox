/*
Copyright (c) 2001-2006, Gerrit Pape
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Busyboxed by Denis Vlasenko <vda.linux@googlemail.com> */
/* Collected into one file from runit's many tiny files */
/* TODO: review, eliminate unneeded stuff, move good stuff to libbb */

#include <sys/poll.h>
#include <sys/file.h>
#include "libbb.h"
#include "runit_lib.h"

/*** byte_chr.c ***/

unsigned byte_chr(char *s,unsigned n,int c)
{
	char ch;
	char *t;

	ch = c;
	t = s;
	for (;;) {
		if (!n) break;
		if (*t == ch) break;
		++t;
		--n;
	}
	return t - s;
}


/*** coe.c ***/

int coe(int fd)
{
	return fcntl(fd,F_SETFD,FD_CLOEXEC);
}


/*** fd_copy.c ***/

int fd_copy(int to,int from)
{
	if (to == from)
		return 0;
	if (fcntl(from,F_GETFL,0) == -1)
		return -1;
	close(to);
	if (fcntl(from,F_DUPFD,to) == -1)
		return -1;
	return 0;
}


/*** fd_move.c ***/

int fd_move(int to,int from)
{
	if (to == from)
		return 0;
	if (fd_copy(to,from) == -1)
		return -1;
	close(from);
	return 0;
}


/*** fmt_ptime.c ***/

void fmt_ptime30nul(char *s, struct taia *ta) {
	struct tm *t;
	unsigned long u;

	if (ta->sec.x < 4611686018427387914ULL)
		return; /* impossible? */
	u = ta->sec.x -4611686018427387914ULL;
	t = gmtime((time_t*)&u);
	if (!t)
		return; /* huh? */
	//fmt_ulong(s, 1900 + t->tm_year);
	//s[4] = '-'; fmt_uint0(&s[5], t->tm_mon+1, 2);
	//s[7] = '-'; fmt_uint0(&s[8], t->tm_mday, 2);
	//s[10] = '_'; fmt_uint0(&s[11], t->tm_hour, 2);
	//s[13] = ':'; fmt_uint0(&s[14], t->tm_min, 2);
	//s[16] = ':'; fmt_uint0(&s[17], t->tm_sec, 2);
	//s[19] = '.'; fmt_uint0(&s[20], ta->nano, 9);
	sprintf(s, "%04u-%02u-%02u_%02u:%02u:%02u.%09u",
		(unsigned)(1900 + t->tm_year),
		(unsigned)(t->tm_mon+1),
		(unsigned)(t->tm_mday),
		(unsigned)(t->tm_hour),
		(unsigned)(t->tm_min),
		(unsigned)(t->tm_sec),
		(unsigned)(ta->nano)
	);
	/* 4+1 + 2+1 + 2+1 + 2+1 + 2+1 + 2+1 + 9 = */
	/* 5   + 3   + 3   + 3   + 3   + 3   + 9 = */
	/* 20 (up to '.' inclusive) + 9 (not including '\0') */
}

unsigned fmt_taia25(char *s, struct taia *t) {
	static char pack[TAIA_PACK];

	taia_pack(pack, t);
	*s++ = '@';
	bin2hex(s, pack, 12);
	return 25;
}


/*** tai_pack.c ***/

static /* as it isn't used anywhere else */
void tai_pack(char *s,const struct tai *t)
{
	uint64_t x;

	x = t->x;
	s[7] = x & 255; x >>= 8;
	s[6] = x & 255; x >>= 8;
	s[5] = x & 255; x >>= 8;
	s[4] = x & 255; x >>= 8;
	s[3] = x & 255; x >>= 8;
	s[2] = x & 255; x >>= 8;
	s[1] = x & 255; x >>= 8;
	s[0] = x;
}


#ifdef UNUSED
/*** tai_sub.c ***/

void tai_sub(struct tai *t, const struct tai *u, const struct tai *v)
{
	t->x = u->x - v->x;
}
#endif


/*** tai_unpack.c ***/

void tai_unpack(const char *s,struct tai *t)
{
	uint64_t x;

	x = (unsigned char) s[0];
	x <<= 8; x += (unsigned char) s[1];
	x <<= 8; x += (unsigned char) s[2];
	x <<= 8; x += (unsigned char) s[3];
	x <<= 8; x += (unsigned char) s[4];
	x <<= 8; x += (unsigned char) s[5];
	x <<= 8; x += (unsigned char) s[6];
	x <<= 8; x += (unsigned char) s[7];
	t->x = x;
}


/*** taia_add.c ***/

void taia_add(struct taia *t,const struct taia *u,const struct taia *v)
{
	t->sec.x = u->sec.x + v->sec.x;
	t->nano = u->nano + v->nano;
	t->atto = u->atto + v->atto;
	if (t->atto > 999999999UL) {
		t->atto -= 1000000000UL;
		++t->nano;
	}
	if (t->nano > 999999999UL) {
		t->nano -= 1000000000UL;
		++t->sec.x;
	}
}


/*** taia_less.c ***/

int taia_less(const struct taia *t, const struct taia *u)
{
	if (t->sec.x < u->sec.x) return 1;
	if (t->sec.x > u->sec.x) return 0;
	if (t->nano < u->nano) return 1;
	if (t->nano > u->nano) return 0;
	return t->atto < u->atto;
}


/*** taia_now.c ***/

void taia_now(struct taia *t)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	tai_unix(&t->sec, now.tv_sec);
	t->nano = 1000 * now.tv_usec + 500;
	t->atto = 0;
}


/*** taia_pack.c ***/

void taia_pack(char *s, const struct taia *t)
{
	unsigned long x;

	tai_pack(s, &t->sec);
	s += 8;

	x = t->atto;
	s[7] = x & 255; x >>= 8;
	s[6] = x & 255; x >>= 8;
	s[5] = x & 255; x >>= 8;
	s[4] = x;
	x = t->nano;
	s[3] = x & 255; x >>= 8;
	s[2] = x & 255; x >>= 8;
	s[1] = x & 255; x >>= 8;
	s[0] = x;
}


/*** taia_sub.c ***/

void taia_sub(struct taia *t, const struct taia *u, const struct taia *v)
{
	unsigned long unano = u->nano;
	unsigned long uatto = u->atto;

	t->sec.x = u->sec.x - v->sec.x;
	t->nano = unano - v->nano;
	t->atto = uatto - v->atto;
	if (t->atto > uatto) {
		t->atto += 1000000000UL;
		--t->nano;
	}
	if (t->nano > unano) {
		t->nano += 1000000000UL;
		--t->sec.x;
	}
}


/*** taia_uint.c ***/

/* XXX: breaks tai encapsulation */

void taia_uint(struct taia *t, unsigned s)
{
	t->sec.x = s;
	t->nano = 0;
	t->atto = 0;
}


/*** iopause.c ***/

static
uint64_t taia2millisec(const struct taia *t)
{
	return (t->sec.x * 1000) + (t->nano / 1000000);
}


void iopause(iopause_fd *x, unsigned len, struct taia *deadline, struct taia *stamp)
{
	int millisecs;
	int i;

	if (taia_less(deadline, stamp))
		millisecs = 0;
	else {
		uint64_t m;
		struct taia t;
		t = *stamp;
		taia_sub(&t, deadline, &t);
		millisecs = m = taia2millisec(&t);
		if (m > 1000) millisecs = 1000;
		millisecs += 20;
	}

	for (i = 0; i < len; ++i)
		x[i].revents = 0;

	poll(x, len, millisecs);
	/* XXX: some kernels apparently need x[0] even if len is 0 */
	/* XXX: how to handle EAGAIN? are kernels really this dumb? */
	/* XXX: how to handle EINVAL? when exactly can this happen? */
}


/*** lock_ex.c ***/

int lock_ex(int fd)
{
	return flock(fd,LOCK_EX);
}


/*** lock_exnb.c ***/

int lock_exnb(int fd)
{
	return flock(fd,LOCK_EX | LOCK_NB);
}


/*** open_append.c ***/

int open_append(const char *fn)
{
	return open(fn, O_WRONLY|O_NDELAY|O_APPEND|O_CREAT, 0600);
}


/*** open_read.c ***/

int open_read(const char *fn)
{
	return open(fn, O_RDONLY|O_NDELAY);
}


/*** open_trunc.c ***/

int open_trunc(const char *fn)
{
	return open(fn,O_WRONLY | O_NDELAY | O_TRUNC | O_CREAT,0644);
}


/*** open_write.c ***/

int open_write(const char *fn)
{
	return open(fn, O_WRONLY|O_NDELAY);
}


/*** pmatch.c ***/

unsigned pmatch(const char *p, const char *s, unsigned len) {
	for (;;) {
		char c = *p++;
		if (!c) return !len;
		switch (c) {
		case '*':
			if (!(c = *p)) return 1;
			for (;;) {
				if (!len) return 0;
				if (*s == c) break;
				++s; --len;
			}
			continue;
		case '+':
			if ((c = *p++) != *s) return 0;
			for (;;) {
				if (!len) return 1;
				if (*s != c) break;
				++s; --len;
			}
			continue;
			/*
		case '?':
			if (*p == '?') {
				if (*s != '?') return 0;
				++p;
			}
			++s; --len;
			continue;
			*/
		default:
			if (!len) return 0;
			if (*s != c) return 0;
			++s; --len;
			continue;
		}
	}
	return 0;
}


#ifdef UNUSED
/*** seek_set.c ***/

int seek_set(int fd,seek_pos pos)
{
	if (lseek(fd,(off_t) pos,SEEK_SET) == -1) return -1; return 0;
}
#endif


/*** sig_block.c ***/

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

void sig_blocknone(void)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigprocmask(SIG_SETMASK, &ss, NULL);
}


/*** sig_catch.c ***/

void sig_catch(int sig,void (*f)(int))
{
	struct sigaction sa;
	sa.sa_handler = f;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(sig,&sa, NULL);
}


/*** sig_pause.c ***/

void sig_pause(void)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigsuspend(&ss);
}


/*** str_chr.c ***/

unsigned str_chr(const char *s,int c)
{
	char ch;
	const char *t;

	ch = c;
	t = s;
	for (;;) {
		if (!*t) break;
		if (*t == ch) break;
		++t;
	}
	return t - s;
}


/*** wait_nohang.c ***/

int wait_nohang(int *wstat)
{
	return waitpid(-1, wstat, WNOHANG);
}


/*** wait_pid.c ***/

int wait_pid(int *wstat, int pid)
{
	int r;

	do
		r = waitpid(pid, wstat, 0);
	while ((r == -1) && (errno == EINTR));
	return r;
}
