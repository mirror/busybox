/*
 * Support code for the hexdump and od applets,
 * based on code from util-linux v 2.11l
 *
 * Copyright (c) 1989
 *	The Regents of the University of California.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Original copyright notice is retained at the end of this file.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>		/* for isdigit() */
#include "dump.h"
#include "libbb.h"

enum _vflag vflag = FIRST;
FS *fshead;				/* head of format strings */
extern FS *fshead;				/* head of format strings */
extern int blocksize;
static FU *endfu;
static char **_argv;
static off_t savaddress;		/* saved address/offset in stream */
static off_t eaddress;			/* end address */
static off_t address;			/* address/offset in stream */
off_t skip;                      /* bytes to skip */
off_t saveaddress;
int exitval;				/* final exit value */
int blocksize;				/* data block size */
int length = -1;			/* max bytes to read */


int size(FS *fs)
{
	register FU *fu;
	register int bcnt, cursize;
	register char *fmt;
	int prec;

	/* figure out the data block size needed for each format unit */
	for (cursize = 0, fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (fu->bcnt) {
			cursize += fu->bcnt * fu->reps;
			continue;
		}
		for (bcnt = prec = 0, fmt = fu->fmt; *fmt; ++fmt) {
			if (*fmt != '%')
				continue;
			/*
			 * skip any special chars -- save precision in
			 * case it's a %s format.
			 */
			while (index(".#-+ 0123456789" + 1, *++fmt));
			if (*fmt == '.' && isdigit(*++fmt)) {
				prec = atoi(fmt);
				while (isdigit(*++fmt));
			}
			switch(*fmt) {
			case 'c':
				bcnt += 1;
				break;
			case 'd': case 'i': case 'o': case 'u':
			case 'x': case 'X':
				bcnt += 4;
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				bcnt += 8;
				break;
			case 's':
				bcnt += prec;
				break;
			case '_':
				switch(*++fmt) {
				case 'c': case 'p': case 'u':
					bcnt += 1;
					break;
				}
			}
		}
		cursize += bcnt * fu->reps;
	}
	return(cursize);
}

