#include "internal.h"
#include <sys/types.h>
#include <stdio.h>
#include <utime.h>

const char	touch_usage[] = "touch [-c] file [file ...]\n"
"\n"
"\tUpdate the last-modified date on the given file[s].\n";

extern int
touch_fn(const struct FileInfo * i)
{
	if ( (utime(i->source, 0) != 0) && (i->create != 1) ) {
		if ( fopen(i->source, "w") == NULL ) {
			name_and_error(i->source);
			return 1;
		}
	}
	return 0;
}
