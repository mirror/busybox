/*
 *  xreadlink.c - safe implementation of readlink.
 *  Returns a NULL on failure...
 */

#include <stdio.h>

/*
 * NOTE: This function returns a malloced char* that you will have to free
 * yourself. You have been warned.
 */

#include <unistd.h>
#include "libbb.h"

extern char *xreadlink(const char *path)
{                       
	static const int GROWBY = 80; /* how large we will grow strings by */

	char *buf = NULL;   
	int bufsize = 0, readsize = 0;

	do {
		buf = xrealloc(buf, bufsize += GROWBY);
		readsize = readlink(path, buf, bufsize); /* 1st try */
		if (readsize == -1) {
                   bb_perror_msg("%s", path);
		    return NULL;
		}
	}           
	while (bufsize < readsize + 1);

	buf[readsize] = '\0';

	return buf;
}       
