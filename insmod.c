/*
 * Mini insmod implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
 */

#include "internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <linux/module.h>

/* Some firendly syscalls to cheer everyone's day...  */
_syscall2(int, init_module, const char *, name,
	const struct module *, info)

#ifndef BB_RMMOD
_syscall1(int, delete_module, const char *, name)
#else
extern int delete_module(const char *);
#endif

#if defined(__i386__) || defined(__m68k__) || defined(__arm__)
/* Jump through hoops to fixup error return codes */
#define __NR__create_module  __NR_create_module
static inline _syscall2(long, _create_module, const char *, name, size_t, size)
unsigned long create_module(const char *name, size_t size)
{
    long ret = _create_module(name, size);
    if (ret == -1 && errno > 125) {
	ret = -errno;
	errno = 0;
    }
    return ret;
}
#else
_syscall2(unsigned long, create_module, const char *, name, size_t, size)
#endif


static const char insmod_usage[] =
    "insmod [OPTION]... MODULE [symbol=value]...\n\n"
    "Loads the specified kernel modules into the kernel.\n\n"
    "Options:\n"
    "\t-f\tForce module to load into the wrong kernel version.\n"
    "\t-k\tMake module autoclean-able.\n";



extern int insmod_main(int argc, char **argv)
{
    int len;
    char m_name[PATH_MAX];
    char* tmp;



    if (argc<=1) {
	usage( insmod_usage);
    }

    /* Parse any options */
    while (--argc > 0 && **(++argv) == '-') {
	while (*(++(*argv))) {
	    switch (**argv) {
	    case 'f':
		break;
	    case 'k':
		break;
	    default:
		usage(insmod_usage);
	    }
	}
    }

    if (argc <= 0 )
	usage(insmod_usage);

    /* Grab the module name */
    if ((tmp = strrchr(*argv, '/')) != NULL)
	tmp++;
    else 
	tmp = *argv;
    len = strlen(tmp);

    if (len > 2 && tmp[len - 2] == '.' && tmp[len - 1] == 'o')
	len -= 2;
    memcpy(m_name, tmp, len);

    fprintf(stderr, "m_name='%s'\n", m_name);

    
    exit( TRUE);
}
