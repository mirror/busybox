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

#define _PATH_MODULES	"/lib/modules"

#warning "Danger Will Robinson, Danger!!!"
#warning " "
#warning "insmod is still under construction.  Don't use it."
#warning " "
#warning "	You have been warned!"
#warning " "


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


static char m_filename[PATH_MAX] = "\0";
static char m_fullName[PATH_MAX] ="\0";
static const char insmod_usage[] =
    "insmod [OPTION]... MODULE [symbol=value]...\n\n"
    "Loads the specified kernel modules into the kernel.\n\n"
    "Options:\n"
    "\t-f\tForce module to load into the wrong kernel version.\n"
    "\t-k\tMake module autoclean-able.\n";


static int findNamedModule(const char *fileName, struct stat* statbuf)
{
    if (m_fullName[0]=='\0')
	return( FALSE);
    else {
	char* tmp = strrchr( fileName, '/');
	if (tmp == NULL)
	    tmp = (char*)fileName;
	else
	    tmp++;
	if (check_wildcard_match(tmp, m_fullName) == TRUE) {
	    /* Stop searching if we find a match */
	    memcpy(m_filename, fileName, strlen(fileName));
	    return( FALSE);
	}
    }
    return( TRUE);
}


extern int insmod_main(int argc, char **argv)
{
    int len;
    char *tmp;
    char m_name[PATH_MAX] ="\0";
    FILE *fp;

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
    strcpy(m_fullName, m_name);
    strcat(m_fullName, ".o");

    /* Get a filedesc for the module */
    if ((fp = fopen(*argv, "r")) == NULL) {
	/* Hmpf.  Could not open it. Search through _PATH_MODULES to find a module named m_name */
	if (recursiveAction(_PATH_MODULES, TRUE, FALSE, FALSE,
		    findNamedModule, findNamedModule) == FALSE) {
	    if ( m_filename[0] == '\0' ||  ((fp = fopen(m_filename, "r")) == NULL)) {
		    perror("No module by that name found in " _PATH_MODULES "\n");
		    exit( FALSE);
	    }
	}
    } else
	memcpy(m_filename, *argv, strlen(*argv));


    fprintf(stderr, "m_filename='%s'\n", m_filename);
    fprintf(stderr, "m_name='%s'\n", m_name);


    /* TODO: do something roughtly like this... */
#if 0

    if ((f = obj_load(fp)) == NULL) {
	perror("Could not load the module\n");
	exit( FALSE);
    }
    
    /* Let the module know about the kernel symbols.  */
    add_kernel_symbols(f);

    if (!create_this_module(f, m_name)) {
	perror("Could not create the module\n");
	exit( FALSE);
    }

    if (!obj_check_undefineds(f, quiet)) {
	perror("Undefined symbols in the module\n");
	exit( FALSE);
    }
    obj_allocate_commons(f);

    /* Perse the module's arguments */
    while (argc-- >0 && *(argv++) != '\0') {
	if (!process_module_arguments(f, argc - optind, argv + optind)) {
	    perror("Undefined symbols in the module\n");
	    exit( FALSE);
	}
    }

    /* Find current size of the module */
    m_size = obj_load_size(f);


    errno = 0;
    m_addr = create_module(m_name, m_size);
    switch (errno) {
	/* yada yada */
	default:
	    perror("create_module: %m");

    }

#endif

    fclose( fp);
    exit( TRUE);
}
