#include "internal.h"
#include <sys/ioctl.h>
#include <linux/fd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char			fdflush_usage[] = "fdflush device";

int 
fdflush(const char *filename) 
{
	int	status;
	int	fd = open(filename, 0);

	if ( fd < 0 ) {
		name_and_error(filename);
		return 1;
	}

	status = ioctl(fd, FDFLUSH, 0);
	close(fd);

	if ( status != 0 ) {
		name_and_error(filename);
		return 1;
	}
	return 0;
}


int
fdflush_fn(const struct FileInfo * i)
{
  return fdflush(i->source);
}
