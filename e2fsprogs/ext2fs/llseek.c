/*
 * llseek.c -- stub calling the llseek system call
 *
 * Copyright (C) 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ext2fs/ext2_io.h"

#ifdef CONFIG_LFS
# define my_llseek lseek64
#else
# define my_llseek lseek
#endif

ext2_loff_t ext2fs_llseek (int fd, ext2_loff_t offset, int origin)
{
	ext2_loff_t result;
	static int do_compat = 0;

	if ((sizeof(off_t) >= sizeof(ext2_loff_t)) ||
	    (offset < ((ext2_loff_t) 1 << ((sizeof(off_t)*8) -1))))
		return lseek(fd, (off_t) offset, origin);

	if (do_compat) {
		errno = EINVAL;
		return -1;
	}
	
	result = my_llseek (fd, offset, origin);
	if (result == -1 && errno == ENOSYS) {
		/*
		 * Just in case this code runs on top of an old kernel
		 * which does not support the llseek system call
		 */
		do_compat++;
		errno = EINVAL;
	}
	return result;
}
