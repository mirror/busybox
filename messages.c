/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2000 by BitterSweet Enterprises, LLC.
 * Written by Karl M. Hegbloom <karlheg@debian.org>
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

/*
 *  Let's put all of these messages in one place, and link this in as
 *  a separate object module, so that there are not going to be
 *  multiple non-unique but very similar strings in the binary.
 *  Perhaps this will make it simpler to internationalize also, and
 *  may make the binary slightly smaller.
 */

// To use this header file, include something like this:
//
//#define BB_DECLARE_EXTERN
//#define bb_need_memory_exhausted
//#include "messages.c"
//
//Then just use the string memory_exhausted when it is needed.
//

#include "busybox.h"
#ifndef _BB_MESSAGES_C
#define _BB_MESSAGES_C

#ifdef BB_DECLARE_EXTERN
#  define BB_DEF_MESSAGE(symbol, string_const) extern const char *symbol;
#else
#  define BB_DEF_MESSAGE(symbol, string_const) const char *symbol = string_const;
#endif


#if defined bb_need_full_version || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(full_version,
	 "BusyBox v" BB_VER " (" BB_BT ") multi-call binary")
#endif
#if defined bb_need_name_too_long || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(name_too_long, "file name too long")
#endif
#if defined bb_need_omitting_directory || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(omitting_directory, "%s: omitting directory")
#endif
#if defined bb_need_not_a_directory || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(not_a_directory, "%s: not a directory")
#endif
#if defined bb_need_memory_exhausted || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(memory_exhausted, "memory exhausted")
#endif
#if defined bb_need_invalid_date || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(invalid_date, "invalid date `%s'")
#endif
#if defined bb_need_invalid_option || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(invalid_option, "invalid option -- %c")
#endif
#if defined bb_need_io_error || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(io_error, "%s: input/output error -- %s")
#endif
#if defined bb_need_help || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(dash_dash_help, "--help")
#endif
#if defined bb_need_write_error || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(write_error, "Write Error")
#endif
#if defined bb_need_too_few_args || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(too_few_args, "too few arguments")
#endif
#if defined bb_need_name_longer_than_foo || ! defined BB_DECLARE_EXTERN
	BB_DEF_MESSAGE(name_longer_than_foo, "Names longer than %d chars not supported.")
#endif


#endif /* _BB_MESSAGES_C */

