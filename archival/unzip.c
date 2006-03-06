/* vi: set sw=4 ts=4: */
/*
 * Mini unzip implementation for busybox
 *
 * Copyright (C) 2004 by Ed Clark
 *
 * Loosely based on original busybox unzip applet by Laurence Anderson.
 * All options and features should work in this version.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/* For reference see
 * http://www.pkware.com/company/standards/appnote/
 * http://www.info-zip.org/pub/infozip/doc/appnote-iz-latest.zip
 */

/* TODO
 * Endian issues
 * Zip64 + other methods
 * Improve handling of zip format, ie.
 * - deferred CRC, comp. & uncomp. lengths (zip header flags bit 3)
 * - unix file permissions, etc.
 * - central directory
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "unarchive.h"
#include "busybox.h"

#if BB_BIG_ENDIAN
static inline unsigned short
__swap16(unsigned short x) {
	return (((uint16_t)(x) & 0xFF) << 8) | (((uint16_t)(x) & 0xFF00) >> 8);
}

static inline uint32_t
__swap32(uint32_t x) {
	 return (((x & 0xFF) << 24) |
		((x & 0xFF00) << 8) |
		((x & 0xFF0000) >> 8) |
		((x & 0xFF000000) >> 24));
}
#else /* it's little-endian */
# define __swap16(x) (x)
# define __swap32(x) (x)
#endif /* BB_BIG_ENDIAN */

#define ZIP_FILEHEADER_MAGIC		__swap32(0x04034b50)
#define ZIP_CDS_MAGIC			__swap32(0x02014b50)
#define ZIP_CDS_END_MAGIC		__swap32(0x06054b50)
#define ZIP_DD_MAGIC			__swap32(0x08074b50)

extern unsigned int gunzip_crc;
extern unsigned int gunzip_bytes_out;

typedef union {
	unsigned char raw[26];
	struct {
		unsigned short version;	/* 0-1 */
		unsigned short flags;	/* 2-3 */
		unsigned short method;	/* 4-5 */
		unsigned short modtime;	/* 6-7 */
		unsigned short moddate;	/* 8-9 */
		unsigned int crc32 ATTRIBUTE_PACKED;	/* 10-13 */
		unsigned int cmpsize ATTRIBUTE_PACKED;	/* 14-17 */
		unsigned int ucmpsize ATTRIBUTE_PACKED;	/* 18-21 */
		unsigned short filename_len;	/* 22-23 */
		unsigned short extra_len;		/* 24-25 */
	} formated ATTRIBUTE_PACKED;
} zip_header_t;

static void unzip_skip(int fd, off_t skip)
{
	if (lseek(fd, skip, SEEK_CUR) == (off_t)-1) {
		if ((errno != ESPIPE) || (bb_copyfd_size(fd, -1, skip) != skip)) {
			bb_error_msg_and_die("Seek failure");
		}
	}
}

static void unzip_read(int fd, void *buf, size_t count)
{
	if (bb_xread(fd, buf, count) != count) {
		bb_error_msg_and_die("Read failure");
	}
}

static void unzip_create_leading_dirs(char *fn)
{
	/* Create all leading directories */
	char *name = bb_xstrdup(fn);
	if (bb_make_directory(dirname(name), 0777, FILEUTILS_RECUR)) {
		bb_error_msg_and_die("Failed to create directory");
	}
	free(name);
}

static void unzip_extract(zip_header_t *zip_header, int src_fd, int dst_fd)
{
	if (zip_header->formated.method == 0) {
		/* Method 0 - stored (not compressed) */
		int size = zip_header->formated.ucmpsize;
		if (size && (bb_copyfd_size(src_fd, dst_fd, size) != size)) {
			bb_error_msg_and_die("Cannot complete extraction");
		}

	} else {
		/* Method 8 - inflate */
		inflate_init(zip_header->formated.cmpsize);
		inflate_unzip(src_fd, dst_fd);
		inflate_cleanup();
		/* Validate decompression - crc */
		if (zip_header->formated.crc32 != (gunzip_crc ^ 0xffffffffL)) {
			bb_error_msg("Invalid compressed data--crc error");
		}
		/* Validate decompression - size */
		if (zip_header->formated.ucmpsize != gunzip_bytes_out) {
			bb_error_msg("Invalid compressed data--length error");
		}
	}
}

