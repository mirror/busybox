/* vi: set sw=4 ts=4: */
/*
 * Mini cpio implementation for busybox
 *
 * Copyright (C) 2001 by Glenn McGrath 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Limitations:
 *		Doesn't check CRC's
 *		Only supports new ASCII and CRC formats
 *
 */
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unarchive.h"
#include "busybox.h"

typedef struct hardlinks_s {
	file_header_t *entry;
	int inode;
	struct hardlinks_s *next;
} hardlinks_t;

extern int cpio_main(int argc, char **argv)
{
	archive_handle_t *archive_handle;
	int opt;

	/* Initialise */
	archive_handle = init_handle();
	archive_handle->src_fd = fileno(stdin);
	archive_handle->action_header = header_list;

	while ((opt = getopt(argc, argv, "idmuvtF:")) != -1) {
		switch (opt) {
		case 'i': /* extract */
			archive_handle->action_data = data_extract_all;
			break;
		case 'd': /* create _leading_ directories */
			archive_handle->flags |= ARCHIVE_CREATE_LEADING_DIRS;
			break;
		case 'm': /* preserve modification time */
			archive_handle->flags |= ARCHIVE_PRESERVE_DATE;
			break;
		case 'v': /* verbosly list files */
			archive_handle->action_header = header_verbose_list;
			break;
		case 'u': /* unconditional */
			archive_handle->flags |= ARCHIVE_EXTRACT_UNCONDITIONAL;
			break;
		case 't': /* list files */
			archive_handle->action_header = header_list;
			break;
		case 'F':
			archive_handle->src_fd = xopen(optarg, O_RDONLY);
			break;
		default:
			show_usage();
		}
	}

	while (optind < argc) {
		archive_handle->filter = filter_accept_list;
		archive_handle->accept = add_to_list(archive_handle->accept, argv[optind]);
		optind++;
	}

	while (1) {
	 	static hardlinks_t *saved_hardlinks = NULL;
 		static unsigned short pending_hardlinks = 0;
		file_header_t *file_header = archive_handle->file_header;
		char cpio_header[110];
		int namesize;
 		char dummy[16];
 		int major, minor, nlink, inode;
		char extract_flag;

		if (pending_hardlinks) { /* Deal with any pending hardlinks */
			hardlinks_t *tmp;
			hardlinks_t *oldtmp;

			tmp = saved_hardlinks;
			oldtmp = NULL;

			while (tmp) {
				error_msg_and_die("need to fix this\n");
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
		archive_handle->offset += data_align(archive_handle->src_fd, archive_handle->offset, 4);

		if (xread_all_eof(archive_handle->src_fd, cpio_header, 110) == 0) {
			return(EXIT_FAILURE);
		}
		archive_handle->offset += 110;

		if (strncmp(&cpio_header[0], "07070", 5) != 0) {
			printf("cpio header is %x-%x-%x-%x-%x\n",
				cpio_header[0],
				cpio_header[1],
				cpio_header[2],
				cpio_header[3],
				cpio_header[4]);
			error_msg_and_die("Unsupported cpio format");
		}
		
		if ((cpio_header[5] != '1') && (cpio_header[5] != '2')) {
			error_msg_and_die("Unsupported cpio format, use newc or crc");
	}

		sscanf(cpio_header, "%6c%8x%8x%8x%8x%8x%8lx%8lx%16c%8x%8x%8x%8c",
			dummy, &inode, (unsigned int*)&file_header->mode, 
			(unsigned int*)&file_header->uid, (unsigned int*)&file_header->gid,
			&nlink, &file_header->mtime, &file_header->size,
			dummy, &major, &minor, &namesize, dummy);

		file_header->name = (char *) xmalloc(namesize + 1);
		xread(archive_handle->src_fd, file_header->name, namesize); /* Read in filename */
		file_header->name[namesize] = '\0';
		archive_handle->offset += namesize;

		/* Update offset amount and skip padding before file contents */
		archive_handle->offset += data_align(archive_handle->src_fd, archive_handle->offset, 4);

		if (strcmp(file_header->name, "TRAILER!!!") == 0) {
			printf("%d blocks\n", (int) (archive_handle->offset % 512 ? (archive_handle->offset / 512) + 1 : archive_handle->offset / 512)); /* Always round up */
			if (saved_hardlinks) { /* Bummer - we still have unresolved hardlinks */
				hardlinks_t *tmp = saved_hardlinks;
				hardlinks_t *oldtmp = NULL;
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
			return(EXIT_FAILURE);
		}

		if (S_ISLNK(file_header->mode)) {
			file_header->link_name = (char *) xmalloc(file_header->size + 1);
			xread(archive_handle->src_fd, file_header->link_name, file_header->size);
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
				continue;
			} else { /* Found the file with data in */
				hardlinks_t *tmp = saved_hardlinks;
				pending_hardlinks = 1;
				while (tmp) {
					if (tmp->inode == inode) {
						tmp->entry->link_name = xstrdup(file_header->name);
						nlink--;
					}
					tmp = tmp->next;
				}
				if (nlink > 1) {
					error_msg("error resolving hardlink: did you create the archive with GNU cpio 2.0-2.2?");
				}
			}
		}
		file_header->device = (major << 8) | minor;

		extract_flag = FALSE;
		if (archive_handle->filter(archive_handle->accept, archive_handle->reject, file_header->name) == EXIT_SUCCESS) {
			struct stat statbuf;

			extract_flag = TRUE;

			/* Check if the file already exists */
			if (lstat (file_header->name, &statbuf) == 0) {
				if ((archive_handle->flags & ARCHIVE_EXTRACT_UNCONDITIONAL) || (statbuf.st_mtime < file_header->mtime)) {
					/* Remove file if flag set or its older than the file to be extracted */
					if (unlink(file_header->name) == -1) {
						perror_msg_and_die("Couldnt remove old file");
					}
				} else {
					if (! archive_handle->flags & ARCHIVE_EXTRACT_QUIET) {
						error_msg("%s not created: newer or same age file exists", file_header->name);
					}
					extract_flag = FALSE;
				}
			}
			archive_handle->action_header(file_header);
		}

		archive_handle->action_header(file_header);
		if (extract_flag) {
			archive_handle->action_data(archive_handle);
		} else {
			data_skip(archive_handle);
		}
		archive_handle->offset += file_header->size;
	}

	return(EXIT_SUCCESS);
}

