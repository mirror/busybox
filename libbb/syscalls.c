/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
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
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <linux/unistd.h>

#include "libbb.h"

_syscall3(int, sysfs, int, option, unsigned int, fs_index, char *, buf);

#ifndef __NR_pivot_root
#warning This kernel does not support the pivot_root syscall
#warning -> The pivot_root system call is being stubbed out...
int pivot_root(const char * new_root,const char * put_old)
{
	/* BusyBox was compiled against a kernel that did not support
	 *  the pivot_root system call.  To make this application work,
	 *  you will need to recompile with a kernel supporting the
	 *  pivot_root system call.
	 */
	fprintf(stderr, "\n\nTo make this application work, you will need to recompile\n");
	fprintf(stderr, "with a kernel supporting the pivot_root system call. -Erik\n\n");
	errno=ENOSYS;
	return -1;
}
#else
_syscall2(int,pivot_root,const char *,new_root,const char *,put_old)
#endif




#if __GNU_LIBRARY__ < 5
/* These syscalls are not included as part of libc5 */
_syscall2(int, bdflush, int, func, int, data);
_syscall1(int, delete_module, const char *, name)

#ifndef __NR_query_module
#warning This kernel does not support the query_module syscall
#warning -> The query_module system call is being stubbed out...
int query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret)
{
	/* BusyBox was compiled against a kernel that did not support
	 *  the query_module system call.  To make this application work,
	 *  you will need to recompile with a kernel supporting the
	 *  query_module system call.
	 */
	fprintf(stderr, "\n\nTo make this application work, you will need to recompile\n");
	fprintf(stderr, "with a kernel supporting the query_module system call. -Erik\n\n");
	errno=ENOSYS;
	return -1;
}
#else
_syscall5(int, query_module, const char *, name, int, which,
		void *, buf, size_t, bufsize, size_t*, ret);
#endif

#ifndef __alpha__
# define __NR_klogctl __NR_syslog
  _syscall3(int, klogctl, int, type, char *, b, int, len);
#endif

#ifndef __NR_umount2
# warning This kernel does not support the umount2 syscall
# warning -> The umount2 system call is being stubbed out...
int umount2(const char * special_file, int flags)
{
	/* BusyBox was compiled against a kernel that did not support
	 *  the umount2 system call.  To make this application work,
	 *  you will need to recompile with a kernel supporting the
	 *  umount2 system call.
	 */
	fprintf(stderr, "\n\nTo make this application work, you will need to recompile\n");
	fprintf(stderr, "with a kernel supporting the umount2 system call. -Erik\n\n");
	errno=ENOSYS;
	return -1;
}
# else
_syscall2(int, umount2, const char *, special_file, int, flags);
#endif


#endif /* __GNU_LIBRARY__ < 5 */


#if 0
_syscall1(int, sysinfo, struct sysinfo *, info);
#endif


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
