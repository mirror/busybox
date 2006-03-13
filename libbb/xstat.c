/*
 * xstat.c - a stat() which dies on failure with meaningful error message
 */
#include <unistd.h>
#include "libbb.h"

void xstat(const char *name, struct stat *stat_buf)
{
	if (stat(name, stat_buf))
		bb_perror_msg_and_die("Can't stat '%s'", name);
}
