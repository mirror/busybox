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

/*** buffer.h ***/

typedef struct buffer {
	char *x;
	unsigned p;
	unsigned n;
	int fd;
	int (*op)(int fd,char *buf,unsigned len);
} buffer;

#define BUFFER_INIT(op,fd,buf,len) { (buf), 0, (len), (fd), (op) }
#define BUFFER_INSIZE 8192
#define BUFFER_OUTSIZE 8192

extern void buffer_init(buffer *,int (*)(int fd,char *buf,unsigned len),int,char *,unsigned);

extern int buffer_flush(buffer *);
extern int buffer_put(buffer *,const char *,unsigned);
extern int buffer_putalign(buffer *,const char *,unsigned);
extern int buffer_putflush(buffer *,const char *,unsigned);
extern int buffer_puts(buffer *,const char *);
extern int buffer_putsalign(buffer *,const char *);
extern int buffer_putsflush(buffer *,const char *);

#define buffer_PUTC(s,c) \
	( ((s)->n != (s)->p) \
		? ( (s)->x[(s)->p++] = (c), 0 ) \
		: buffer_put((s),&(c),1) \
	)

extern int buffer_get(buffer *,char *,unsigned);
extern int buffer_bget(buffer *,char *,unsigned);
extern int buffer_feed(buffer *);

extern char *buffer_peek(buffer *);
extern void buffer_seek(buffer *,unsigned);

#define buffer_PEEK(s) ( (s)->x + (s)->n )
#define buffer_SEEK(s,len) ( ( (s)->p -= (len) ) , ( (s)->n += (len) ) )

#define buffer_GETC(s,c) \
	( ((s)->p > 0) \
		? ( *(c) = (s)->x[(s)->n], buffer_SEEK((s),1), 1 ) \
		: buffer_get((s),(c),1) \
	)

extern int buffer_copy(buffer *,buffer *);

extern int buffer_unixread(int,char *,unsigned);
/* Actually, int buffer_unixwrite(int,const char *,unsigned),
	 but that 'const' will produce warnings... oh well */
extern int buffer_unixwrite(int,char *,unsigned);


/*** byte.h ***/

extern unsigned byte_chr(char *s,unsigned n,int c);


/*** coe.h ***/

extern int coe(int);


/*** direntry.h ***/

#define direntry struct dirent


/*** fd.h ***/

extern int fd_copy(int,int);
extern int fd_move(int,int);


/*** fifo.h ***/

extern int fifo_make(const char *,int);


/*** fmt.h ***/

#define FMT_ULONG 40 /* enough space to hold 2^128 - 1 in decimal, plus \0 */
#define FMT_LEN ((char *) 0) /* convenient abbreviation */

extern unsigned fmt_uint(char *,unsigned);
extern unsigned fmt_uint0(char *,unsigned,unsigned);
extern unsigned fmt_xint(char *,unsigned);
extern unsigned fmt_nbbint(char *,unsigned,unsigned,unsigned,unsigned);
extern unsigned fmt_ushort(char *,unsigned short);
extern unsigned fmt_xshort(char *,unsigned short);
extern unsigned fmt_nbbshort(char *,unsigned,unsigned,unsigned,unsigned short);
extern unsigned fmt_ulong(char *,unsigned long);
extern unsigned fmt_xlong(char *,unsigned long);
extern unsigned fmt_nbblong(char *,unsigned,unsigned,unsigned,unsigned long);

extern unsigned fmt_plusminus(char *,int);
extern unsigned fmt_minus(char *,int);
extern unsigned fmt_0x(char *,int);

extern unsigned fmt_str(char *,const char *);
extern unsigned fmt_strn(char *,const char *,unsigned);


/*** tai.h ***/

struct tai {
	uint64_t x;
} ;

#define tai_unix(t,u) ((void) ((t)->x = 4611686018427387914ULL + (uint64_t) (u)))

extern void tai_now(struct tai *);

#define tai_approx(t) ((double) ((t)->x))

