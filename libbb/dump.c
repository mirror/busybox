/* vi: set sw=4 ts=4: */
/*
 * Support code for the hexdump and od applets,
 * based on code from util-linux v 2.11l
 *
 * Copyright (c) 1989
 *	The Regents of the University of California.  All rights reserved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Original copyright notice is retained at the end of this file.
 */

#include "libbb.h"
#include "dump.h"

enum _vflag bb_dump_vflag = FIRST;
FS *bb_dump_fshead;				/* head of format strings */
static FU *endfu;
static char **_argv;
static off_t savaddress;	/* saved address/offset in stream */
static off_t eaddress;	/* end address */
static off_t address;	/* address/offset in stream */
off_t bb_dump_skip;				/* bytes to skip */
static int exitval;			/* final exit value */
int bb_dump_blocksize;			/* data block size */
int bb_dump_length = -1;		/* max bytes to read */

static const char index_str[] = ".#-+ 0123456789";

static const char size_conv_str[] =
"\x1\x4\x4\x4\x4\x4\x4\x8\x8\x8\x8\010cdiouxXeEfgG";

static const char lcc[] = "diouxX";

int bb_dump_size(FS * fs)
{
	FU *fu;
	int bcnt, cur_size;
	char *fmt;
	const char *p;
	int prec;

	/* figure out the data block bb_dump_size needed for each format unit */
	for (cur_size = 0, fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (fu->bcnt) {
			cur_size += fu->bcnt * fu->reps;
			continue;
		}
		for (bcnt = prec = 0, fmt = fu->fmt; *fmt; ++fmt) {
			if (*fmt != '%')
				continue;
			/*
			 * bb_dump_skip any special chars -- save precision in
			 * case it's a %s format.
			 */
			while (strchr(index_str + 1, *++fmt));
			if (*fmt == '.' && isdigit(*++fmt)) {
				prec = atoi(fmt);
				while (isdigit(*++fmt));
			}
			if (!(p = strchr(size_conv_str + 12, *fmt))) {
				if (*fmt == 's') {
					bcnt += prec;
				} else if (*fmt == '_') {
					++fmt;
					if ((*fmt == 'c') || (*fmt == 'p') || (*fmt == 'u')) {
						bcnt += 1;
					}
				}
			} else {
				bcnt += size_conv_str[p - (size_conv_str + 12)];
			}
		}
		cur_size += bcnt * fu->reps;
	}
	return cur_size;
}

