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

extern void data_extract_all_prefix(archive_handle_t *archive_handle)
{
	char *name_ptr = archive_handle->file_header->name;

	name_ptr += strspn(name_ptr, "./");
	if (name_ptr[0] != '\0') {
		archive_handle->file_header->name = xmalloc(strlen(archive_handle->buffer) + 2 + strlen(name_ptr));
		sprintf(archive_handle->file_header->name, "%s%s", archive_handle->buffer, name_ptr);
		data_extract_all(archive_handle);
	}
	return;
}
