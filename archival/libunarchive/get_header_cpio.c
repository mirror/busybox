/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unarchive.h"
#include "libbb.h"

typedef struct hardlinks_s {
	file_header_t *entry;
	int inode;
	struct hardlinks_s *next;
} hardlinks_t;

extern char get_header_cpio(archive_handle_t *archive_handle)
{
	static hardlinks_t *saved_hardlinks = NULL;
	static unsigned short pending_hardlinks = 0;
	file_header_t *file_header = archive_handle->file_header;
	char cpio_header[110];
	int namesize;
	char dummy[16];
	int major, minor, nlink, inode;
	
	if (pending_hardlinks) { /* Deal with any pending hardlinks */
		hardlinks_t *tmp;
		hardlinks_t *oldtmp;

		tmp = saved_hardlinks;
		oldtmp = NULL;

		while (tmp) {
			bb_error_msg_and_die("need to fix this\n");
			if (tmp->entry->link_name) { /* Found a hardlink ready to be extracted */
				file_header = tmp->entry;
				if (oldtmp) {
					oldtmp->next = tmp->next; /* Remove item from linked list */
				} else {
					saved_hardlinks = tmp->next;
				}
				free(tmp);
				continue;
			}
			oldtmp = tmp;
			tmp = tmp->next;
		}
		pending_hardlinks = 0; /* No more pending hardlinks, read next file entry */
	}

	/* There can be padding before archive header */
	data_align(archive_handle, 4);

	if (archive_xread_all_eof(archive_handle, cpio_header, 110) == 0) {
		return(EXIT_FAILURE);
	}
	archive_handle->offset += 110;

	if ((strncmp(&cpio_header[0], "07070", 5) != 0) || ((cpio_header[5] != '1') && (cpio_header[5] != '2'))) {
		bb_error_msg_and_die("Unsupported cpio format, use newc or crc");
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

	file_header->name = (char *) xmalloc(namesize + 1);
	archive_xread_all(archive_handle, file_header->name, namesize); /* Read in filename */
	file_header->name[namesize] = '\0';
	archive_handle->offset += namesize;

	/* Update offset amount and skip padding before file contents */
	data_align(archive_handle, 4);

	if (strcmp(file_header->name, "TRAILER!!!") == 0) {
		printf("%d blocks\n", (int) (archive_handle->offset % 512 ? (archive_handle->offset / 512) + 1 : archive_handle->offset / 512)); /* Always round up */
		if (saved_hardlinks) { /* Bummer - we still have unresolved hardlinks */
			hardlinks_t *tmp = saved_hardlinks;
			hardlinks_t *oldtmp = NULL;
			while (tmp) {
				bb_error_msg("%s not created: cannot resolve hardlink", tmp->entry->name);
				oldtmp = tmp;
				tmp = tmp->next;
				free (oldtmp->entry->name);
				free (oldtmp->entry);
				free (oldtmp);
			}
			saved_hardlinks = NULL;
			pending_hardlinks = 0;
		}
		return(EXIT_FAILURE);
	}

	if (S_ISLNK(file_header->mode)) {
		file_header->link_name = (char *) xmalloc(file_header->size + 1);
		archive_xread_all(archive_handle, file_header->link_name, file_header->size);
		file_header->link_name[file_header->size] = '\0';
		archive_handle->offset += file_header->size;
		file_header->size = 0; /* Stop possible seeks in future */
	}
	if (nlink > 1 && !S_ISDIR(file_header->mode)) {
		if (file_header->size == 0) { /* Put file on a linked list for later */
			hardlinks_t *new = xmalloc(sizeof(hardlinks_t));
			new->next = saved_hardlinks;
			new->inode = inode;
			new->entry = file_header;
			saved_hardlinks = new;
			return(EXIT_SUCCESS); // Skip this one
		} else { /* Found the file with data in */
			hardlinks_t *tmp = saved_hardlinks;
			pending_hardlinks = 1;
			while (tmp) {
				if (tmp->inode == inode) {
					tmp->entry->link_name = bb_xstrdup(file_header->name);
					nlink--;
				}
				tmp = tmp->next;
			}
			if (nlink > 1) {
				bb_error_msg("error resolving hardlink: did you create the archive with GNU cpio 2.0-2.2?");
			}
		}
	}
	file_header->device = (major << 8) | minor;

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_data(archive_handle);
		archive_handle->action_header(archive_handle->file_header);
	} else {
		data_skip(archive_handle);			
	}

	archive_handle->offset += file_header->size;
	return (EXIT_SUCCESS);
}
