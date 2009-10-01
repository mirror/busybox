/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2008 Denys Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include <unistd.h>

/* Just #include "autoconf.h" doesn't work for builds in separate
 * object directory */
#include "autoconf.h"

/* Since we can't use platform.h, have to do this again by hand: */
#if ENABLE_NOMMU
# define BB_MMU 0
# define USE_FOR_NOMMU(...) __VA_ARGS__
# define USE_FOR_MMU(...)
#else
# define BB_MMU 1
# define USE_FOR_NOMMU(...)
# define USE_FOR_MMU(...) __VA_ARGS__
#endif

static const char usage_messages[] = ""
#define MAKE_USAGE
#include "usage.h"
#include "applets.h"
;

int main(void)
{
	write(STDOUT_FILENO, usage_messages, sizeof(usage_messages));
	return 0;
}
