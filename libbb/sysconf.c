/* vi: set sw=4 ts=4: */
/*
 * Various system configuration helpers.
 *
 * Copyright (C) 2014 Bartosz Golaszewski <bartekgola@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

#if defined _SC_ARG_MAX
unsigned FAST_FUNC bb_arg_max(void)
{
	return sysconf(_SC_ARG_MAX);
}
#endif
