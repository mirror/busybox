/* vi: set sw=4 ts=4: */
/*
 * Various system configuration helpers.
 *
 * Copyright (C) 2014 Bartosz Golaszewski <bartekgola@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

#if !defined(ARG_MAX) && defined(_SC_ARG_MAX)
unsigned FAST_FUNC bb_arg_max(void)
{
	return sysconf(_SC_ARG_MAX);
}
#endif

/* Return the number of clock ticks per second. */
unsigned FAST_FUNC bb_clk_tck(void)
{
	return sysconf(_SC_CLK_TCK);
}
