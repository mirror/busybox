#include "internal.h"
#include <errno.h>
#include <sys/param.h>

const char mkdir_usage[] = "mkdir [-m mode] directory [directory ...]\n"
"\tCreate directories.\n"
"\n"
"\t-m mode:\tSpecifiy the mode for the new directory\n"
"\t\tunder the argument directory.";

/*make directories skipping the last part of the path. Used here and by untar*/
int mkdir_until(const char *fpath, const struct FileInfo * fi)
{
	char    path[PATH_MAX];
	char *  s = path;

	strcpy(path, fpath);
	if ( s[0] == '\0' && s[1] == '\0' ) {
		usage(mkdir_usage);
		return 1;
	}
	s++;
	while ( *s != '\0' ) {
		if ( *s == '/' ) {
			int	status;

			*s = '\0';
			status = mkdir(path, (fi?fi->orWithMode:0700) );
			*s = '/';

			if ( status != 0 ) {
				if ( errno != EEXIST ) {
					name_and_error(fpath);
					return 1;
				}
			}

		}
		s++;
	}
	return 0;
}

int
mkdir_fn(const struct FileInfo * i)
{
	if ( i->makeParentDirectories ) {
		if(mkdir_until(i->source, i)) return 1;
	}

	if ( mkdir(i->source, i->orWithMode) != 0 && errno != EEXIST ) {
		name_and_error(i->source);
		return 1;
	}
	else
		return 0;
}

