#include "internal.h"
#include <errno.h>

const char	rmdir_usage[] = "rmdir directory [directory ...]\n"
"\n"
"\tDelete directories.\n";

extern int
rmdir_fn(const struct FileInfo * i)
{
	if ( rmdir(i->source) != 0 && errno != ENOENT && !i->force ) {
		name_and_error(i->source);
		return 1;
	}
	else
		return 0;
}
