#include "internal.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>

const char	ln_usage[] = "ln [-s] [-f] original-name additional-name\n"
"\n"
"\tAdd a new name that refers to the same file as \"original-name\"\n"
"\n"
"\t-s:\tUse a \"symbolic\" link, instead of a \"hard\" link.\n"
"\t-f:\tRemove existing destination files.\n";

int
ln_fn(const struct FileInfo * i)
{
	int				status = 0;
	char			d[PATH_MAX];
	const char *	destination = i->destination;

	if ( !i->makeSymbolicLink && (i->stat.st_mode & S_IFMT) == S_IFDIR ) {
		fprintf(stderr, "Please use \"ln -s\" to link directories.\n");
		return 1;
	}

	/*
	 * If the destination is a directory, create a file within it.
	 */
	if ( is_a_directory(i->destination) ) {
			destination = join_paths(
			 d
			,i->destination
			,&i->source[i->directoryLength]);
	}

	if ( i->force )
		status = ( unlink(destination) && errno != ENOENT );

	if ( status == 0 ) {
		if ( i->makeSymbolicLink )
			status = symlink(i->source, destination);
		else
			status = link(i->source, destination);
	}

	if ( status != 0 ) {
		name_and_error(destination);
		return 1;
	}
	else
		return 0;
}
