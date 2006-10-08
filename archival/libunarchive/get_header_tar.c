/* vi: set sw=4 ts=4: */
/* Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 *  FIXME:
 *    In privileged mode if uname and gname map to a uid and gid then use the
 *    mapped value instead of the uid/gid values in tar header
 *
 *  References:
 *    GNU tar and star man pages,
 *    Opengroup's ustar interchange format,
 *	http://www.opengroup.org/onlinepubs/007904975/utilities/pax.html
 */

#include "libbb.h"
#include "unarchive.h"

#ifdef CONFIG_FEATURE_TAR_GNU_EXTENSIONS
static char *longname = NULL;
static char *linkname = NULL;
#endif

char get_header_tar(archive_handle_t *archive_handle)
{
	file_header_t *file_header = archive_handle->file_header;
	union {
		/* ustar header, Posix 1003.1 */
		unsigned char raw[512];
		struct {
			char name[100];	/*   0-99 */
			char mode[8];	/* 100-107 */
			char uid[8];	/* 108-115 */
			char gid[8];	/* 116-123 */
			char size[12];	/* 124-135 */
			char mtime[12];	/* 136-147 */
			char chksum[8];	/* 148-155 */
			char typeflag;	/* 156-156 */
			char linkname[100];	/* 157-256 */
			char magic[6];	/* 257-262 */
			char version[2];	/* 263-264 */
			char uname[32];	/* 265-296 */
			char gname[32];	/* 297-328 */
			char devmajor[8];	/* 329-336 */
			char devminor[8];	/* 337-344 */
			char prefix[155];	/* 345-499 */
			char padding[12];	/* 500-512 */
		} formatted;
	} tar;
	long sum = 0;
	long i;
	static int end = 0;

	/* Align header */
	data_align(archive_handle, 512);

	xread(archive_handle->src_fd, tar.raw, 512);
	archive_handle->offset += 512;

	/* If there is no filename its an empty header */
	if (tar.formatted.name[0] == 0) {
		if (end) {
			/* This is the second consecutive empty header! End of archive!
			 * Read until the end to empty the pipe from gz or bz2
			 */
			while (full_read(archive_handle->src_fd, tar.raw, 512) == 512);
			return EXIT_FAILURE;
		}
		end = 1;
		return EXIT_SUCCESS;
	}
	end = 0;

	/* Check header has valid magic, "ustar" is for the proper tar
	 * 0's are for the old tar format
	 */
	if (strncmp(tar.formatted.magic, "ustar", 5) != 0) {
#ifdef CONFIG_FEATURE_TAR_OLDGNU_COMPATIBILITY
		if (strncmp(tar.formatted.magic, "\0\0\0\0\0", 5) != 0)
#endif
			bb_error_msg_and_die("invalid tar magic");
	}
	/* Do checksum on headers */
	for (i =  0; i < 148 ; i++) {
		sum += tar.raw[i];
	}
	sum += ' ' * 8;
	for (i = 156; i < 512 ; i++) {
		sum += tar.raw[i];
	}
	if (sum != xstrtoul(tar.formatted.chksum, 8)) {
		bb_error_msg_and_die("invalid tar header checksum");
	}

#ifdef CONFIG_FEATURE_TAR_GNU_EXTENSIONS
	if (longname) {
		file_header->name = longname;
		longname = NULL;
	}
	else if (linkname) {
		file_header->name = linkname;
		linkname = NULL;
	} else
#endif
	{
		file_header->name = xstrndup(tar.formatted.name, 100);
		if (tar.formatted.prefix[0]) {
			char *temp = file_header->name;
			file_header->name = concat_path_file(tar.formatted.prefix, temp);
			free(temp);
		}
	}

	file_header->uid = xstrtoul(tar.formatted.uid, 8);
	file_header->gid = xstrtoul(tar.formatted.gid, 8);
	// TODO: LFS support
	file_header->size = xstrtoul(tar.formatted.size, 8);
	file_header->mtime = xstrtoul(tar.formatted.mtime, 8);
	file_header->link_name = tar.formatted.linkname[0] ?
	                         xstrdup(tar.formatted.linkname) : NULL;
	file_header->device = makedev(xstrtoul(tar.formatted.devmajor, 8),
	                              xstrtoul(tar.formatted.devminor, 8));

	/* Set bits 0-11 of the files mode */
	file_header->mode = 07777 & xstrtoul(tar.formatted.mode, 8);

	/* Set bits 12-15 of the files mode */
	switch (tar.formatted.typeflag) {
	/* busybox identifies hard links as being regular files with 0 size and a link name */
	case '1':
		file_header->mode |= S_IFREG;
		break;
	case '7':
		/* Reserved for high performance files, treat as normal file */
	case 0:
	case '0':
#ifdef CONFIG_FEATURE_TAR_OLDGNU_COMPATIBILITY
		if (last_char_is(file_header->name, '/')) {
			file_header->mode |= S_IFDIR;
		} else
#endif
			file_header->mode |= S_IFREG;
		break;
	case '2':
		file_header->mode |= S_IFLNK;
		break;
	case '3':
		file_header->mode |= S_IFCHR;
		break;
	case '4':
		file_header->mode |= S_IFBLK;
		break;
	case '5':
		file_header->mode |= S_IFDIR;
		break;
	case '6':
		file_header->mode |= S_IFIFO;
		break;
#ifdef CONFIG_FEATURE_TAR_GNU_EXTENSIONS
	case 'L': {
			longname = xzalloc(file_header->size + 1);
			xread(archive_handle->src_fd, longname, file_header->size);
			archive_handle->offset += file_header->size;

			return get_header_tar(archive_handle);
		}
	case 'K': {
			linkname = xzalloc(file_header->size + 1);
			xread(archive_handle->src_fd, linkname, file_header->size);
			archive_handle->offset += file_header->size;

			file_header->name = linkname;
			return get_header_tar(archive_handle);
		}
	case 'D':	/* GNU dump dir */
	case 'M':	/* Continuation of multi volume archive*/
	case 'N':	/* Old GNU for names > 100 characters */
	case 'S':	/* Sparse file */
	case 'V':	/* Volume header */
#endif
	case 'g':	/* pax global header */
	case 'x':	/* pax extended header */
		bb_error_msg("ignoring extension type %c", tar.formatted.typeflag);
		break;
	default:
		bb_error_msg("unknown typeflag: 0x%x", tar.formatted.typeflag);
	}
	{	/* Strip trailing '/' in directories */
		/* Must be done after mode is set as '/' is used to check if its a directory */
		char *tmp = last_char_is(file_header->name, '/');
		if (tmp) {
			*tmp = '\0';
		}
	}

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_header(archive_handle->file_header);
		archive_handle->flags |= ARCHIVE_EXTRACT_QUIET;
		archive_handle->action_data(archive_handle);
		llist_add_to(&(archive_handle->passed), file_header->name);
	} else {
		data_skip(archive_handle);
	}
	archive_handle->offset += file_header->size;

	free(file_header->link_name);

	return EXIT_SUCCESS;
}
