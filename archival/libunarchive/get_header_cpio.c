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
#include "unarchive.h"
#include "libbb.h"

struct hardlinks {
	file_header_t *entry;
	int inode;
	struct hardlinks *next;
};

file_header_t *get_header_cpio(FILE *src_stream)
{
	file_header_t *cpio_entry = NULL;
	char cpio_header[110];
	int namesize;
 	char dummy[16];
 	int major, minor, nlink, inode;
 	static struct hardlinks *saved_hardlinks = NULL;
 	static int pending_hardlinks = 0;

	if (pending_hardlinks) { /* Deal with any pending hardlinks */
		struct hardlinks *tmp = saved_hardlinks, *oldtmp = NULL;
		while (tmp) {
			if (tmp->entry->link_name) { /* Found a hardlink ready to be extracted */
				cpio_entry = tmp->entry;
				if (oldtmp) oldtmp->next = tmp->next; /* Remove item from linked list */
				else saved_hardlinks = tmp->next;
				free(tmp);
				return (cpio_entry);
			}
			oldtmp = tmp;
			tmp = tmp->next;
		}
		pending_hardlinks = 0; /* No more pending hardlinks, read next file entry */
	}
  
	/* There can be padding before archive header */
	seek_sub_file(src_stream, (4 - (archive_offset % 4)) % 4);
	if (fread(cpio_header, 1, 110, src_stream) == 110) {
		archive_offset += 110;
		if (strncmp(cpio_header, "07070", 5) != 0) {
			error_msg("Unsupported format or invalid magic");
			return(NULL);
		}
		switch (cpio_header[5]) {
			case '2': /* "crc" header format */
				/* Doesnt do the crc check yet */
			case '1': /* "newc" header format */
				cpio_entry = (file_header_t *) xcalloc(1, sizeof(file_header_t));
				sscanf(cpio_header, "%6c%8x%8x%8x%8x%8x%8lx%8lx%16c%8x%8x%8x%8c",
					dummy, &inode, (unsigned int*)&cpio_entry->mode, 
					(unsigned int*)&cpio_entry->uid, (unsigned int*)&cpio_entry->gid,
					&nlink, &cpio_entry->mtime, &cpio_entry->size,
					dummy, &major, &minor, &namesize, dummy);

				cpio_entry->name = (char *) xcalloc(1, namesize);
				fread(cpio_entry->name, 1, namesize, src_stream); /* Read in filename */
				archive_offset += namesize;
				/* Skip padding before file contents */
				seek_sub_file(src_stream, (4 - (archive_offset % 4)) % 4);
				if (strcmp(cpio_entry->name, "TRAILER!!!") == 0) {
					printf("%d blocks\n", (int) (archive_offset % 512 ? (archive_offset / 512) + 1 : archive_offset / 512)); /* Always round up */
					if (saved_hardlinks) { /* Bummer - we still have unresolved hardlinks */
						struct hardlinks *tmp = saved_hardlinks, *oldtmp = NULL;
						while (tmp) {
							error_msg("%s not created: cannot resolve hardlink", tmp->entry->name);
							oldtmp = tmp;
							tmp = tmp->next;
							free (oldtmp->entry->name);
							free (oldtmp->entry);
							free (oldtmp);
						}
						saved_hardlinks = NULL;
						pending_hardlinks = 0;
					}
					return(NULL);
				}

				if (S_ISLNK(cpio_entry->mode)) {
					cpio_entry->link_name = (char *) xcalloc(1, cpio_entry->size + 1);
					fread(cpio_entry->link_name, 1, cpio_entry->size, src_stream);
					archive_offset += cpio_entry->size;
					cpio_entry->size = 0; /* Stop possiable seeks in future */
				}
				if (nlink > 1 && !S_ISDIR(cpio_entry->mode)) {
					if (cpio_entry->size == 0) { /* Put file on a linked list for later */
						struct hardlinks *new = xmalloc(sizeof(struct hardlinks));
						new->next = saved_hardlinks;
						new->inode = inode;
						new->entry = cpio_entry;
						saved_hardlinks = new;
					return(get_header_cpio(src_stream)); /* Recurse to next file */
					} else { /* Found the file with data in */
						struct hardlinks *tmp = saved_hardlinks;
						pending_hardlinks = 1;
						while (tmp) {
							if (tmp->inode == inode) {
								tmp->entry->link_name = xstrdup(cpio_entry->name);
								nlink--;
							}
							tmp = tmp->next;
						}
						if (nlink > 1) error_msg("error resolving hardlink: did you create the archive with GNU cpio 2.0-2.2?");
					}
				}
				cpio_entry->device = (major << 8) | minor;
				break;
			default:
				error_msg("Unsupported format");
				return(NULL);
		}
		if (ferror(src_stream) || feof(src_stream)) {
			perror_msg("Stream error");
			return(NULL);
		}
	}
	return(cpio_entry);
}