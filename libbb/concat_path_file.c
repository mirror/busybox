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
	int  l;
	int  flg_slash = 1;

	l = strlen(path);
	if(l>0 && path[l-1] == '/')
		flg_slash--;
	l += strlen(filename);
	outbuf = xmalloc(l+1+flg_slash);
	sprintf(outbuf, (flg_slash ? "%s/%s" : "%s%s"), path, filename);
	return outbuf;
}