void rewrite(FS *fs)
{
	enum { NOTOKAY, USEBCNT, USEPREC } sokay;
	register PR *pr, **nextpr = NULL;
	register FU *fu;
	register char *p1, *p2;
	char savech, *fmtp;
	int nconv, prec = 0;

	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		/*
		 * break each format unit into print units; each
		 * conversion character gets its own.
		 */
		for (nconv = 0, fmtp = fu->fmt; *fmtp; nextpr = &pr->nextpr) {
			/* NOSTRICT */
			pr = (PR *)xmalloc(sizeof(PR));
			if (!fu->nextpr)
				fu->nextpr = pr;
			else
				*nextpr = pr;

			/* skip preceding text and up to the next % sign */
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
				/* skip to conversion character */
				for (++p1; index(".#-+ 0123456789", *p1); ++p1);
			} else {
				/* skip any special chars, field width */
				while (index(".#-+ 0123456789" + 1, *++p1));
				if (*p1 == '.' && isdigit(*++p1)) {
					sokay = USEPREC;
					prec = atoi(p1);
					while (isdigit(*++p1));
				}
				else
					sokay = NOTOKAY;
			}

			p2 = p1 + 1;		/* set end pointer */

			/*
			 * figure out the byte count for each conversion;
			 * rewrite the format as necessary, set up blank-
			 * padding for end of data.
			 */
			switch(*p1) {
			case 'c':
				pr->flags = F_CHAR;
				switch(fu->bcnt) {
				case 0: case 1:
					pr->bcnt = 1;
					break;
				default:
					p1[1] = '\0';
					error_msg_and_die("bad byte count for conversion character %s.", p1);
				}
				break;
			case 'd': case 'i':
				pr->flags = F_INT;
				goto sw1;
			case 'l':
				++p2;
				switch(p1[1]) {
				case 'd': case 'i':
					++p1;
					pr->flags = F_INT;
					goto sw1;
				case 'o': case 'u': case 'x': case 'X':
					++p1;
					pr->flags = F_UINT;
					goto sw1;
				default:
					p1[2] = '\0';
					error_msg_and_die("hexdump: bad conversion character %%%s.\n", p1);
				}
				/* NOTREACHED */
			case 'o': case 'u': case 'x': case 'X':
				pr->flags = F_UINT;
sw1:				switch(fu->bcnt) {
				case 0: case 4:
					pr->bcnt = 4;
					break;
				case 1:
					pr->bcnt = 1;
					break;
				case 2:
					pr->bcnt = 2;
					break;
				default:
					p1[1] = '\0';
					error_msg_and_die("bad byte count for conversion character %s.", p1);
				}
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				pr->flags = F_DBL;
				switch(fu->bcnt) {
				case 0: case 8:
					pr->bcnt = 8;
					break;
				case 4:
					pr->bcnt = 4;
					break;
				default:
					p1[1] = '\0';
					error_msg_and_die("bad byte count for conversion character %s.", p1);
				}
				break;
			case 's':
				pr->flags = F_STR;
				switch(sokay) {
				case NOTOKAY:
					error_msg_and_die("%%s requires a precision or a byte count.");
				case USEBCNT:
					pr->bcnt = fu->bcnt;
					break;
				case USEPREC:
					pr->bcnt = prec;
					break;
				}
				break;
			case '_':
				++p2;
				switch(p1[1]) {
				case 'A':
					endfu = fu;
					fu->flags |= F_IGNORE;
					/* FALLTHROUGH */
				case 'a':
					pr->flags = F_ADDRESS;
					++p2;
					switch(p1[2]) {
					case 'd': case 'o': case'x':
						*p1 = p1[2];
						break;
					default:
						p1[3] = '\0';
						error_msg_and_die("hexdump: bad conversion character %%%s.\n", p1);
					}
					break;
				case 'c':
					pr->flags = F_C;
					/* *p1 = 'c';	set in conv_c */
					goto sw2;
				case 'p':
					pr->flags = F_P;
					*p1 = 'c';
					goto sw2;
				case 'u':
					pr->flags = F_U;
					/* *p1 = 'c';	set in conv_u */
sw2:					switch(fu->bcnt) {
					case 0: case 1:
						pr->bcnt = 1;
						break;
					default:
						p1[2] = '\0';
						error_msg_and_die("bad byte count for conversion character %s.", p1);
					}
					break;
				default:
					p1[2] = '\0';
					error_msg_and_die("hexdump: bad conversion character %%%s.\n", p1);
				}
				break;
			default:
				p1[1] = '\0';
				error_msg_and_die("hexdump: bad conversion character %%%s.\n", p1);
			}

			/*
			 * copy to PR format string, set conversion character
			 * pointer, update original.
			 */
			savech = *p2;
			p1[1] = '\0';
			if (!(pr->fmt = strdup(fmtp)))
				perror_msg_and_die("hexdump");
			*p2 = savech;
			pr->cchar = pr->fmt + (p1 - fmtp);
			fmtp = p2;

			/* only one conversion character if byte count */
			if (!(pr->flags&F_ADDRESS) && fu->bcnt && nconv++) {
				error_msg_and_die("hexdump: byte count with multiple conversion characters.\n");
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
	 * not the same as the blocksize, and its last format unit
	 * interprets any data at all, and has no iteration count,
	 * repeat it as necessary.
	 *
	 * if, rep count is greater than 1, no trailing whitespace
	 * gets output from the last iteration of the format unit.
	 */
	for (fu = fs->nextfu;; fu = fu->nextfu) {
		if (!fu->nextfu && fs->bcnt < blocksize &&
		    !(fu->flags&F_SETREP) && fu->bcnt)
			fu->reps += (blocksize - fs->bcnt) / fu->bcnt;
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

static void doskip(char *fname, int statok)
{
	struct stat sbuf;

	if (statok) {
		if (fstat(fileno(stdin), &sbuf)) {
			perror_msg_and_die("hexdump: %s", fname);
		}
		if ( ( ! (S_ISCHR(sbuf.st_mode) ||
 			  S_ISBLK(sbuf.st_mode) ||
 			  S_ISFIFO(sbuf.st_mode)) ) &&
		     skip >= sbuf.st_size) {
		  /* If size valid and skip >= size */
			skip -= sbuf.st_size;
			address += sbuf.st_size;
			return;
		}
	}
	if (fseek(stdin, skip, SEEK_SET)) {
		perror_msg_and_die("hexdump: %s", fname);
	}
	savaddress = address += skip;
	skip = 0;
}

int next(char **argv)
{
	static int done;
	int statok;

	if (argv) {
		_argv = argv;
		return(1);
	}
	for (;;) {
		if (*_argv) {
			if (!(freopen(*_argv, "r", stdin))) {
				perror_msg("%s", *_argv);
				exitval = 1;
				++_argv;
				continue;
			}
			statok = done = 1;
		} else {
			if (done++)
				return(0);
			statok = 0;
		}
		if (skip)
			doskip(statok ? *_argv : "stdin", statok);
		if (*_argv)
			++_argv;
		if (!skip)
			return(1);
	}
	/* NOTREACHED */
}

static u_char *
get(void)
{
	static int ateof = 1;
	static u_char *curp, *savp;
	register int n;
	int need, nread;
	u_char *tmpp;

	if (!curp) {
		curp = (u_char *)xmalloc(blocksize);
		savp = (u_char *)xmalloc(blocksize);
	} else {
		tmpp = curp;
		curp = savp;
		savp = tmpp;
		address = savaddress += blocksize;
	}
	for (need = blocksize, nread = 0;;) {
		/*
		 * if read the right number of bytes, or at EOF for one file,
		 * and no other files are available, zero-pad the rest of the
		 * block and set the end flag.
		 */
		if (!length || (ateof && !next((char **)NULL))) {
			if (need == blocksize) {
				return((u_char *)NULL);
			}
			if (vflag != ALL && !bcmp(curp, savp, nread)) {
				if (vflag != DUP) {
					printf("*\n");
				}
				return((u_char *)NULL);
			}
			bzero((char *)curp + nread, need);
			eaddress = address + nread;
			return(curp);
		}
		n = fread((char *)curp + nread, sizeof(u_char),
		    length == -1 ? need : MIN(length, need), stdin);
		if (!n) {
			if (ferror(stdin)) {
				perror_msg("%s", _argv[-1]);
			}
			ateof = 1;
			continue;
		}
		ateof = 0;
		if (length != -1) {
			length -= n;
		}
		if (!(need -= n)) {
			if (vflag == ALL || vflag == FIRST ||
			    bcmp(curp, savp, blocksize)) {
				if (vflag == DUP || vflag == FIRST) {
					vflag = WAIT;
				}
				return(curp);
			}
			if (vflag == WAIT) {
				printf("*\n");
			}
			vflag = DUP;
			address = savaddress += blocksize;
			need = blocksize;
			nread = 0;
		} else {
			nread += n;
		}
	}
}

static void bpad(PR *pr)
{
	register char *p1, *p2;

	/*
	 * remove all conversion flags; '-' is the only one valid
	 * with %s, and it's not useful here.
	 */
	pr->flags = F_BPAD;
	*pr->cchar = 's';
	for (p1 = pr->fmt; *p1 != '%'; ++p1);
	for (p2 = ++p1; *p1 && index(" -0+#", *p1); ++p1);
	while ((*p2++ = *p1++) != 0) ;
}

void conv_c(PR *pr, u_char *p)
{
	char buf[10], *str;

	switch(*p) {
	case '\0':
		str = "\\0";
		goto strpr;
	/* case '\a': */
	case '\007':
		str = "\\a";
		goto strpr;
	case '\b':
		str = "\\b";
		goto strpr;
	case '\f':
		str = "\\f";
		goto strpr;
	case '\n':
		str = "\\n";
		goto strpr;
	case '\r':
		str = "\\r";
		goto strpr;
	case '\t':
		str = "\\t";
		goto strpr;
	case '\v':
		str = "\\v";
		goto strpr;
	default:
		break;
	}
	if (isprint(*p)) {
		*pr->cchar = 'c';
		(void)printf(pr->fmt, *p);
	} else {
		sprintf(str = buf, "%03o", (int)*p);
strpr:
		*pr->cchar = 's';
		printf(pr->fmt, str);
	}
}

void conv_u(PR *pr, u_char *p)
{
	static char *list[] = {
		"nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
		 "bs",  "ht",  "lf",  "vt",  "ff",  "cr",  "so",  "si",
		"dle", "dcl", "dc2", "dc3", "dc4", "nak", "syn", "etb",
		"can",  "em", "sub", "esc",  "fs",  "gs",  "rs",  "us",
	};

	/* od used nl, not lf */
	if (*p <= 0x1f) {
		*pr->cchar = 's';
		printf(pr->fmt, list[*p]);
	} else if (*p == 0x7f) {
		*pr->cchar = 's';
		printf(pr->fmt, "del");
	} else if (isprint(*p)) {
		*pr->cchar = 'c';
		printf(pr->fmt, *p);
	} else {
		*pr->cchar = 'x';
		printf(pr->fmt, (int)*p);
	}
}

void display(void)
{
//	extern FU *endfu;
	register FS *fs;
	register FU *fu;
	register PR *pr;
	register int cnt;
	register u_char *bp;
//	off_t saveaddress;
	u_char savech = 0, *savebp;

	while ((bp = get()) != NULL) {
	    for (fs = fshead, savebp = bp, saveaddress = address; fs;
				fs = fs->nextfs, bp = savebp, address = saveaddress) {
		    for (fu = fs->nextfu; fu; fu = fu->nextfu) {
				if (fu->flags & F_IGNORE) {
					break;
				}
				for (cnt = fu->reps; cnt; --cnt) {
				    for (pr = fu->nextpr; pr; address += pr->bcnt,
							bp += pr->bcnt, pr = pr->nextpr) {
					    if (eaddress && address >= eaddress &&
								!(pr->flags&(F_TEXT|F_BPAD))) {
							bpad(pr);
						}
					    if (cnt == 1 && pr->nospace) {
							savech = *pr->nospace;
							*pr->nospace = '\0';
					    }
//					    PRINT;
						switch(pr->flags) {
							case F_ADDRESS:
								printf(pr->fmt, address);
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
							case F_DBL: {
								double dval;
								float fval;
								switch(pr->bcnt) {
									case 4:
										bcopy((char *)bp, (char *)&fval, sizeof(fval));
										printf(pr->fmt, fval);
										break;
									case 8:
										bcopy((char *)bp, (char *)&dval, sizeof(dval));
										printf(pr->fmt, dval);
										break;
								}
								break;
							}
							case F_INT: {
								int ival;
								short sval;
								switch(pr->bcnt) {
									case 1:
										printf(pr->fmt, (int)*bp);
										break;
									case 2:
										bcopy((char *)bp, (char *)&sval, sizeof(sval));
										printf(pr->fmt, (int)sval);
										break;
									case 4:
										bcopy((char *)bp, (char *)&ival, sizeof(ival));
										printf(pr->fmt, ival);
										break;
								}
								break;
							}
							case F_P:
								printf(pr->fmt, isprint(*bp) ? *bp : '.');
								break;
							case F_STR:
								printf(pr->fmt, (char *)bp);
								break;
							case F_TEXT:
								printf(pr->fmt);
								break;
							case F_U:
								conv_u(pr, bp);
								break;
							case F_UINT: {
								u_int ival;
								u_short sval;
								switch(pr->bcnt) {
									case 1:
										printf(pr->fmt, (u_int)*bp);
										break;
									case 2:
										bcopy((char *)bp, (char *)&sval, sizeof(sval));
										printf(pr->fmt, (u_int)sval);
										break;
									case 4:
										bcopy((char *)bp, (char *)&ival, sizeof(ival));
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
		 * if eaddress not set, error or file size was multiple of
		 * blocksize, and no partial block ever found.
		 */
		if (!eaddress) {
			if (!address) {
				return;
			}
			eaddress = address;
		}
		for (pr = endfu->nextpr; pr; pr = pr->nextpr) {
			switch(pr->flags) {
			case F_ADDRESS:
				(void)printf(pr->fmt, eaddress);
				break;
			case F_TEXT:
				(void)printf(pr->fmt);
				break;
			}
		}
	}
}

int dump(char **argv)
{
	register FS *tfs;

	/* figure out the data block size */
	for (blocksize = 0, tfs = fshead; tfs; tfs = tfs->nextfs) {
		tfs->bcnt = size(tfs);
		if (blocksize < tfs->bcnt) {
			blocksize = tfs->bcnt;
		}
	}
	/* rewrite the rules, do syntax checking */
	for (tfs = fshead; tfs; tfs = tfs->nextfs) {
		rewrite(tfs);
	}

	next(argv);
	display();

	return(exitval);
}

void add(char *fmt)
{
	register char *p;
	register char *p1;
	register char *p2;
	static FS **nextfs;
	FS *tfs;
	FU *tfu, **nextfu;
	char *savep;

	/* start new linked list of format units */
	/* NOSTRICT */
	tfs = (FS *)xmalloc(sizeof(FS));
	if (!fshead) {
		fshead = tfs;
	} else {
		*nextfs = tfs;
	}
	nextfs = &tfs->nextfs;
	nextfu = &tfs->nextfu;

	/* take the format string and break it up into format units */
	for (p = fmt;;) {
		/* skip leading white space */
		for (; isspace(*p); ++p);
		if (!*p) {
			break;
		}

		/* allocate a new format unit and link it in */
		/* NOSTRICT */
		tfu = (FU *)xmalloc(sizeof(FU));
		*nextfu = tfu;
		nextfu = &tfu->nextfu;
		tfu->reps = 1;

		/* if leading digit, repetition count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p);
			if (!isspace(*p) && *p != '/') {
				error_msg_and_die("hexdump: bad format {%s}", fmt);
			}
			/* may overwrite either white space or slash */
			tfu->reps = atoi(savep);
			tfu->flags = F_SETREP;
			/* skip trailing white space */
			for (++p; isspace(*p); ++p);
		}

		/* skip slash and trailing white space */
		if (*p == '/') {
			while (isspace(*++p));
		}

		/* byte count */
		if (isdigit(*p)) {
			for (savep = p; isdigit(*p); ++p);
			if (!isspace(*p)) {
				error_msg_and_die("hexdump: bad format {%s}", fmt);
			}
			tfu->bcnt = atoi(savep);
			/* skip trailing white space */
			for (++p; isspace(*p); ++p);
		}

		/* format */
		if (*p != '"') {
			error_msg_and_die("hexdump: bad format {%s}", fmt);
		}
		for (savep = ++p; *p != '"';) {
			if (*p++ == 0) {
				error_msg_and_die("hexdump: bad format {%s}", fmt);
			}
		}
		if (!(tfu->fmt = malloc(p - savep + 1))) {
			perror_msg_and_die("hexdump");
		}
		strncpy(tfu->fmt, savep, p - savep);
		tfu->fmt[p - savep] = '\0';
//		escape(tfu->fmt);

		p1 = tfu->fmt;

		/* alphabetic escape sequences have to be done in place */
		for (p2 = p1;; ++p1, ++p2) {
			if (!*p1) {
				*p2 = *p1;
				break;
			}
			if (*p1 == '\\') {
				switch(*++p1) {
				case 'a':
				     /* *p2 = '\a'; */
					*p2 = '\007';
					break;
				case 'b':
					*p2 = '\b';
					break;
				case 'f':
					*p2 = '\f';
					break;
				case 'n':
					*p2 = '\n';
					break;
				case 'r':
					*p2 = '\r';
					break;
				case 't':
					*p2 = '\t';
					break;
				case 'v':
					*p2 = '\v';
					break;
				default:
					*p2 = *p1;
					break;
				}
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
