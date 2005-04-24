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
#define E2FSPROGS_VERSION "1.37"
#define E2FSPROGS_DATE "21-Mar-2005"

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

/* NLS crap */
#define _(x) x

/* misc crap */
#define fputs(msg, fd) bb_error_msg(msg)
#define fatal_error(msg, err) bb_error_msg_and_die(msg)
#define usage() bb_show_usage()
#define perror(msg) bb_perror_msg(msg)

#endif /* __E2FSBB_H__ */
