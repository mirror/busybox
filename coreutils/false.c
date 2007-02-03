/* vi: set sw=4 ts=4: */
/*
 * Mini false implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/false.html */

#include <stdlib.h>
#include "busybox.h"

int false_main(int ATTRIBUTE_UNUSED argc, char ATTRIBUTE_UNUSED **argv);
int false_main(int ATTRIBUTE_UNUSED argc, char ATTRIBUTE_UNUSED **argv)
{
	return EXIT_FAILURE;
}
