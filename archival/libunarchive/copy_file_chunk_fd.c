#include <unistd.h>
#include <sys/types.h>
#include "libbb.h"

/* Copy CHUNKSIZE bytes (or untill EOF if chunksize == -1)
 * from SRC_FILE to DST_FILE. */
extern int copy_file_chunk_fd(int src_fd, int dst_fd, off_t chunksize)
{
	size_t nread, size;
	char buffer[BUFSIZ];

	while (chunksize != 0) {
		if (chunksize > BUFSIZ) {
			size = BUFSIZ;
		} else {
			size = chunksize;
		}
		nread = xread(src_fd, buffer, size);
		if (nread == 0) {
			return 1;
		}

		if (write (dst_fd, buffer, nread) != nread) {
			error_msg_and_die ("Short write");
		}

		if (chunksize != -1) {
			chunksize -= nread;
		}
	}

	return 0;
}