static void rewrite(FS * fs)
{
	enum { NOTOKAY, USEBCNT, USEPREC } sokay;
	PR *pr, **nextpr = NULL;
	FU *fu;
	char *p1, *p2, *p3;
	char savech, *fmtp;
	const char *byte_count_str;
	int nconv, prec = 0;

	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		/*
		 * break each format unit into print units; each
		 * conversion character gets its own.
		 */
		for (nconv = 0, fmtp = fu->fmt; *fmtp; nextpr = &pr->nextpr) {
			/* NOSTRICT */
			/* DBU:[dvae@cray.com] calloc so that forward ptrs start out NULL*/
			pr = xzalloc(sizeof(PR));
			if (!fu->nextpr)
				fu->nextpr = pr;
			/* ignore nextpr -- its unused inside the loop and is
			 * uninitialized 1st time thru.
			 */

			/* bb_dump_skip preceding text and up to the next % sign */
			for (p1 = fmtp; *p1 && *p1 != '%'; ++p1);

			/* only text in the string */
			if (!*p1) {
				pr->fmt = fmtp;
				pr->flags = F_TEXT;
				break;
			}

			/*
			 * get precision for %s -- if have a byte count, don't
			 * need it.
			 */
			if (fu->bcnt) {
				sokay = USEBCNT;
				/* bb_dump_skip to conversion character */
				for (++p1; strchr(index_str, *p1); ++p1);
			} else {
				/* bb_dump_skip any special chars, field width */
				while (strchr(index_str + 1, *++p1));
				if (*p1 == '.' && isdigit(*++p1)) {
					sokay = USEPREC;
					prec = atoi(p1);
					while (isdigit(*++p1));
				} else
					sokay = NOTOKAY;
			}

			p2 = p1 + 1;	/* set end pointer */

			/*
			 * figure out the byte count for each conversion;
			 * rewrite the format as necessary, set up blank-
			 * pbb_dump_adding for end of data.
			 */

			if (*p1 == 'c') {
				pr->flags = F_CHAR;
			DO_BYTE_COUNT_1:
				byte_count_str = "\001";
			DO_BYTE_COUNT:
				if (fu->bcnt) {
					do {
						if (fu->bcnt == *byte_count_str) {
							break;
						}
					} while (*++byte_count_str);
				}
				/* Unlike the original, output the remainder of the format string. */
				if (!*byte_count_str) {
					bb_error_msg_and_die("bad byte count for conversion character %s", p1);
				}
				pr->bcnt = *byte_count_str;
			} else if (*p1 == 'l') {
				++p2;
				++p1;
			DO_INT_CONV:
				{
					const char *e;
					if (!(e = strchr(lcc, *p1))) {
						goto DO_BAD_CONV_CHAR;
					}
					pr->flags = F_INT;
					if (e > lcc + 1) {
						pr->flags = F_UINT;
					}
					byte_count_str = "\004\002\001";
					goto DO_BYTE_COUNT;
				}
				/* NOTREACHED */
			} else if (strchr(lcc, *p1)) {
				goto DO_INT_CONV;
			} else if (strchr("eEfgG", *p1)) {
				pr->flags = F_DBL;
				byte_count_str = "\010\004";
				goto DO_BYTE_COUNT;
			} else if (*p1 == 's') {
				pr->flags = F_STR;
				if (sokay == USEBCNT) {
					pr->bcnt = fu->bcnt;
				} else if (sokay == USEPREC) {
					pr->bcnt = prec;
				} else {	/* NOTOKAY */
					bb_error_msg_and_die("%%s requires a precision or a byte count");
				}
			} else if (*p1 == '_') {
				++p2;
				switch (p1[1]) {
				case 'A':
					endfu = fu;
					fu->flags |= F_IGNORE;
					/* FALLTHROUGH */
				case 'a':
					pr->flags = F_ADDRESS;
					++p2;
					if ((p1[2] != 'd') && (p1[2] != 'o') && (p1[2] != 'x')) {
						goto DO_BAD_CONV_CHAR;
					}
					*p1 = p1[2];
					break;
				case 'c':
					pr->flags = F_C;
					/* *p1 = 'c';   set in conv_c */
					goto DO_BYTE_COUNT_1;
				case 'p':
					pr->flags = F_P;
					*p1 = 'c';
					goto DO_BYTE_COUNT_1;
				case 'u':
					pr->flags = F_U;
					/* *p1 = 'c';   set in conv_u */
					goto DO_BYTE_COUNT_1;
				default:
					goto DO_BAD_CONV_CHAR;
				}
			} else {
			DO_BAD_CONV_CHAR:
				bb_error_msg_and_die("bad conversion character %%%s", p1);
			}

			/*
			 * copy to PR format string, set conversion character
			 * pointer, update original.
			 */
			savech = *p2;
			p1[1] = '\0';
			pr->fmt = xstrdup(fmtp);
			*p2 = savech;
			pr->cchar = pr->fmt + (p1 - fmtp);

			/* DBU:[dave@cray.com] w/o this, trailing fmt text, space is lost.
			 * Skip subsequent text and up to the next % sign and tack the
			 * additional text onto fmt: eg. if fmt is "%x is a HEX number",
			 * we lose the " is a HEX number" part of fmt.
			 */
			for (p3 = p2; *p3 && *p3 != '%'; p3++);
			if (p3 > p2)
			{
				savech = *p3;
				*p3 = '\0';
				pr->fmt = xrealloc(pr->fmt, strlen(pr->fmt)+(p3-p2)+1);
				strcat(pr->fmt, p2);
				*p3 = savech;
				p2 = p3;
			}

			fmtp = p2;

			/* only one conversion character if byte count */
			if (!(pr->flags & F_ADDRESS) && fu->bcnt && nconv++) {
				bb_error_msg_and_die("byte count with multiple conversion characters");
			}
		}
		/*
		 * if format unit byte count not specified, figure it out
		 * so can adjust rep count later.
		 */
		if (!fu->bcnt)
			for (pr = fu->nextpr; pr; pr = pr->nextpr)
				fu->bcnt += pr->bcnt;
	}
	/*
	 * if the format string interprets any data at all, and it's
	 * not the same as the bb_dump_blocksize, and its last format unit
	 * interprets any data at all, and has no iteration count,
	 * repeat it as necessary.
	 *
	 * if, rep count is greater than 1, no trailing whitespace
	 * gets output from the last iteration of the format unit.
	 */
	for (fu = fs->nextfu;; fu = fu->nextfu) {
		if (!fu->nextfu && fs->bcnt < bb_dump_blocksize &&
			!(fu->flags & F_SETREP) && fu->bcnt)
			fu->reps += (bb_dump_blocksize - fs->bcnt) / fu->bcnt;
		if (fu->reps > 1) {
			for (pr = fu->nextpr;; pr = pr->nextpr)
				if (!pr->nextpr)
					break;
			for (p1 = pr->fmt, p2 = NULL; *p1; ++p1)
				p2 = isspace(*p1) ? p1 : NULL;
			if (p2)
				pr->nospace = p2;
		}
		if (!fu->nextfu)
			break;
	}
}

