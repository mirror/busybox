#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "unarchive.h"
#include "libbb.h"

extern const unsigned short data_align(const int src_fd, const unsigned int offset, const unsigned short align_to)
{
	const unsigned short skip_amount = (align_to - (offset % align_to)) % align_to;
	seek_sub_file(src_fd, skip_amount);

	return(skip_amount);
}
