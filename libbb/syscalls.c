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


_syscall1(int, sysinfo, struct sysinfo *, info);

/* Include our own version of <sys/mount.h>, since libc5 doesn't
 * know about umount2 */
extern _syscall1(int, umount, const char *, special_file);
extern _syscall5(int, mount, const char *, special_file, const char *, dir,
		const char *, fstype, unsigned long int, rwflag, const void *, data);

#ifndef __NR_umount2
# warning This kernel does not support the umount2 syscall
# warning The umount2 system call is being stubbed out...
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
extern _syscall2(int, umount2, const char *, special_file, int, flags);
#endif

#ifndef __NR_query_module
static const int __NR_query_module = 167;
#endif
_syscall5(int, query_module, const char *, name, int, which,
		void *, buf, size_t, bufsize, size_t*, ret);

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
