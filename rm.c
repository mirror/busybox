#include "internal.h"
#include <errno.h>

const char	rm_usage[] = "rm [-r] file [file ...]\n"
"\n"
"\tDelete files.\n"
"\n"
"\t-r:\tRecursively remove files and directories.\n";

extern int
rm_main(struct FileInfo * i, int argc, char * * argv)
{
	i->processDirectoriesAfterTheirContents = 1;
	return monadic_main(i, argc, argv);
}

extern int
rm_fn(const struct FileInfo * i)
{
	if ( i->recursive
	 && !i->isSymbolicLink
	 && (i->stat.st_mode & S_IFMT) == S_IFDIR )
		return rmdir_fn(i);
	else if ( unlink(i->source) != 0 && errno != ENOENT && !i->force ) {
		name_and_error(i->source);
		return 1;
	}
	else
		return 0;
}
