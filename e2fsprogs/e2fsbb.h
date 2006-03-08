/*
 * File: e2fsbb.h
 *
 * Redefine a bunch of e2fsprogs stuff to use busybox routines
 * instead.  This makes upgrade between e2fsprogs versions easy.
 */

#ifndef __E2FSBB_H__
#define __E2FSBB_H__ 1

#include "libbb.h"

/* version we've last synced against */
#define E2FSPROGS_VERSION "1.38"
#define E2FSPROGS_DATE "30-Jun-2005"

/* make sure com_err.h isnt included before us */
#ifdef __COM_ERR_H__
#error You should not have included com_err.h !
#endif
#define __COM_ERR_H__

/* com_err crap */
#define com_err(w, c, fmt, args...) bb_error_msg(fmt, ## args)
typedef long errcode_t;
#define ERRCODE_RANGE 8
#define error_message(code) strerror((int) (code & ((1<<ERRCODE_RANGE)-1)))

/* misc crap */
#define fatal_error(err, msg) bb_error_msg_and_die(msg)
#define usage() bb_show_usage()

/* header defines */
#define ENABLE_HTREE 1
#define HAVE_ERRNO_H 1
#define HAVE_EXT2_IOCTLS 1
#define HAVE_INTTYPES_H 1
#define HAVE_LINUX_FD_H 1
#define HAVE_MALLOC_H 1
#define HAVE_MNTENT_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NET_IF_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_MOUNT_H 1
#define HAVE_SYS_QUEUE_H 1
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

/* Endianness */
#if __BYTE_ORDER == __BIG_ENDIAN
#define ENABLE_SWAPFS 1
#define WORDS_BIGENDIAN 1
#endif

#endif /* __E2FSBB_H__ */
