#include <string.h>
#include "libbb.h"
#include "unarchive.h"

archive_handle_t *init_handle(void)
{
	archive_handle_t *archive_handle;

	/* Initialise default values */
	archive_handle = xmalloc(sizeof(archive_handle_t));
	memset(archive_handle, 0, sizeof(archive_handle_t));
	archive_handle->file_header = xmalloc(sizeof(file_header_t));
	archive_handle->action_header = header_skip;
	archive_handle->action_data = data_skip;
	archive_handle->filter = filter_accept_all;

	return(archive_handle);
}
