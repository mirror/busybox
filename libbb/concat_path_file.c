/*
 * busybox library eXtendet funcion
 *
 * concatenate path and file name to new allocation buffer,
 * not addition '/' if path name already have '/'
 *
*/

#include <string.h>
#include "libbb.h"

extern char *concat_path_file(const char *path, const char *filename)
{
	char *outbuf;
	char *lc;

	if (!path)
	    path="";
	lc = last_char_is(path, '/');
	while (*filename == '/')
		filename++;
	outbuf = xmalloc(strlen(path)+strlen(filename)+1+(lc==NULL));
	sprintf(outbuf, "%s%s%s", path, (lc==NULL)? "/" : "", filename);

	return outbuf;
}