static void do_skip(const char *fname, int statok)
{
	struct stat sbuf;

	if (statok) {
		if (fstat(STDIN_FILENO, &sbuf)) {
			bb_perror_msg_and_die("%s", fname);
		}
		if ((!(S_ISCHR(sbuf.st_mode) ||
			   S_ISBLK(sbuf.st_mode) ||
			   S_ISFIFO(sbuf.st_mode))) && bb_dump_skip >= sbuf.st_size) {
			/* If bb_dump_size valid and bb_dump_skip >= size */
			bb_dump_skip -= sbuf.st_size;
			address += sbuf.st_size;
			return;
		}
	}
	if (fseek(stdin, bb_dump_skip, SEEK_SET)) {
		bb_perror_msg_and_die("%s", fname);
	}
	savaddress = address += bb_dump_skip;
	bb_dump_skip = 0;
}

static int next(char **argv)
{
	static int done;
	int statok;

	if (argv) {
		_argv = argv;
		return 1;
	}
	for (;;) {
		if (*_argv) {
			if (!(freopen(*_argv, "r", stdin))) {
				bb_perror_msg("%s", *_argv);
				exitval = 1;
				++_argv;
				continue;
			}
			statok = done = 1;
		} else {
			if (done++)
				return 0;
			statok = 0;
		}
		if (bb_dump_skip)
			do_skip(statok ? *_argv : "stdin", statok);
		if (*_argv)
			++_argv;
		if (!bb_dump_skip)
			return 1;
	}
	/* NOTREACHED */
}

static unsigned char *get(void)
{
	static int ateof = 1;
	static unsigned char *curp=NULL, *savp; /*DBU:[dave@cray.com]initialize curp */
	int n;
	int need, nread;
	unsigned char *tmpp;

	if (!curp) {
		address = (off_t)0; /*DBU:[dave@cray.com] initialize,initialize..*/
		curp = xmalloc(bb_dump_blocksize);
		savp = xmalloc(bb_dump_blocksize);
	} else {
		tmpp = curp;
		curp = savp;
		savp = tmpp;
		address = savaddress += bb_dump_blocksize;
	}
	for (need = bb_dump_blocksize, nread = 0;;) {
		/*
		 * if read the right number of bytes, or at EOF for one file,
		 * and no other files are available, zero-pad the rest of the
		 * block and set the end flag.
		 */
		if (!bb_dump_length || (ateof && !next((char **) NULL))) {
			if (need == bb_dump_blocksize) {
				return NULL;
			}
			if (bb_dump_vflag != ALL && !memcmp(curp, savp, nread)) {
				if (bb_dump_vflag != DUP) {
					puts("*");
				}
				return NULL;
			}
			memset((char *) curp + nread, 0, need);
			eaddress = address + nread;
			return curp;
		}
		n = fread((char *) curp + nread, sizeof(unsigned char),
				  bb_dump_length == -1 ? need : MIN(bb_dump_length, need), stdin);
		if (!n) {
			if (ferror(stdin)) {
				bb_perror_msg("%s", _argv[-1]);
			}
			ateof = 1;
			continue;
		}
		ateof = 0;
		if (bb_dump_length != -1) {
			bb_dump_length -= n;
		}
		if (!(need -= n)) {
			if (bb_dump_vflag == ALL || bb_dump_vflag == FIRST
				|| memcmp(curp, savp, bb_dump_blocksize)) {
				if (bb_dump_vflag == DUP || bb_dump_vflag == FIRST) {
					bb_dump_vflag = WAIT;
				}
				return curp;
			}
			if (bb_dump_vflag == WAIT) {
				puts("*");
			}
			bb_dump_vflag = DUP;
			address = savaddress += bb_dump_blocksize;
			need = bb_dump_blocksize;
			nread = 0;
		} else {
			nread += n;
		}
	}
}

static void bpad(PR * pr)
{
	char *p1, *p2;

	/*
	 * remove all conversion flags; '-' is the only one valid
	 * with %s, and it's not useful here.
	 */
	pr->flags = F_BPAD;
	*pr->cchar = 's';
	for (p1 = pr->fmt; *p1 != '%'; ++p1);
	for (p2 = ++p1; *p1 && strchr(" -0+#", *p1); ++p1)
		if (pr->nospace) pr->nospace--;
	while ((*p2++ = *p1++) != 0);
}

