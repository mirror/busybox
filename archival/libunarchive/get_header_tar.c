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

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
static char *longname;
static char *linkname;
#else
enum {
	longname = 0,
	linkname = 0,
};
#endif

/* NB: _DESTROYS_ str[len] character! */
static unsigned long long getOctal(char *str, int len)
{
	unsigned long long v;
	/* Actually, tar header allows leading spaces also.
	 * Oh well, we will be liberal and skip this...
	 * The only downside probably is that we allow "-123" too :)
	if (*str < '0' || *str > '7')
		bb_error_msg_and_die("corrupted octal value in tar header");
	*/
	str[len] = '\0';
	v = strtoull(str, &str, 8);
	if (*str)
		bb_error_msg_and_die("corrupted octal value in tar header");
	return v;
}
#define GET_OCTAL(a) getOctal((a), sizeof(a))

void BUG_tar_header_size(void);
char get_header_tar(archive_handle_t *archive_handle)
{
	static int end;

	file_header_t *file_header = archive_handle->file_header;
	struct {
		/* ustar header, Posix 1003.1 */
		char name[100];     /*   0-99 */
		char mode[8];       /* 100-107 */
		char uid[8];        /* 108-115 */
		char gid[8];        /* 116-123 */
		char size[12];      /* 124-135 */
		char mtime[12];     /* 136-147 */
		char chksum[8];     /* 148-155 */
		char typeflag;      /* 156-156 */
		char linkname[100]; /* 157-256 */
		char magic[6];      /* 257-262 */
		char version[2];    /* 263-264 */
		char uname[32];     /* 265-296 */
		char gname[32];     /* 297-328 */
		char devmajor[8];   /* 329-336 */
		char devminor[8];   /* 337-344 */
		char prefix[155];   /* 345-499 */
		char padding[12];   /* 500-512 */
	} tar;
	char *cp;
	int sum, i;
	int parse_names;

	if (sizeof(tar) != 512)
		BUG_tar_header_size();

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
 again:
#endif
	/* Align header */
	data_align(archive_handle, 512);

 again_after_align:

	xread(archive_handle->src_fd, &tar, 512);
	archive_handle->offset += 512;

	/* If there is no filename its an empty header */
	if (tar.name[0] == 0) {
		if (end) {
			/* This is the second consecutive empty header! End of archive!
			 * Read until the end to empty the pipe from gz or bz2
			 */
			while (full_read(archive_handle->src_fd, &tar, 512) == 512)
				/* repeat */;
			return EXIT_FAILURE;
		}
		end = 1;
		return EXIT_SUCCESS;
	}
	end = 0;

	/* Check header has valid magic, "ustar" is for the proper tar
	 * 0's are for the old tar format
	 */
	if (strncmp(tar.magic, "ustar", 5) != 0) {
#if ENABLE_FEATURE_TAR_OLDGNU_COMPATIBILITY
		if (memcmp(tar.magic, "\0\0\0\0", 5) != 0)
#endif
			bb_error_msg_and_die("invalid tar magic");
	}
	/* Do checksum on headers */
	sum = ' ' * sizeof(tar.chksum);
	for (i = 0; i < 148 ; i++) {
		sum += ((char*)&tar)[i];
	}
	for (i = 156; i < 512 ; i++) {
		sum += ((char*)&tar)[i];
	}
	/* This field does not need special treatment (getOctal) */
	if (sum != xstrtoul(tar.chksum, 8)) {
		bb_error_msg_and_die("invalid tar header checksum");
	}

	/* 0 is reserved for high perf file, treat as normal file */
	if (!tar.typeflag) tar.typeflag = '0';
	parse_names = (tar.typeflag >= '0' && tar.typeflag <= '7');

	/* getOctal trashes subsequent field, therefore we call it
	 * on fields in reverse order */
	if (tar.devmajor[0]) {
		unsigned minor = GET_OCTAL(tar.devminor);
		unsigned major = GET_OCTAL(tar.devmajor);
		file_header->device = makedev(major, minor);
	}
	file_header->link_name = NULL;
	if (!linkname && parse_names && tar.linkname[0]) {
		/* we trash magic[0] here, it's ok */
		tar.linkname[sizeof(tar.linkname)] = '\0';
		file_header->link_name = xstrdup(tar.linkname);
		/* FIXME: what if we have non-link object with link_name? */
		/* Will link_name be free()ed? */
	}
	file_header->mtime = GET_OCTAL(tar.mtime);
	file_header->size = GET_OCTAL(tar.size);
	file_header->gid = GET_OCTAL(tar.gid);
	file_header->uid = GET_OCTAL(tar.uid);
	/* Set bits 0-11 of the files mode */
	file_header->mode = 07777 & GET_OCTAL(tar.mode);

	file_header->name = NULL;
	if (!longname && parse_names) {
		/* we trash mode[0] here, it's ok */
		tar.name[sizeof(tar.name)] = '\0';
		if (tar.prefix[0]) {
			/* and padding[0] */
			tar.prefix[sizeof(tar.prefix)] = '\0';
			file_header->name = concat_path_file(tar.prefix, tar.name);
		} else
			file_header->name = xstrdup(tar.name);
	}

	/* Set bits 12-15 of the files mode */
	/* (typeflag was not trashed because chksum does not use getOctal) */
	switch (tar.typeflag) {
	/* busybox identifies hard links as being regular files with 0 size and a link name */
	case '1':
		file_header->mode |= S_IFREG;
		break;
	case '7':
	/* case 0: */
	case '0':
#if ENABLE_FEATURE_TAR_OLDGNU_COMPATIBILITY
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
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	case 'L':
		/* free: paranoia: tar with several consecutive longnames */
		free(longname);
		/* For paranoia reasons we allocate extra NUL char */
		longname = xzalloc(file_header->size + 1);
		/* We read ASCIZ string, including NUL */
		xread(archive_handle->src_fd, longname, file_header->size);
		archive_handle->offset += file_header->size;
		/* return get_header_tar(archive_handle); */
		/* gcc 4.1.1 didn't optimize it into jump */
		/* so we will do it ourself, this also saves stack */
		goto again;
	case 'K':
		free(linkname);
		linkname = xzalloc(file_header->size + 1);
		xread(archive_handle->src_fd, linkname, file_header->size);
		archive_handle->offset += file_header->size;
		/* return get_header_tar(archive_handle); */
		goto again;
	case 'D':	/* GNU dump dir */
	case 'M':	/* Continuation of multi volume archive */
	case 'N':	/* Old GNU for names > 100 characters */
	case 'S':	/* Sparse file */
	case 'V':	/* Volume header */
#endif
	case 'g':	/* pax global header */
	case 'x': {	/* pax extended header */
		off_t sz;
		bb_error_msg("warning: skipping header '%c'", tar.typeflag);
		sz = (file_header->size + 511) & ~(off_t)511;
		archive_handle->offset += sz;
		sz >>= 9; /* sz /= 512 but w/o contortions for signed div */
		while (sz--)
			xread(archive_handle->src_fd, &tar, 512);
		/* return get_header_tar(archive_handle); */
		goto again_after_align;
	}
	default:
		bb_error_msg_and_die("unknown typeflag: 0x%x", tar.typeflag);
	}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	if (longname) {
		file_header->name = longname;
		longname = NULL;
	}
	if (linkname) {
		file_header->link_name = linkname;
		linkname = NULL;
	}
#endif
	if (!strncmp(file_header->name, "/../"+1, 3)
	 || strstr(file_header->name, "/../")
	) {
		bb_error_msg_and_die("name with '..' encountered: '%s'",
				file_header->name);
	}

	/* Strip trailing '/' in directories */
	/* Must be done after mode is set as '/' is used to check if it's a directory */
	cp = last_char_is(file_header->name, '/');

	if (archive_handle->filter(archive_handle) == EXIT_SUCCESS) {
		archive_handle->action_header(archive_handle->file_header);
		/* Note that we kill the '/' only after action_header() */
		/* (like GNU tar 1.15.1: verbose mode outputs "dir/dir/") */
		if (cp) *cp = '\0';
		archive_handle->flags |= ARCHIVE_EXTRACT_QUIET;
		archive_handle->action_data(archive_handle);
		llist_add_to(&(archive_handle->passed), file_header->name);
	} else {
		data_skip(archive_handle);
		free(file_header->name);
	}
	archive_handle->offset += file_header->size;

	free(file_header->link_name);
	/* Do not free(file_header->name)! */

	return EXIT_SUCCESS;
}
