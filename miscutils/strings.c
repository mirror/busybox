/* vi: set sw=4 ts=4: */
/*
 * strings implementation for busybox
 *
 * Copyright (c) 1980, 1987
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
 *
 * Modified for BusyBox by Erik Andersen <andersen@codepoet.org>
 * Badly hacked by Tito Ragusa <farmatito@tiscali.it>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <ctype.h>
#include "busybox.h"

#define ISSTR(ch)	(isprint(ch) || ch == '\t')

int strings_main(int argc, char **argv)
{
	int n=4, c, i, opt=0, status=EXIT_SUCCESS;
	long t=0, count;
	FILE *file = stdin;
	char *string=NULL;
	const char *fmt="%s: ";

	while ((i = getopt(argc, argv, "afon:")) > 0)
		switch(i)
		{
			case 'a':
				break;
			case 'f':
				opt+=1;
				break;
			case 'o':
				opt+=2;
				break;
			case 'n':
				n = bb_xgetlarg(optarg, 10, 1, INT_MAX);
				break;
			default:
				bb_show_usage();
		}

	argc -= optind;
	argv += optind;

	i=0;

	string=xmalloc(n+1);
	/*string[n]='\0';*/
	n-=1;

	if(argc==0)
	{
		fmt="{%s}: ";
		*argv=(char *)bb_msg_standard_input;
		goto pipe;
	}

	for(  ;*argv!=NULL;*argv++)
	{
		if((file=bb_wfopen(*argv,"r")))
		{
pipe:

			count=0;
			do{
				c=fgetc(file);
				if(ISSTR(c))
				{
					if(i==0)
						t=count;
					if(i<=n)
						string[i]=c;
					if(i==n)
					{
						if(opt == 1 || opt == 3 )
							printf(fmt,*argv);
						if(opt >= 2 )
							printf("%7lo ", t);
						printf("%s", string);
					}
					if(i>n)
						putchar(c);
					i++;
				}
				else
				{
					if(i>n)
						putchar('\n');
					i=0;
				}
				count++;
			}while(c!=EOF);

			bb_fclose_nonstdin(file);
		}
		else
			status=EXIT_FAILURE;
	}
	/*free(string);*/
	exit(status);
}

/*
 * Copyright (c) 1980, 1987
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
