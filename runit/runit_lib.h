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

/*** byte.h ***/

extern unsigned byte_chr(char *s,unsigned n,int c);


/*** coe.h ***/

extern int coe(int);


/*** direntry.h ***/

#define direntry struct dirent


/*** fd.h ***/

extern int fd_copy(int,int);
extern int fd_move(int,int);


/*** tai.h ***/

struct tai {
	uint64_t x;
};

#define tai_unix(t,u) ((void) ((t)->x = 4611686018427387914ULL + (uint64_t) (u)))

#define TAI_PACK 8
//extern void tai_pack(char *,const struct tai *);
extern void tai_unpack(const char *,struct tai *);

extern void tai_uint(struct tai *,unsigned);


/*** taia.h ***/

struct taia {
	struct tai sec;
	unsigned long nano; /* 0...999999999 */
	unsigned long atto; /* 0...999999999 */
};

//extern void taia_tai(const struct taia *,struct tai *);

extern void taia_now(struct taia *);

extern void taia_add(struct taia *,const struct taia *,const struct taia *);
extern void taia_addsec(struct taia *,const struct taia *,int);
extern void taia_sub(struct taia *,const struct taia *,const struct taia *);
extern void taia_half(struct taia *,const struct taia *);
extern int taia_less(const struct taia *,const struct taia *);

#define TAIA_PACK 16
extern void taia_pack(char *,const struct taia *);
//extern void taia_unpack(const char *,struct taia *);

//#define TAIA_FMTFRAC 19
//extern unsigned taia_fmtfrac(char *,const struct taia *);

extern void taia_uint(struct taia *,unsigned);


/*** fmt_ptime.h ***/

#define FMT_PTIME 30

/* NUL terminated */
extern void fmt_ptime30nul(char *, struct taia *);
/* NOT terminated! */
extern unsigned fmt_taia25(char *, struct taia *);


/*** iopause.h ***/

typedef struct pollfd iopause_fd;
#define IOPAUSE_READ POLLIN
#define IOPAUSE_WRITE POLLOUT

extern void iopause(iopause_fd *,unsigned,struct taia *,struct taia *);


/*** lock.h ***/

extern int lock_ex(int);
extern int lock_un(int);
extern int lock_exnb(int);


/*** open.h ***/

extern int open_read(const char *);
extern int open_excl(const char *);
extern int open_append(const char *);
extern int open_trunc(const char *);
extern int open_write(const char *);


/*** pmatch.h ***/

extern unsigned pmatch(const char *, const char *, unsigned);


/*** sig.h ***/

extern void sig_catch(int,void (*)(int));
#define sig_ignore(s) (sig_catch((s), SIG_IGN))
#define sig_uncatch(s) (sig_catch((s), SIG_DFL))

extern void sig_block(int);
extern void sig_unblock(int);
extern void sig_blocknone(void);
extern void sig_pause(void);


/*** str.h ***/

extern unsigned str_chr(const char *,int);  /* never returns NULL */

#define str_diff(s,t) strcmp((s), (t))
#define str_equal(s,t) (!strcmp((s), (t)))


/*** wait.h ***/

extern int wait_pid(int *wstat, int pid);
extern int wait_nohang(int *wstat);

#define wait_crashed(w) ((w) & 127)
#define wait_exitcode(w) ((w) >> 8)
#define wait_stopsig(w) ((w) >> 8)
#define wait_stopped(w) (((w) & 127) == 127)
