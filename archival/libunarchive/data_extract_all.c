#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <stdlib.h>
#include "libbb.h"
#include "unarchive.h"

extern void data_extract_all(archive_handle_t *archive_handle)
{
	file_header_t *file_header = archive_handle->file_header;
	int dst_fd;
	int res;

	if (archive_handle->flags & ARCHIVE_CREATE_LEADING_DIRS) {
		char *name = strdup(file_header->name);
		make_directory (dirname(name), 0777, FILEUTILS_RECUR);
		free(name);
	}                  

	/* Create the file */
	switch(file_header->mode & S_IFMT) {
		case S_IFREG: {
#ifdef CONFIG_CPIO
			if (file_header->link_name) {
				/* hard link */
				res = link(file_header->link_name, file_header->name);
				if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
					perror_msg("Couldnt create hard link");
				}
			} else
#endif
			{
				/* Regular file */
				dst_fd = xopen(file_header->name, O_WRONLY | O_CREAT);
				copy_file_chunk_fd(archive_handle->src_fd, dst_fd, file_header->size);
				close(dst_fd);
			}
			break;
		}
		case S_IFDIR:
			res = mkdir(file_header->name, file_header->mode);
			if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				perror_msg("extract_archive: %s", file_header->name);
			}
			break;
		case S_IFLNK:
			/* Symlink */
			res = symlink(file_header->link_name, file_header->name);
			if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				perror_msg("Cannot create symlink from %s to '%s'", file_header->name, file_header->link_name);
			}
			break;
		case S_IFSOCK:
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
			res = mknod(file_header->name, file_header->mode, file_header->device);
			if ((res == -1) && !(archive_handle->flags & ARCHIVE_EXTRACT_QUIET)) {
				perror_msg("Cannot create node %s", file_header->name);
			}
			break;
	}

	chmod(file_header->name, file_header->mode);
	chown(file_header->name, file_header->uid, file_header->gid);

	if (archive_handle->flags & ARCHIVE_PRESERVE_DATE) {
		struct utimbuf t;
		t.actime = t.modtime = file_header->mtime;
		utime(file_header->name, &t);
	}
}
