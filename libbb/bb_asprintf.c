/*
   Copyright (C) 2002 Vladimir Oleynik <dzo@simtreas.ru>
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


#include "libbb.h"


void bb_asprintf(char **string_ptr, const char *format, ...)
{
       va_list p;

       va_start(p, format);
       if(vasprintf(string_ptr, format, p)<0)
		error_msg_and_die(memory_exhausted);
       va_end(p);
}
