/*
 * busybox library eXtendet funcion
 *
 * concatenate path and file name to new allocation buffer,
 * not addition '/' if path name already have '/'
 *
*/

#include "libbb.h"

extern char *concat_path_file(const char *path, const char *filename)
{
	char *outbuf;
	char *lc;
	
	lc = last_char_is(path, '/');
	if (filename[0] == '/')
		filename++;
	outbuf = xmalloc(strlen(path)+strlen(filename)+1+(lc==NULL));
	sprintf(outbuf, (lc==NULL ? "%s/%s" : "%s%s"), path, filename);
	return outbuf;
}
