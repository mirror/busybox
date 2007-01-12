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

/*** buffer.c ***/

void buffer_init(buffer *s,int (*op)(int fd,char *buf,unsigned len),int fd,char *buf,unsigned len)
{
	s->x = buf;
	s->fd = fd;
	s->op = op;
	s->p = 0;
	s->n = len;
}


/*** buffer_get.c ***/

static int oneread(int (*op)(int fd,char *buf,unsigned len),int fd,char *buf,unsigned len)
{
	int r;

	for (;;) {
		r = op(fd,buf,len);
		if (r == -1) if (errno == EINTR) continue;
		return r;
	}
}

static int getthis(buffer *s,char *buf,unsigned len)
{
	if (len > s->p) len = s->p;
	s->p -= len;
	memcpy(buf,s->x + s->n,len);
	s->n += len;
	return len;
}

int buffer_feed(buffer *s)
{
	int r;

	if (s->p) return s->p;
	r = oneread(s->op,s->fd,s->x,s->n);
	if (r <= 0) return r;
	s->p = r;
	s->n -= r;
	if (s->n > 0) memmove(s->x + s->n,s->x,r);
	return r;
}

int buffer_bget(buffer *s,char *buf,unsigned len)
{
	int r;

	if (s->p > 0) return getthis(s,buf,len);
	if (s->n <= len) return oneread(s->op,s->fd,buf,s->n);
	r = buffer_feed(s); if (r <= 0) return r;
	return getthis(s,buf,len);
}

int buffer_get(buffer *s,char *buf,unsigned len)
{
	int r;

	if (s->p > 0) return getthis(s,buf,len);
	if (s->n <= len) return oneread(s->op,s->fd,buf,len);
	r = buffer_feed(s); if (r <= 0) return r;
	return getthis(s,buf,len);
}

char *buffer_peek(buffer *s)
{
	return s->x + s->n;
}

void buffer_seek(buffer *s,unsigned len)
{
	s->n += len;
	s->p -= len;
}


/*** buffer_put.c ***/

static int allwrite(int (*op)(int fd,char *buf,unsigned len),int fd,const char *buf,unsigned len)
{
	int w;

	while (len) {
		w = op(fd,(char*)buf,len);
		if (w == -1) {
			if (errno == EINTR) continue;
			return -1; /* note that some data may have been written */
		}
		if (w == 0) ; /* luser's fault */
		buf += w;
		len -= w;
	}
	return 0;
}

int buffer_flush(buffer *s)
{
	int p;

	p = s->p;
	if (!p) return 0;
	s->p = 0;
	return allwrite(s->op,s->fd,s->x,p);
}

int buffer_putalign(buffer *s,const char *buf,unsigned len)
{
	unsigned n;

	while (len > (n = s->n - s->p)) {
		memcpy(s->x + s->p,buf,n);
		s->p += n;
		buf += n;
		len -= n;
		if (buffer_flush(s) == -1) return -1;
	}
	/* now len <= s->n - s->p */
	memcpy(s->x + s->p,buf,len);
	s->p += len;
	return 0;
}

int buffer_put(buffer *s,const char *buf,unsigned len)
{
	unsigned n;

	n = s->n;
	if (len > n - s->p) {
		if (buffer_flush(s) == -1) return -1;
		/* now s->p == 0 */
		if (n < BUFFER_OUTSIZE) n = BUFFER_OUTSIZE;
		while (len > s->n) {
			if (n > len) n = len;
			if (allwrite(s->op,s->fd,buf,n) == -1) return -1;
			buf += n;
			len -= n;
		}
	}
	/* now len <= s->n - s->p */
	memcpy(s->x + s->p,buf,len);
	s->p += len;
	return 0;
}

int buffer_putflush(buffer *s,const char *buf,unsigned len)
{
	if (buffer_flush(s) == -1) return -1;
	return allwrite(s->op,s->fd,buf,len);
}

int buffer_putsalign(buffer *s,const char *buf)
{
	return buffer_putalign(s,buf,strlen(buf));
}

int buffer_puts(buffer *s,const char *buf)
{
	return buffer_put(s,buf,strlen(buf));
}

int buffer_putsflush(buffer *s,const char *buf)
{
	return buffer_putflush(s,buf,strlen(buf));
}


/*** buffer_read.c ***/

int buffer_unixread(int fd,char *buf,unsigned len)
{
	return read(fd,buf,len);
}


/*** buffer_write.c ***/