extern void tai_add(struct tai *,const struct tai *,const struct tai *);
extern void tai_sub(struct tai *,const struct tai *,const struct tai *);
#define tai_less(t,u) ((t)->x < (u)->x)

#define TAI_PACK 8
extern void tai_pack(char *,const struct tai *);
extern void tai_unpack(const char *,struct tai *);

extern void tai_uint(struct tai *,unsigned);


/*** taia.h ***/

struct taia {
	struct tai sec;
	unsigned long nano; /* 0...999999999 */
	unsigned long atto; /* 0...999999999 */
} ;

extern void taia_tai(const struct taia *,struct tai *);

extern void taia_now(struct taia *);

extern double taia_approx(const struct taia *);
extern double taia_frac(const struct taia *);

extern void taia_add(struct taia *,const struct taia *,const struct taia *);
extern void taia_addsec(struct taia *,const struct taia *,int);
extern void taia_sub(struct taia *,const struct taia *,const struct taia *);
extern void taia_half(struct taia *,const struct taia *);
extern int taia_less(const struct taia *,const struct taia *);

#define TAIA_PACK 16
extern void taia_pack(char *,const struct taia *);
extern void taia_unpack(const char *,struct taia *);

#define TAIA_FMTFRAC 19
extern unsigned taia_fmtfrac(char *,const struct taia *);

extern void taia_uint(struct taia *,unsigned);


/*** fmt_ptime.h ***/

#define FMT_PTIME 30

extern unsigned fmt_ptime(char *, struct taia *);
extern unsigned fmt_taia(char *, struct taia *);


/*** gen_alloc.h ***/

#define GEN_ALLOC_typedef(ta,type,field,len,a) \
	typedef struct ta { type *field; unsigned len; unsigned a; } ta;


/*** gen_allocdefs.h ***/

#define GEN_ALLOC_ready(ta,type,field,len,a,i,n,x,base,ta_ready) \
int ta_ready(ta *x,unsigned n) \
{ unsigned i; \
	if (x->field) { \
		i = x->a; \
		if (n > i) { \
			x->a = base + n + (n >> 3); \
			x->field = realloc(x->field,x->a * sizeof(type)); \
			if (x->field) return 1; \
			x->a = i; return 0; } \
		return 1; } \
	x->len = 0; \
	return !!(x->field = malloc((x->a = n) * sizeof(type))); }

#define GEN_ALLOC_readyplus(ta,type,field,len,a,i,n,x,base,ta_rplus) \
int ta_rplus(ta *x,unsigned n) \
{ unsigned i; \
	if (x->field) { \
		i = x->a; n += x->len; \
		if (n > i) { \
			x->a = base + n + (n >> 3); \
			x->field = realloc(x->field,x->a * sizeof(type)); \
			if (x->field) return 1; \
			x->a = i; return 0; } \
		return 1; } \
	x->len = 0; \
	return !!(x->field = malloc((x->a = n) * sizeof(type))); }

#define GEN_ALLOC_append(ta,type,field,len,a,i,n,x,base,ta_rplus,ta_append) \
int ta_append(ta *x,const type *i) \
{ if (!ta_rplus(x,1)) return 0; x->field[x->len++] = *i; return 1; }


/*** stralloc.h ***/
#if 0
GEN_ALLOC_typedef(stralloc,char,s,len,a)

extern int stralloc_ready(stralloc *,unsigned);
extern int stralloc_readyplus(stralloc *,unsigned);
extern int stralloc_copy(stralloc *,const stralloc *);
extern int stralloc_cat(stralloc *,const stralloc *);
extern int stralloc_copys(stralloc *,const char *);
extern int stralloc_cats(stralloc *,const char *);
extern int stralloc_copyb(stralloc *,const char *,unsigned);
extern int stralloc_catb(stralloc *,const char *,unsigned);
extern int stralloc_append(stralloc *,const char *); /* beware: this takes a pointer to 1 char */
extern int stralloc_starts(stralloc *,const char *);

#define stralloc_0(sa) stralloc_append(sa,"")