int unzip_main(int argc, char **argv)
{
	zip_header_t zip_header;
	enum {v_silent, v_normal, v_list} verbosity = v_normal;
	enum {o_prompt, o_never, o_always} overwrite = o_prompt;
	unsigned int total_size = 0;
	unsigned int total_entries = 0;
	int src_fd = -1, dst_fd = -1;
	char *src_fn = NULL, *dst_fn = NULL;
	llist_t *zaccept = NULL;
	llist_t *zreject = NULL;
	char *base_dir = NULL;
	int i, opt, opt_range = 0, list_header_done = 0;
	char key_buf[512];
	struct stat stat_buf;

	while((opt = getopt(argc, argv, "-d:lnopqx")) != -1) {
		switch(opt_range) {
		case 0: /* Options */
			switch(opt) {
			case 'l': /* List */
				verbosity = v_list;
				break;

			case 'n': /* Never overwrite existing files */
				overwrite = o_never;
				break;

			case 'o': /* Always overwrite existing files */
				overwrite = o_always;
				break;

			case 'p': /* Extract files to stdout and fall through to set verbosity */
				dst_fd = STDOUT_FILENO;

			case 'q': /* Be quiet */
				verbosity = (verbosity == v_normal) ? v_silent : verbosity;
				break;

			case 1 : /* The zip file */
				src_fn = bb_xstrndup(optarg, strlen(optarg)+4);
				opt_range++;
				break;

			default:
				bb_show_usage();

			}
			break;

		case 1: /* Include files */
			if (opt == 1) {
				zaccept = llist_add_to(zaccept, optarg);

			} else if (opt == 'd') {
				base_dir = optarg;
				opt_range += 2;

			} else if (opt == 'x') {
				opt_range++;

			} else {
				bb_show_usage();
			}
			break;

		case 2 : /* Exclude files */
			if (opt == 1) {
				zreject = llist_add_to(zreject, optarg);

			} else if (opt == 'd') { /* Extract to base directory */
				base_dir = optarg;
				opt_range++;

			} else {
				bb_show_usage();
			}
			break;

		default:
			bb_show_usage();
		}
	}

	if (src_fn == NULL) {
		bb_show_usage();
	}

	/* Open input file */
	if (strcmp("-", src_fn) == 0) {
		src_fd = STDIN_FILENO;
		/* Cannot use prompt mode since zip data is arriving on STDIN */
		overwrite = (overwrite == o_prompt) ? o_never : overwrite;

	} else {
		static const char *const extn[] = {"", ".zip", ".ZIP"};
		int orig_src_fn_len = strlen(src_fn);
		for(i = 0; (i < 3) && (src_fd == -1); i++) {
			strcpy(src_fn + orig_src_fn_len, extn[i]);
			src_fd = open(src_fn, O_RDONLY);
		}
		if (src_fd == -1) {
			src_fn[orig_src_fn_len] = 0;
			bb_error_msg_and_die("Cannot open %s, %s.zip, %s.ZIP", src_fn, src_fn, src_fn);
		}
	}

	/* Change dir if necessary */
	if (base_dir && chdir(base_dir)) {
		bb_perror_msg_and_die("Cannot chdir");
	}

	if (verbosity != v_silent)
		printf("Archive:  %s\n", src_fn);

	while (1) {
		unsigned int magic;

		/* Check magic number */
		unzip_read(src_fd, &magic, 4);
		if (magic == ZIP_CDS_MAGIC) {
			break;
		} else if (magic != ZIP_FILEHEADER_MAGIC) {
			bb_error_msg_and_die("Invalid zip magic %08X", magic);
		}

		/* Read the file header */
		unzip_read(src_fd, zip_header.raw, 26);
#if BB_BIG_ENDIAN
		zip_header.formated.version = __swap16(zip_header.formated.version);
		zip_header.formated.flags = __swap16(zip_header.formated.flags);
		zip_header.formated.method = __swap16(zip_header.formated.method);
		zip_header.formated.modtime = __swap16(zip_header.formated.modtime);
		zip_header.formated.moddate = __swap16(zip_header.formated.moddate);
		zip_header.formated.crc32 = __swap32(zip_header.formated.crc32);
		zip_header.formated.cmpsize = __swap32(zip_header.formated.cmpsize);
		zip_header.formated.ucmpsize = __swap32(zip_header.formated.ucmpsize);
		zip_header.formated.filename_len = __swap16(zip_header.formated.filename_len);
		zip_header.formated.extra_len = __swap16(zip_header.formated.extra_len);
#endif /* BB_BIG_ENDIAN */
		if ((zip_header.formated.method != 0) && (zip_header.formated.method != 8)) {
			bb_error_msg_and_die("Unsupported compression method %d", zip_header.formated.method);
		}

		/* Read filename */
		free(dst_fn);
		dst_fn = xmalloc(zip_header.formated.filename_len + 1);
		unzip_read(src_fd, dst_fn, zip_header.formated.filename_len);
		dst_fn[zip_header.formated.filename_len] = 0;

		/* Skip extra header bytes */
		unzip_skip(src_fd, zip_header.formated.extra_len);

		if ((verbosity == v_list) && !list_header_done){
			printf("  Length     Date   Time    Name\n");
			printf(" --------    ----   ----    ----\n");
			list_header_done = 1;
		}

		/* Filter zip entries */
		if (find_list_entry(zreject, dst_fn) ||
			(zaccept && !find_list_entry(zaccept, dst_fn))) { /* Skip entry */
			i = 'n';

		} else { /* Extract entry */
			total_size += zip_header.formated.ucmpsize;

			if (verbosity == v_list) { /* List entry */
				unsigned int dostime = zip_header.formated.modtime | (zip_header.formated.moddate << 16);
				printf("%9u  %02u-%02u-%02u %02u:%02u   %s\n",
					   zip_header.formated.ucmpsize,
					   (dostime & 0x01e00000) >> 21,
					   (dostime & 0x001f0000) >> 16,
					   (((dostime & 0xfe000000) >> 25) + 1980) % 100,
					   (dostime & 0x0000f800) >> 11,
					   (dostime & 0x000007e0) >> 5,
					   dst_fn);
				total_entries++;
				i = 'n';

			} else if (dst_fd == STDOUT_FILENO) { /* Extracting to STDOUT */
				i = -1;

			} else if (last_char_is(dst_fn, '/')) { /* Extract directory */
				if (stat(dst_fn, &stat_buf) == -1) {
					if (errno != ENOENT) {
						bb_perror_msg_and_die("Cannot stat '%s'",dst_fn);
					}
					if (verbosity == v_normal) {
						printf("   creating: %s\n", dst_fn);
					}
					unzip_create_leading_dirs(dst_fn);
					if (bb_make_directory(dst_fn, 0777, 0)) {
						bb_error_msg_and_die("Failed to create directory");
					}
				} else {
					if (!S_ISDIR(stat_buf.st_mode)) {
						bb_error_msg_and_die("'%s' exists but is not directory", dst_fn);
					}
				}
				i = 'n';

			} else {  /* Extract file */
			_check_file:
				if (stat(dst_fn, &stat_buf) == -1) { /* File does not exist */
					if (errno != ENOENT) {
						bb_perror_msg_and_die("Cannot stat '%s'",dst_fn);
					}
					i = 'y';

				} else { /* File already exists */
					if (overwrite == o_never) {
						i = 'n';

					} else if (S_ISREG(stat_buf.st_mode)) { /* File is regular file */
						if (overwrite == o_always) {
							i = 'y';
						} else {
							printf("replace %s? [y]es, [n]o, [A]ll, [N]one, [r]ename: ", dst_fn);
							if (!fgets(key_buf, 512, stdin)) {
								bb_perror_msg_and_die("Cannot read input");
							}
							i = key_buf[0];
						}

					} else { /* File is not regular file */
						bb_error_msg_and_die("'%s' exists but is not regular file",dst_fn);
					}
				}
			}
		}

		switch (i) {
		case 'A':
			overwrite = o_always;
		case 'y': /* Open file and fall into unzip */
			unzip_create_leading_dirs(dst_fn);
			dst_fd = bb_xopen(dst_fn, O_WRONLY | O_CREAT);
		case -1: /* Unzip */
			if (verbosity == v_normal) {
				printf("  inflating: %s\n", dst_fn);
			}
			unzip_extract(&zip_header, src_fd, dst_fd);
			if (dst_fd != STDOUT_FILENO) {
				/* closing STDOUT is potentially bad for future business */
				close(dst_fd);
			}
			break;

		case 'N':
			overwrite = o_never;
		case 'n':
			/* Skip entry data */
			unzip_skip(src_fd, zip_header.formated.cmpsize);
			break;

		case 'r':
			/* Prompt for new name */
			printf("new name: ");
			if (!fgets(key_buf, 512, stdin)) {
				bb_perror_msg_and_die("Cannot read input");
			}
			free(dst_fn);
			dst_fn = bb_xstrdup(key_buf);
			chomp(dst_fn);
			goto _check_file;

		default:
			printf("error: invalid response [%c]\n",(char)i);
			goto _check_file;
		}

		/* Data descriptor section */
		if (zip_header.formated.flags & 4) {
			/* skip over duplicate crc, compressed size and uncompressed size */
			unzip_skip(src_fd, 12);
		}
	}

	if (verbosity == v_list) {
		printf(" --------                   -------\n"
		       "%9d                   %d files\n", total_size, total_entries);
	}

	return(EXIT_SUCCESS);
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