int buffer_unixwrite(int fd,char *buf,unsigned len)
{
	return write(fd,buf,len);
}


/*** byte_chr.c ***/

unsigned byte_chr(char *s,unsigned n,int c)
{
	char ch;
	char *t;

	ch = c;
	t = s;
	for (;;) {
		if (!n) break; if (*t == ch) break; ++t; --n;
		if (!n) break; if (*t == ch) break; ++t; --n;
		if (!n) break; if (*t == ch) break; ++t; --n;
		if (!n) break; if (*t == ch) break; ++t; --n;
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
	if (to == from) return 0;
	if (fcntl(from,F_GETFL,0) == -1) return -1;
	close(to);
	if (fcntl(from,F_DUPFD,to) == -1) return -1;
	return 0;
}


/*** fd_move.c ***/

int fd_move(int to,int from)
{
	if (to == from) return 0;
	if (fd_copy(to,from) == -1) return -1;
	close(from);
	return 0;
}


/*** fifo.c ***/

int fifo_make(const char *fn,int mode) { return mkfifo(fn,mode); }


/*** fmt_ptime.c ***/

unsigned fmt_ptime(char *s, struct taia *ta) {
	struct tm *t;
	unsigned long u;

	if (ta->sec.x < 4611686018427387914ULL) return 0; /* impossible? */
	u = ta->sec.x -4611686018427387914ULL;
	if (!(t = gmtime((time_t*)&u))) return 0;
	fmt_ulong(s, 1900 + t->tm_year);
	s[4] = '-'; fmt_uint0(&s[5], t->tm_mon+1, 2);
	s[7] = '-'; fmt_uint0(&s[8], t->tm_mday, 2);
	s[10] = '_'; fmt_uint0(&s[11], t->tm_hour, 2);
	s[13] = ':'; fmt_uint0(&s[14], t->tm_min, 2);
	s[16] = ':'; fmt_uint0(&s[17], t->tm_sec, 2);
	s[19] = '.'; fmt_uint0(&s[20], ta->nano, 9);
	return 25;
}

unsigned fmt_taia(char *s, struct taia *t) {
	static char pack[TAIA_PACK];

	taia_pack(pack, t);
	*s++ = '@';
	bin2hex(s, pack, 12);
	return 25;
}


/*** fmt_uint.c ***/

unsigned fmt_uint(char *s,unsigned u)
{
	return fmt_ulong(s,u);
}


/*** fmt_uint0.c ***/

unsigned fmt_uint0(char *s,unsigned u,unsigned n)
{
	unsigned len;
	len = fmt_uint(FMT_LEN,u);
	while (len < n) { if (s) *s++ = '0'; ++len; }
	if (s) fmt_uint(s,u);
	return len;
}


/*** fmt_ulong.c ***/

unsigned fmt_ulong(char *s,unsigned long u)
{
	unsigned len; unsigned long q;
	len = 1; q = u;
	while (q > 9) { ++len; q /= 10; }
	if (s) {
		s += len;
		do { *--s = '0' + (u % 10); u /= 10; } while (u); /* handles u == 0 */
	}
	return len;
}


/*** tai_now.c ***/

void tai_now(struct tai *t)
{
	tai_unix(t,time((time_t *) 0));
}


/*** tai_pack.c ***/

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


/*** tai_sub.c ***/

void tai_sub(struct tai *t,const struct tai *u,const struct tai *v)
{
	t->x = u->x - v->x;
}


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

/* XXX: breaks tai encapsulation */

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


/*** taia_approx.c ***/

double taia_approx(const struct taia *t)
{
	return tai_approx(&t->sec) + taia_frac(t);
}


/*** taia_frac.c ***/

double taia_frac(const struct taia *t)
{
	return (t->atto * 0.000000001 + t->nano) * 0.000000001;
}


/*** taia_less.c ***/

/* XXX: breaks tai encapsulation */

int taia_less(const struct taia *t,const struct taia *u)
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
	gettimeofday(&now,(struct timezone *) 0);
	tai_unix(&t->sec,now.tv_sec);
	t->nano = 1000 * now.tv_usec + 500;
	t->atto = 0;
}


/*** taia_pack.c ***/