extern int stralloc_catulong0(stralloc *,unsigned long,unsigned);
extern int stralloc_catlong0(stralloc *,long,unsigned);

#define stralloc_catlong(sa,l) (stralloc_catlong0((sa),(l),0))
#define stralloc_catuint0(sa,i,n) (stralloc_catulong0((sa),(i),(n)))
#define stralloc_catint0(sa,i,n) (stralloc_catlong0((sa),(i),(n)))
#define stralloc_catint(sa,i) (stralloc_catlong0((sa),(i),0))
#endif

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


/*** openreadclose.h ***/
#if 0
extern int openreadclose(const char *,stralloc *,unsigned);
#endif

/*** pathexec.h ***/

extern void pathexec_run(const char *,char *const *,char *const *);
extern int pathexec_env(const char *,const char *);
extern void pathexec(char **);


/*** pmatch.h ***/

extern unsigned pmatch(const char *, const char *, unsigned);


/*** prot.h ***/

extern int prot_gid(int);
extern int prot_uid(int);


/*** readclose.h ***/
#if 0
extern int readclose_append(int,stralloc *,unsigned);
extern int readclose(int,stralloc *,unsigned);
#endif

/*** scan.h ***/

extern unsigned scan_uint(const char *,unsigned *);
extern unsigned scan_xint(const char *,unsigned *);
extern unsigned scan_nbbint(const char *,unsigned,unsigned,unsigned,unsigned *);
extern unsigned scan_ushort(const char *,unsigned short *);
extern unsigned scan_xshort(const char *,unsigned short *);
extern unsigned scan_nbbshort(const char *,unsigned,unsigned,unsigned,unsigned short *);
extern unsigned scan_ulong(const char *,unsigned long *);
extern unsigned scan_xlong(const char *,unsigned long *);
extern unsigned scan_nbblong(const char *,unsigned,unsigned,unsigned,unsigned long *);

extern unsigned scan_plusminus(const char *,int *);
extern unsigned scan_0x(const char *,unsigned *);

extern unsigned scan_whitenskip(const char *,unsigned);
extern unsigned scan_nonwhitenskip(const char *,unsigned);
extern unsigned scan_charsetnskip(const char *,const char *,unsigned);
extern unsigned scan_noncharsetnskip(const char *,const char *,unsigned);

extern unsigned scan_strncmp(const char *,const char *,unsigned);
extern unsigned scan_memcmp(const char *,const char *,unsigned);

extern unsigned scan_long(const char *,long *);
extern unsigned scan_8long(const char *,unsigned long *);


/*** seek.h ***/

typedef unsigned long seek_pos;

extern seek_pos seek_cur(int);

extern int seek_set(int,seek_pos);
extern int seek_end(int);

extern int seek_trunc(int,seek_pos);

#define seek_begin(fd) (seek_set((fd),(seek_pos) 0))


/*** sig.h ***/

extern int sig_alarm;
extern int sig_child;
extern int sig_cont;
extern int sig_hangup;
extern int sig_int;
extern int sig_pipe;
extern int sig_term;

extern void (*sig_defaulthandler)(int);
extern void (*sig_ignorehandler)(int);

extern void sig_catch(int,void (*)(int));
#define sig_ignore(s) (sig_catch((s),sig_ignorehandler))
#define sig_uncatch(s) (sig_catch((s),sig_defaulthandler))

extern void sig_block(int);
extern void sig_unblock(int);
extern void sig_blocknone(void);
extern void sig_pause(void);

extern void sig_dfl(int);


/*** str.h ***/

extern unsigned str_chr(const char *,int);  /* never returns NULL */

#define str_diff(s,t) strcmp((s),(t))
#define str_equal(s,t) (!strcmp((s),(t)))


/*** wait.h ***/

extern int wait_pid(int *wstat, int pid);
extern int wait_nohang(int *wstat);

#define wait_crashed(w) ((w) & 127)
#define wait_exitcode(w) ((w) >> 8)
#define wait_stopsig(w) ((w) >> 8)
#define wait_stopped(w) (((w) & 127) == 127)
