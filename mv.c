#include "internal.h"
#include <stdio.h>
#include <errno.h>

const char	mv_usage[] = "mv source-file destination-file\n"
"\t\tmv source-file [source-file ...] destination-directory\n"
"\n"
"\tMove the source files to the destination.\n"
"\n";

extern int
mv_fn(const struct FileInfo * i)
{
	struct stat		destination_stat;
	char			d[1024];
	struct FileInfo	n;

	if ( stat(i->destination, &destination_stat) == 0 ) {
		if ( i->stat.st_ino == destination_stat.st_ino
		 &&  i->stat.st_dev == destination_stat.st_dev )
			return 0;	/* Move file to itself. */
	}
	if ( (destination_stat.st_mode & S_IFMT) == S_IFDIR ) {
		n = *i;
		n.destination = join_paths(d, i->destination, basename(i->source));
		i = &n;
	}
	if ( rename(i->source, i->destination) == 0 )
		return 0;
	else if ( errno == EXDEV && is_a_directory(i->source) ) {
		fprintf(stderr
		,"%s: Can't move directory across filesystems.\n"
		,i->source);
		return 1;
	}
	else
		return cp_fn(i);
}