void taia_pack(char *s,const struct taia *t)
{
	unsigned long x;

	tai_pack(s,&t->sec);
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

/* XXX: breaks tai encapsulation */

void taia_sub(struct taia *t,const struct taia *u,const struct taia *v)
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

void taia_uint(struct taia *t,unsigned s)
{
	t->sec.x = s;
	t->nano = 0;
	t->atto = 0;
}


/*** stralloc_cat.c ***/
#if 0

int stralloc_cat(stralloc *sato,const stralloc *safrom)
{
	return stralloc_catb(sato,safrom->s,safrom->len);
}


/*** stralloc_catb.c ***/

int stralloc_catb(stralloc *sa,const char *s,unsigned n)
{
	if (!sa->s) return stralloc_copyb(sa,s,n);
	if (!stralloc_readyplus(sa,n + 1)) return 0;
	memcpy(sa->s + sa->len,s,n);
	sa->len += n;
	sa->s[sa->len] = 'Z'; /* ``offensive programming'' */
	return 1;
}


/*** stralloc_cats.c ***/

int stralloc_cats(stralloc *sa,const char *s)
{
	return stralloc_catb(sa,s,strlen(s));
}


/*** stralloc_eady.c ***/

GEN_ALLOC_ready(stralloc,char,s,len,a,i,n,x,30,stralloc_ready)
GEN_ALLOC_readyplus(stralloc,char,s,len,a,i,n,x,30,stralloc_readyplus)


/*** stralloc_opyb.c ***/

int stralloc_copyb(stralloc *sa,const char *s,unsigned n)
{
	if (!stralloc_ready(sa,n + 1)) return 0;
	memcpy(sa->s,s,n);
	sa->len = n;
	sa->s[n] = 'Z'; /* ``offensive programming'' */
	return 1;
}


/*** stralloc_opys.c ***/

int stralloc_copys(stralloc *sa,const char *s)
{
	return stralloc_copyb(sa,s,strlen(s));
}


/*** stralloc_pend.c ***/

GEN_ALLOC_append(stralloc,char,s,len,a,i,n,x,30,stralloc_readyplus,stralloc_append)

#endif /* stralloc */

/*** iopause.c ***/

void iopause(iopause_fd *x,unsigned len,struct taia *deadline,struct taia *stamp)
{
	struct taia t;
	int millisecs;
	double d;
	int i;

	if (taia_less(deadline,stamp))
		millisecs = 0;
	else {
		t = *stamp;
		taia_sub(&t,deadline,&t);
		d = taia_approx(&t);
		if (d > 1000.0) d = 1000.0;
		millisecs = d * 1000.0 + 20.0;
	}

	for (i = 0;i < len;++i)
		x[i].revents = 0;

	poll(x,len,millisecs);
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


/*** openreadclose.c ***/
#if 0
int openreadclose(const char *fn,stralloc *sa,unsigned bufsize)
{
	int fd;
	fd = open_read(fn);
	if (fd == -1) {
		if (errno == ENOENT) return 0;
		return -1;
	}
	if (readclose(fd,sa,bufsize) == -1) return -1;
	return 1;
}
#endif


/*** pathexec_env.c ***/
#if 0
static stralloc plus;
static stralloc tmp;

int pathexec_env(const char *s,const char *t)
{
	if (!s) return 1;
	if (!stralloc_copys(&tmp,s)) return 0;
	if (t) {
		if (!stralloc_cats(&tmp,"=")) return 0;
		if (!stralloc_cats(&tmp,t)) return 0;
	}
	if (!stralloc_0(&tmp)) return 0;
	return stralloc_cat(&plus,&tmp);
}

void pathexec(char **argv)
{
	char **e;
	unsigned elen;
	unsigned i;
	unsigned j;
	unsigned split;
	unsigned t;

	if (!stralloc_cats(&plus,"")) return;

	elen = 0;
	for (i = 0;environ[i];++i)
		++elen;
	for (i = 0;i < plus.len;++i)
		if (!plus.s[i])
			++elen;

	e = malloc((elen + 1) * sizeof(char *));
	if (!e) return;

	elen = 0;
	for (i = 0;environ[i];++i)
		e[elen++] = environ[i];

	j = 0;
	for (i = 0;i < plus.len;++i)
		if (!plus.s[i]) {
			split = str_chr(plus.s + j,'=');
			for (t = 0;t < elen;++t)
				if (memcmp(plus.s + j,e[t],split) == 0)
					if (e[t][split] == '=') {
						--elen;
						e[t] = e[elen];
						break;
					}
			if (plus.s[j + split])
				e[elen++] = plus.s + j;
			j = i + 1;
		}
	e[elen] = 0;

	pathexec_run(*argv,argv,e);
	free(e);
}
#endif

/*** pathexec_run.c ***/
#if 0
static stralloc tmp;

void pathexec_run(const char *file,char *const *argv,char *const *envp)
{
	const char *path;
	unsigned split;
	int savederrno;

	if (file[str_chr(file,'/')]) {
		execve(file,argv,envp);
		return;
	}

	path = getenv("PATH");
	if (!path) path = "/bin:/usr/bin";

	savederrno = 0;
	for (;;) {
		split = str_chr(path,':');
		if (!stralloc_copyb(&tmp,path,split)) return;
		if (!split)
			if (!stralloc_cats(&tmp,".")) return;
		if (!stralloc_cats(&tmp,"/"))  return;
		if (!stralloc_cats(&tmp,file)) return;
		if (!stralloc_0(&tmp)) return;

		execve(tmp.s,argv,envp);
		if (errno != ENOENT) {
			savederrno = errno;
			if ((errno != EACCES) && (errno != EPERM) && (errno != EISDIR)) return;
		}

		if (!path[split]) {
			if (savederrno) errno = savederrno;
			return;
		}
		path += split;
		path += 1;
	}
}
#endif

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


/*** prot.c ***/

int prot_gid(int gid)
{
	gid_t x = gid;
	if (setgroups(1,&x) == -1) return -1;
	return setgid(gid); /* _should_ be redundant, but on some systems it isn't */
}

int prot_uid(int uid)
{
	return setuid(uid);
}


/*** readclose.c ***/
#if 0
int readclose_append(int fd,stralloc *sa,unsigned bufsize)
{
	int r;
	for (;;) {
		if (!stralloc_readyplus(sa,bufsize)) { close(fd); return -1; }
		r = read(fd,sa->s + sa->len,bufsize);
		if (r == -1) if (errno == EINTR) continue;
		if (r <= 0) { close(fd); return r; }
		sa->len += r;
	}
}

int readclose(int fd,stralloc *sa,unsigned bufsize)
{
	if (!stralloc_copys(sa,"")) { close(fd); return -1; }
	return readclose_append(fd,sa,bufsize);
}
#endif

/*** scan_ulong.c ***/

unsigned scan_ulong(const char *s,unsigned long *u)
{
	unsigned pos = 0;
	unsigned long result = 0;
	unsigned long c;
	while ((c = (unsigned long) (unsigned char) (s[pos] - '0')) < 10) {
		result = result * 10 + c;
		++pos;
	}
	*u = result;
	return pos;
}


/*** seek_set.c ***/

int seek_set(int fd,seek_pos pos)
{
	if (lseek(fd,(off_t) pos,SEEK_SET) == -1) return -1; return 0;
}


/*** sig.c ***/

int sig_alarm = SIGALRM;
int sig_child = SIGCHLD;
int sig_cont = SIGCONT;
int sig_hangup = SIGHUP;
int sig_int = SIGINT;
int sig_pipe = SIGPIPE;
int sig_term = SIGTERM;

void (*sig_defaulthandler)(int) = SIG_DFL;
void (*sig_ignorehandler)(int) = SIG_IGN;


/*** sig_block.c ***/

void sig_block(int sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss,sig);
	sigprocmask(SIG_BLOCK,&ss,(sigset_t *) 0);
}

void sig_unblock(int sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss,sig);
	sigprocmask(SIG_UNBLOCK,&ss,(sigset_t *) 0);
}

void sig_blocknone(void)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigprocmask(SIG_SETMASK,&ss,(sigset_t *) 0);
}


/*** sig_catch.c ***/

void sig_catch(int sig,void (*f)(int))
{
	struct sigaction sa;
	sa.sa_handler = f;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(sig,&sa,(struct sigaction *) 0);
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
		if (!*t) break; if (*t == ch) break; ++t;
		if (!*t) break; if (*t == ch) break; ++t;
		if (!*t) break; if (*t == ch) break; ++t;
		if (!*t) break; if (*t == ch) break; ++t;
	}
	return t - s;
}


/*** wait_nohang.c ***/

int wait_nohang(int *wstat)
{
	return waitpid(-1,wstat,WNOHANG);
}


/*** wait_pid.c ***/

int wait_pid(int *wstat, int pid)
{
	int r;

	do
		r = waitpid(pid,wstat,0);
	while ((r == -1) && (errno == EINTR));
	return r;
}
