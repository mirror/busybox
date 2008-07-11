/* vi: set sw=4 ts=4: */
/* Copyright 2002 Laurence Anderson
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

typedef struct hardlinks_t {
	struct hardlinks_t *next;
	int inode; /* TODO: must match maj/min too! */
	int mode ;
	int mtime; /* These three are useful only in corner case */
	int uid  ; /* of hardlinks with zero size body */
	int gid  ;
	char name[1];
} hardlinks_t;

char get_header_cpio(archive_handle_t *archive_handle)
{
	static hardlinks_t *saved_hardlinks = NULL;
	static hardlinks_t *saved_hardlinks_created = NULL;

	file_header_t *file_header = archive_handle->file_header;
	char cpio_header[110];
	char dummy[16];
	int namesize;
	int major, minor, nlink, mode, inode;
	unsigned size, uid, gid, mtime;

	/* There can be padding before archive header */
	data_align(archive_handle, 4);

	if (archive_xread_all_eof(archive_handle, (unsigned char*)cpio_header, 110) == 0) {
		goto create_hardlinks;
	}
	archive_handle->offset += 110;

	if (strncmp(&cpio_header[0], "07070", 5) != 0
	 || (cpio_header[5] != '1' && cpio_header[5] != '2')
	) {
		bb_error_msg_and_die("unsupported cpio format, use newc or crc");
	}

	sscanf(cpio_header + 6,
			"%8x" "%8x" "%8x" "%8x"
			"%8x" "%8x" "%8x" /*maj,min:*/ "%16c"
			/*rmaj,rmin:*/"%8x" "%8x" "%8x" /*chksum:*/ "%8c",
			&inode, &mode, &uid, &gid,
			&nlink, &mtime, &size, dummy,
			&major, &minor, &namesize, dummy);
	file_header->mode = mode;
	file_header->uid = uid;
	file_header->gid = gid;
	file_header->mtime = mtime;
	file_header->size = size;

	file_header->name = xzalloc(namesize + 1);
	/* Read in filename */
	xread(archive_handle->src_fd, file_header->name, namesize);
	archive_handle->offset += namesize;

	/* Update offset amount and skip padding before file contents */
	data_align(archive_handle, 4);

	if (strcmp(file_header->name, "TRAILER!!!") == 0) {
		/* Always round up. ">> 9" divides by 512 */
		printf("%"OFF_FMT"u blocks\n", (archive_handle->offset + 511) >> 9);
		goto create_hardlinks;
	}

	if (S_ISLNK(file_header->mode)) {
		file_header->link_target = xzalloc(file_header->size + 1);
		xread(archive_handle->src_fd, file_header->link_target, file_header->size);
		archive_handle->offset += file_header->size;
		file_header->size = 0; /* Stop possible seeks in future */
	} else {
		file_header->link_target = NULL;
	}

// TODO: data_extract_all can't deal with hardlinks to non-files...
// (should be !S_ISDIR instead of S_ISREG here)

	if (nlink > 1 && S_ISREG(file_header->mode)) {
		hardlinks_t *new = xmalloc(sizeof(*new) + namesize);
		new->inode = inode;
		new->mode  = mode ;
		new->mtime = mtime;
		new->uid   = uid  ;
		new->gid   = gid  ;
		strcpy(new->name, file_header->name);
		/* Put file on a linked list for later */
		if (size == 0) {
			new->next = saved_hardlinks;
			saved_hardlinks = new;
			return EXIT_SUCCESS; /* Skip this one */
			/* TODO: this breaks cpio -t (it does not show hardlinks) */
		}
		new->next = saved_hardlinks_created;
		saved_hardlinks_created = new;
	}
	file_header->device = makedev(major, minor);

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_data(archive_handle);
		archive_handle->action_header(file_header);
	} else {
		data_skip(archive_handle);
	}

	archive_handle->offset += file_header->size;

	free(file_header->link_target);
	free(file_header->name);
	file_header->link_target = NULL;
	file_header->name = NULL;

	return EXIT_SUCCESS;

 create_hardlinks:
	free(file_header->link_target);
	free(file_header->name);

	while (saved_hardlinks) {
		hardlinks_t *cur;
		hardlinks_t *make_me = saved_hardlinks;
		saved_hardlinks = make_me->next;

		memset(file_header, 0, sizeof(*file_header));
		file_header->name = make_me->name;
		file_header->mode = make_me->mode;
		/*file_header->size = 0;*/

		/* Try to find a file we are hardlinked to */
		cur = saved_hardlinks_created;
		while (cur) {
			/* TODO: must match maj/min too! */
			if (cur->inode == make_me->inode) {
				file_header->link_target = cur->name;
				 /* link_target != NULL, size = 0: "I am a hardlink" */
				if (archive_handle->filter(archive_handle) == EXIT_SUCCESS)
					archive_handle->action_data(archive_handle);
				free(make_me);
				goto next_link;
			}
			cur = cur->next;
		}
		/* Oops... no file with such inode was created... do it now
		 * (happens when hardlinked files are empty (zero length)) */
		file_header->mtime = make_me->mtime;
		file_header->uid   = make_me->uid  ;
		file_header->gid   = make_me->gid  ;
		if (archive_handle->filter(archive_handle) == EXIT_SUCCESS)
			archive_handle->action_data(archive_handle);
		/* Move to the list of created hardlinked files */
		make_me->next = saved_hardlinks_created;
		saved_hardlinks_created = make_me;
 next_link: ;
	}

	while (saved_hardlinks_created) {
		hardlinks_t *p = saved_hardlinks_created;
		saved_hardlinks_created = p->next;
		free(p);
	}

	return EXIT_FAILURE; /* "No more files to process" */
}
