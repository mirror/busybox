/*
   Copyright (C) 2002 Vladimir Oleynik <dzo@simtreas.ru>
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "libbb.h"

void bb_xasprintf(char **string_ptr, const char *format, ...)
{
	va_list p;
	int r;

	va_start(p, format);
	r = vasprintf(string_ptr, format, p);
	va_end(p);

	if (r < 0) {
		bb_perror_msg_and_die("bb_xasprintf");
	}
}