static const char conv_str[] =
	"\0\\0\0"
	"\007\\a\0"				/* \a */
	"\b\\b\0"
	"\f\\b\0"
	"\n\\n\0"
	"\r\\r\0"
	"\t\\t\0"
	"\v\\v\0"
	"\0";


static void conv_c(PR * pr, unsigned char * p)
{
	const char *str = conv_str;
	char buf[10];

	do {
		if (*p == *str) {
			++str;
			goto strpr;
		}
		str += 4;
	} while (*str);

	if (isprint(*p)) {
		*pr->cchar = 'c';
		(void) printf(pr->fmt, *p);
	} else {
		sprintf(buf, "%03o", (int) *p);
		str = buf;
	  strpr:
		*pr->cchar = 's';
		printf(pr->fmt, str);
	}
}

static void conv_u(PR * pr, unsigned char * p)
{
	static const char list[] =
		"nul\0soh\0stx\0etx\0eot\0enq\0ack\0bel\0"
		"bs\0_ht\0_lf\0_vt\0_ff\0_cr\0_so\0_si\0_"
		"dle\0dcl\0dc2\0dc3\0dc4\0nak\0syn\0etb\0"
		"can\0em\0_sub\0esc\0fs\0_gs\0_rs\0_us";

	/* od used nl, not lf */
	if (*p <= 0x1f) {
		*pr->cchar = 's';
		printf(pr->fmt, list + (4 * (int)*p));
	} else if (*p == 0x7f) {
		*pr->cchar = 's';
		printf(pr->fmt, "del");
	} else if (isprint(*p)) {
		*pr->cchar = 'c';
		printf(pr->fmt, *p);
	} else {
		*pr->cchar = 'x';
		printf(pr->fmt, (int) *p);
	}
}

static void display(void)
{
/*  extern FU *endfu; */
	FS *fs;
	FU *fu;
	PR *pr;
	int cnt;
	unsigned char *bp;

	off_t saveaddress;
	unsigned char savech = 0, *savebp;

	while ((bp = get()) != NULL) {
		for (fs = bb_dump_fshead, savebp = bp, saveaddress = address; fs;
			 fs = fs->nextfs, bp = savebp, address = saveaddress) {
			for (fu = fs->nextfu; fu; fu = fu->nextfu) {
				if (fu->flags & F_IGNORE) {
					break;
				}
				for (cnt = fu->reps; cnt; --cnt) {
					for (pr = fu->nextpr; pr; address += pr->bcnt,
						 bp += pr->bcnt, pr = pr->nextpr) {
						if (eaddress && address >= eaddress &&
							!(pr->flags & (F_TEXT | F_BPAD))) {
							bpad(pr);
						}
						if (cnt == 1 && pr->nospace) {
							savech = *pr->nospace;
							*pr->nospace = '\0';
						}
/*                      PRINT; */
						switch (pr->flags) {
						case F_ADDRESS:
							printf(pr->fmt, (unsigned int) address);
							break;
						case F_BPAD:
							printf(pr->fmt, "");
							break;
						case F_C:
							conv_c(pr, bp);
							break;
						case F_CHAR:
							printf(pr->fmt, *bp);
							break;
						case F_DBL:{
							double dval;
							float fval;

							switch (pr->bcnt) {
							case 4:
								memmove((char *) &fval, (char *) bp,
									  sizeof(fval));
								printf(pr->fmt, fval);
								break;
							case 8:
								memmove((char *) &dval, (char *) bp,
									  sizeof(dval));
								printf(pr->fmt, dval);
								break;
							}
							break;
						}
						case F_INT:{
							int ival;
							short sval;

							switch (pr->bcnt) {
							case 1:
								printf(pr->fmt, (int) *bp);
								break;
							case 2:
								memmove((char *) &sval, (char *) bp,
									  sizeof(sval));
								printf(pr->fmt, (int) sval);
								break;
							case 4:
								memmove((char *) &ival, (char *) bp,
									  sizeof(ival));
								printf(pr->fmt, ival);
								break;
							}
							break;
						}
						case F_P:
							printf(pr->fmt, isprint(*bp) ? *bp : '.');
							break;
						case F_STR:
							printf(pr->fmt, (char *) bp);
							break;
						case F_TEXT:
							printf(pr->fmt);
							break;
						case F_U:
							conv_u(pr, bp);
							break;
						case F_UINT:{
							unsigned int ival;
							unsigned short sval;

							switch (pr->bcnt) {
							case 1:
								printf(pr->fmt, (unsigned int) * bp);
								break;
							case 2:
								memmove((char *) &sval, (char *) bp,
									  sizeof(sval));
								printf(pr->fmt, (unsigned int) sval);
								break;
							case 4:
								memmove((char *) &ival, (char *) bp,
									  sizeof(ival));
								printf(pr->fmt, ival);
								break;
							}
							break;
						}
						}
						if (cnt == 1 && pr->nospace) {
							*pr->nospace = savech;
						}
					}
				}
			}
		}
	}
	if (endfu) {
		/*
		 * if eaddress not set, error or file bb_dump_size was multiple of
		 * bb_dump_blocksize, and no partial block ever found.
		 */
		if (!eaddress) {
			if (!address) {
				return;
			}
			eaddress = address;
		}
		for (pr = endfu->nextpr; pr; pr = pr->nextpr) {
			switch (pr->flags) {
			case F_ADDRESS:
				(void) printf(pr->fmt, (unsigned int) eaddress);
				break;
			case F_TEXT:
				(void) printf(pr->fmt);
				break;
			}
		}
	}
}

