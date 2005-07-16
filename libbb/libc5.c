/* vi: set sw=4 ts=4: */


#include <features.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>


#if ! defined __dietlibc__ &&  __GNU_LIBRARY__ < 5


/* Copyright (C) 1991 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/*
 * Modified by Manuel Novoa III     Mar 1, 2001
 *
 * Converted original strtok.c code of strtok to __strtok_r.
 * Cleaned up logic and reduced code size.
 */


char *strtok_r(char *s, const char *delim, char **save_ptr)
{
	char *token;

	token = 0;					/* Initialize to no token. */

	if (s == 0) {				/* If not first time called... */
		s = *save_ptr;			/* restart from where we left off. */
	}
	
	if (s != 0) {				/* If not finished... */
		*save_ptr = 0;

		s += strspn(s, delim);	/* Skip past any leading delimiters. */
		if (*s != '\0') {		/* We have a token. */
			token = s;
			*save_ptr = strpbrk(token, delim); /* Find token's end. */
			if (*save_ptr != 0) {
				/* Terminate the token and make SAVE_PTR point past it.  */
				*(*save_ptr)++ = '\0';
			}
		}
	}

	return token;
}

/* Basically getdelim() with the delimiter hard wired to '\n' */
ssize_t getline(char **linebuf, size_t *n, FILE *file)
{
      return (getdelim (linebuf, n, '\n', file));
}


#ifndef __uClinux__
/*
 * daemon implementation for uClibc
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Modified for uClibc by Erik Andersen <andersee@debian.org>
 *
 * The uClibc Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * The GNU C Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with the GNU C Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 *
 * Original copyright notice is retained at the end of this file.
 */

int daemon( int nochdir, int noclose )
{
    int fd;

    switch (fork()) {
	case -1:
	    return(-1);
	case 0:
	    break;
	default:
	    _exit(0);
    }

    if (setsid() == -1)
	return(-1);

    if (!nochdir)
	chdir("/");

    if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	if (fd > 2)
	    close(fd);
    }
    return(0);
}
#endif /* __uClinux__ */


/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. <BSD Advertising Clause omitted per the July 22, 1999 licensing change 
 *		ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change> 
 *
 * 4. Neither the name of the University nor the names of its contributors
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


#endif	

