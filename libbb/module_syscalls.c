/* vi: set sw=4 ts=4: */
/*
 * some system calls possibly missing from libc
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
/* Kernel headers before 2.1.mumble need this on the Alpha to get
   _syscall* defined.  */
#define __LIBRARY__
#include <sys/syscall.h>
#include "libbb.h"


/* These syscalls are not included in very old glibc versions */
#if ((__GLIBC__ <= 2) && (__GLIBC_MINOR__ < 1))
int delete_module(const char *name)
{
    return(syscall(__NR_delete_module, name));
}
int get_kernel_syms(__ptr_t ks)
{
    return(syscall(__NR_get_kernel_syms, ks));
}

/* This may have 5 arguments (for old 2.0 kernels) or 2 arguments
 * (for 2.2 and 2.4 kernels).  Use the greatest common denominator,
 * and let the kernel cope with whatever it gets.  Its good at that. */
int init_module(void *first, void *second, void *third, void *fourth, void *fifth)
{
    return(syscall(__NR_init_module, first, second, third, fourth, fifth));
}

int query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret)
{
#ifndef __NR_query_module
#warning This kernel does not support the query_module syscall
#warning -> The query_module system call is being stubbed out...
    bb_error_msg("\n\nTo make this application work, you will need to recompile\n"
	    "BusyBox with a kernel supporting the query_module system call.\n");
    errno=ENOSYS;
    return -1;
#else
    return(syscall(__NR_query_module, name, which, buf, bufsize, ret));
#endif
}

/* Jump through hoops to fixup error return codes */
unsigned long create_module(const char *name, size_t size)
{
    long ret = syscall(__NR_create_module, name, size);

    if (ret == -1 && errno > 125) {
	ret = -errno;
	errno = 0;
    }
    return ret;
}

#endif


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/