int bb_dump_dump(char **argv)
{
	FS *tfs;

	/* figure out the data block bb_dump_size */
	for (bb_dump_blocksize = 0, tfs = bb_dump_fshead; tfs; tfs = tfs->nextfs) {
		tfs->bcnt = bb_dump_size(tfs);
		if (bb_dump_blocksize < tfs->bcnt) {
			bb_dump_blocksize = tfs->bcnt;
		}
	}
	/* rewrite the rules, do syntax checking */
	for (tfs = bb_dump_fshead; tfs; tfs = tfs->nextfs) {
		rewrite(tfs);
	}

	next(argv);
	display();

	return exitval;
}

void bb_dump_add(const char *fmt)
{
	const char *p;
	char *p1;
	char *p2;
	static FS **nextfs;
	FS *tfs;
	FU *tfu, **nextfu;
	const char *savep;

	/* start new linked list of format units */
	tfs = xzalloc(sizeof(FS)); /*DBU:[dave@cray.com] start out NULL */
	if (!bb_dump_fshead) {
		bb_dump_fshead = tfs;
	} else {
		*nextfs = tfs;
	}
	nextfs = &tfs->nextfs;
	nextfu = &tfs->nextfu;

	/* take the format string and break it up into format units */
	for (p = fmt;;) {
		/* bb_dump_skip leading white space */
		p = skip_whitespace(p);
		if (!*p) {
			break;
		}

		/* allocate a new format unit and link it in */
		/* NOSTRICT */
		/* DBU:[dave@cray.com] calloc so that forward pointers start out NULL */
		tfu = xzalloc(sizeof(FU));
		*nextfu = tfu;
		nextfu = &tfu->nextfu;
		tfu->reps = 1;

		/* if leading digit, repetition count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p);
			if (!isspace(*p) && *p != '/') {
				bb_error_msg_and_die("bad format {%s}", fmt);
			}
			/* may overwrite either white space or slash */
			tfu->reps = atoi(savep);
			tfu->flags = F_SETREP;
			/* bb_dump_skip trailing white space */
			p = skip_whitespace(++p);
		}

		/* bb_dump_skip slash and trailing white space */
		if (*p == '/') {
			p = skip_whitespace(++p);
		}

		/* byte count */
		if (isdigit(*p)) {
// TODO: use bb_strtou
			savep = p;
			do p++; while (isdigit(*p));
			if (!isspace(*p)) {
				bb_error_msg_and_die("bad format {%s}", fmt);
			}
			tfu->bcnt = atoi(savep);
			/* bb_dump_skip trailing white space */
			p = skip_whitespace(++p);
		}

		/* format */
		if (*p != '"') {
			bb_error_msg_and_die("bad format {%s}", fmt);
		}
		for (savep = ++p; *p != '"';) {
			if (*p++ == 0) {
				bb_error_msg_and_die("bad format {%s}", fmt);
			}
		}
		tfu->fmt = xmalloc(p - savep + 1);
		strncpy(tfu->fmt, savep, p - savep);
		tfu->fmt[p - savep] = '\0';
/*      escape(tfu->fmt); */

		p1 = tfu->fmt;

		/* alphabetic escape sequences have to be done in place */
		for (p2 = p1;; ++p1, ++p2) {
			if (!*p1) {
				*p2 = *p1;
				break;
			}
			if (*p1 == '\\') {
				const char *cs = conv_str + 4;
				++p1;
				*p2 = *p1;
				do {
					if (*p1 == cs[2]) {
						*p2 = cs[0];
						break;
					}
					cs += 4;
				} while (*cs);
			}
		}

		p++;
	}
}

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
