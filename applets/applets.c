/* vi: set sw=4 ts=4: */
/*
 * Stub for linking busybox binary against libbusybox.
 *
 * Copyright (C) 2007 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file License in this tarball for details.
 */

#include <assert.h>
#include "busybox.h"

/* Apparently uclibc defines __GLIBC__ (compat trick?). Oh well. */
#if ENABLE_STATIC && defined(__GLIBC__) && !defined(__UCLIBC__)
#warning Static linking against glibc produces buggy executables
#warning (glibc does not cope well with ld --gc-sections).
#warning See sources.redhat.com/bugzilla/show_bug.cgi?id=3400
#warning Note that glibc is unsuitable for static linking anyway.
#warning If you still want to do it, remove -Wl,--gc-sections
#warning from scripts/trylink and remove this warning.
#error Aborting compilation.
#endif

#if ENABLE_BUILD_LIBBUSYBOX
int main(int argc, char **argv)
{
	return lbb_main(argc, argv);
}
#endif
