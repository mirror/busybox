/* vi: set sw=4 ts=4: */
/* Copyright 2002 Laurence Anderson
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

typedef struct hardlinks_s {
	char *name;
	int inode;
	struct hardlinks_s *next;
} hardlinks_t;

char get_header_cpio(archive_handle_t *archive_handle)
{
	static hardlinks_t *saved_hardlinks = NULL;
	static unsigned short pending_hardlinks = 0;
	static int inode;
	file_header_t *file_header = archive_handle->file_header;
	char cpio_header[110];
	int namesize;
	char dummy[16];
	int major, minor, nlink;

	if (pending_hardlinks) { /* Deal with any pending hardlinks */
		hardlinks_t *tmp, *oldtmp;

		tmp = saved_hardlinks;
		oldtmp = NULL;

		file_header->link_name = file_header->name;
		file_header->size = 0;

		while (tmp) {
			if (tmp->inode != inode) {
				tmp = tmp->next;
				continue;
			}

			file_header->name = tmp->name;

			if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
				archive_handle->action_data(archive_handle);
				archive_handle->action_header(archive_handle->file_header);
			}

			pending_hardlinks--;

			oldtmp = tmp;
			tmp = tmp->next;
			free(oldtmp->name);
			free(oldtmp);
			if (oldtmp == saved_hardlinks)
				saved_hardlinks = tmp;
		}

		file_header->name = file_header->link_name;

		if (pending_hardlinks > 1) {
			bb_error_msg("error resolving hardlink: archive made by GNU cpio 2.0-2.2?");
		}

		/* No more pending hardlinks, read next file entry */
		pending_hardlinks = 0;
	}

	/* There can be padding before archive header */
	data_align(archive_handle, 4);

	if (archive_xread_all_eof(archive_handle, (unsigned char*)cpio_header, 110) == 0) {
		return EXIT_FAILURE;
	}
	archive_handle->offset += 110;

	if (strncmp(&cpio_header[0], "07070", 5) != 0
	 || (cpio_header[5] != '1' && cpio_header[5] != '2')
	) {
		bb_error_msg_and_die("unsupported cpio format, use newc or crc");
	}

	{
		unsigned long tmpsize;
		sscanf(cpio_header, "%6c%8x%8x%8x%8x%8x%8lx%8lx%16c%8x%8x%8x%8c",
		    dummy, &inode, (unsigned int*)&file_header->mode,
		    (unsigned int*)&file_header->uid, (unsigned int*)&file_header->gid,
		    &nlink, &file_header->mtime, &tmpsize,
		    dummy, &major, &minor, &namesize, dummy);
		file_header->size = tmpsize;
	}

	free(file_header->name);
	file_header->name = xzalloc(namesize + 1);
	/* Read in filename */
	xread(archive_handle->src_fd, file_header->name, namesize);
	archive_handle->offset += namesize;

	/* Update offset amount and skip padding before file contents */
	data_align(archive_handle, 4);

	if (strcmp(file_header->name, "TRAILER!!!") == 0) {
		/* Always round up */
		printf("%d blocks\n", (int) (archive_handle->offset % 512 ?
		                             archive_handle->offset / 512 + 1 :
		                             archive_handle->offset / 512
		                            ));
		if (saved_hardlinks) { /* Bummer - we still have unresolved hardlinks */
			hardlinks_t *tmp = saved_hardlinks;
			hardlinks_t *oldtmp = NULL;
			while (tmp) {
				bb_error_msg("%s not created: cannot resolve hardlink", tmp->name);
				oldtmp = tmp;
				tmp = tmp->next;
				free(oldtmp->name);
				free(oldtmp);
			}
			saved_hardlinks = NULL;
			pending_hardlinks = 0;
		}
		return EXIT_FAILURE;
	}

	if (S_ISLNK(file_header->mode)) {
		file_header->link_name = xzalloc(file_header->size + 1);
		xread(archive_handle->src_fd, file_header->link_name, file_header->size);
		archive_handle->offset += file_header->size;
		file_header->size = 0; /* Stop possible seeks in future */
	} else {
		file_header->link_name = NULL;
	}
	if (nlink > 1 && !S_ISDIR(file_header->mode)) {
		if (file_header->size == 0) { /* Put file on a linked list for later */
			hardlinks_t *new = xmalloc(sizeof(hardlinks_t));
			new->next = saved_hardlinks;
			new->inode = inode;
			/* name current allocated, freed later */
			new->name = file_header->name;
			file_header->name = NULL;
			saved_hardlinks = new;
			return EXIT_SUCCESS; /* Skip this one */
		}
		/* Found the file with data in */
		pending_hardlinks = nlink;
	}
	file_header->device = makedev(major, minor);

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_data(archive_handle);
		archive_handle->action_header(archive_handle->file_header);
	} else {
		data_skip(archive_handle);
	}

	archive_handle->offset += file_header->size;

	free(file_header->link_name);

	return EXIT_SUCCESS;
}
