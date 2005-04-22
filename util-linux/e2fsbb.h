/*
 * File: e2fsbb.h
 *
 * Redefine a bunch of e2fsprogs stuff to use busybox routines
 * instead.  This makes upgrade between e2fsprogs versions easy.
 */

#ifndef __E2FSBB_H__
#define __E2FSBB_H__ 1

#include "libbb.h"

#define _(x) x

#define com_err(w, c, fmt, args...) bb_error_msg(fmt, ## args)

#define fputs(msg, fd) bb_error_msg(msg)
#define fatal_error(msg, err) bb_error_msg_and_die(msg)
#define usage() bb_show_usage()
#define perror(msg) bb_perror_msg(msg)

#endif /* __E2FSBB_H__ */
