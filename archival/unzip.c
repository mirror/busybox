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

#include "libbb.h"
#include "unarchive.h"

enum {
#if BB_BIG_ENDIAN
	ZIP_FILEHEADER_MAGIC = 0x504b0304,
	ZIP_CDS_MAGIC        = 0x504b0102,
	ZIP_CDS_END_MAGIC    = 0x504b0506,
	ZIP_DD_MAGIC         = 0x504b0708,
#else
	ZIP_FILEHEADER_MAGIC = 0x04034b50,
	ZIP_CDS_MAGIC        = 0x02014b50,
	ZIP_CDS_END_MAGIC    = 0x06054b50,
	ZIP_DD_MAGIC         = 0x08074b50,
#endif
};

#define ZIP_HEADER_LEN 26

typedef union {
	uint8_t raw[ZIP_HEADER_LEN];
	struct {
		uint16_t version;                       /* 0-1 */
		uint16_t flags;                         /* 2-3 */
		uint16_t method;                        /* 4-5 */
		uint16_t modtime;                       /* 6-7 */
		uint16_t moddate;                       /* 8-9 */
		uint32_t crc32 ATTRIBUTE_PACKED;        /* 10-13 */
		uint32_t cmpsize ATTRIBUTE_PACKED;      /* 14-17 */
		uint32_t ucmpsize ATTRIBUTE_PACKED;     /* 18-21 */
		uint16_t filename_len;                  /* 22-23 */
		uint16_t extra_len;                     /* 24-25 */
	} formatted ATTRIBUTE_PACKED;
} zip_header_t; /* ATTRIBUTE_PACKED - gcc 4.2.1 doesn't like it (spews warning) */

/* Check the offset of the last element, not the length.  This leniency
 * allows for poor packing, whereby the overall struct may be too long,
 * even though the elements are all in the right place.
 */
struct BUG_zip_header_must_be_26_bytes {
	char BUG_zip_header_must_be_26_bytes[
		offsetof(zip_header_t, formatted.extra_len) + 2 ==
			ZIP_HEADER_LEN ? 1 : -1];
};

#define FIX_ENDIANNESS(zip_header) do { \
	(zip_header).formatted.version      = SWAP_LE16((zip_header).formatted.version     ); \
	(zip_header).formatted.flags        = SWAP_LE16((zip_header).formatted.flags       ); \
	(zip_header).formatted.method       = SWAP_LE16((zip_header).formatted.method      ); \
	(zip_header).formatted.modtime      = SWAP_LE16((zip_header).formatted.modtime     ); \
	(zip_header).formatted.moddate      = SWAP_LE16((zip_header).formatted.moddate     ); \
	(zip_header).formatted.crc32        = SWAP_LE32((zip_header).formatted.crc32       ); \
	(zip_header).formatted.cmpsize      = SWAP_LE32((zip_header).formatted.cmpsize     ); \
	(zip_header).formatted.ucmpsize     = SWAP_LE32((zip_header).formatted.ucmpsize    ); \
	(zip_header).formatted.filename_len = SWAP_LE16((zip_header).formatted.filename_len); \
	(zip_header).formatted.extra_len    = SWAP_LE16((zip_header).formatted.extra_len   ); \
} while (0)

static void unzip_skip(int fd, off_t skip)
{
	bb_copyfd_exact_size(fd, -1, skip);
}

static void unzip_create_leading_dirs(const char *fn)
{
	/* Create all leading directories */
	char *name = xstrdup(fn);
	if (bb_make_directory(dirname(name), 0777, FILEUTILS_RECUR)) {
		bb_error_msg_and_die("exiting"); /* bb_make_directory is noisy */
	}
	free(name);
}

static void unzip_extract(zip_header_t *zip_header, int src_fd, int dst_fd)
{
	if (zip_header->formatted.method == 0) {
		/* Method 0 - stored (not compressed) */
		off_t size = zip_header->formatted.ucmpsize;
		if (size)
			bb_copyfd_exact_size(src_fd, dst_fd, size);
	} else {
		/* Method 8 - inflate */
		inflate_unzip_result res;
		if (inflate_unzip(&res, zip_header->formatted.cmpsize, src_fd, dst_fd) < 0)
			bb_error_msg_and_die("inflate error");
		/* Validate decompression - crc */
		if (zip_header->formatted.crc32 != (res.crc ^ 0xffffffffL)) {
			bb_error_msg_and_die("crc error");
		}
		/* Validate decompression - size */
		if (zip_header->formatted.ucmpsize != res.bytes_out) {
			/* Don't die. Who knows, maybe len calculation
			 * was botched somewhere. After all, crc matched! */
			bb_error_msg("bad length");
		}
	}
}

int unzip_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int unzip_main(int argc, char **argv)
{
	enum { O_PROMPT, O_NEVER, O_ALWAYS };

	zip_header_t zip_header;
	smallint verbose = 1;
	smallint listing = 0;
	smallint overwrite = O_PROMPT;
	unsigned total_size;
	unsigned total_entries;
	int src_fd = -1;
	int dst_fd = -1;
	char *src_fn = NULL;
	char *dst_fn = NULL;
	llist_t *zaccept = NULL;
	llist_t *zreject = NULL;
	char *base_dir = NULL;
	int i, opt;
	int opt_range = 0;
	char key_buf[80];
	struct stat stat_buf;

	/* '-' makes getopt return 1 for non-options */
	while ((opt = getopt(argc, argv, "-d:lnopqx")) != -1) {
		switch (opt_range) {
		case 0: /* Options */
			switch (opt) {
			case 'l': /* List */
				listing = 1;
				break;

			case 'n': /* Never overwrite existing files */
				overwrite = O_NEVER;
				break;

			case 'o': /* Always overwrite existing files */
				overwrite = O_ALWAYS;
				break;

			case 'p': /* Extract files to stdout and fall through to set verbosity */
				dst_fd = STDOUT_FILENO;

			case 'q': /* Be quiet */
				verbose = 0;
				break;

			case 1: /* The zip file */
				/* +5: space for ".zip" and NUL */
				src_fn = xmalloc(strlen(optarg) + 5);
				strcpy(src_fn, optarg);
				opt_range++;
				break;

			default:
				bb_show_usage();

			}
			break;

		case 1: /* Include files */
			if (opt == 1) {
				llist_add_to(&zaccept, optarg);
				break;
			}
			if (opt == 'd') {
				base_dir = optarg;
				opt_range += 2;
				break;
			}
			if (opt == 'x') {
				opt_range++;
				break;
			}
			bb_show_usage();

		case 2 : /* Exclude files */
			if (opt == 1) {
				llist_add_to(&zreject, optarg);
				break;
			}
			if (opt == 'd') { /* Extract to base directory */
				base_dir = optarg;
				opt_range++;
				break;
			}
			/* fall through */

		default:
			bb_show_usage();
		}
	}

	if (src_fn == NULL) {
		bb_show_usage();
	}

	/* Open input file */
	if (LONE_DASH(src_fn)) {
		src_fd = STDIN_FILENO;
		/* Cannot use prompt mode since zip data is arriving on STDIN */
		if (overwrite == O_PROMPT)
			overwrite = O_NEVER;
	} else {
		static const char extn[][5] = {"", ".zip", ".ZIP"};
		int orig_src_fn_len = strlen(src_fn);

		for (i = 0; (i < 3) && (src_fd == -1); i++) {
			strcpy(src_fn + orig_src_fn_len, extn[i]);
			src_fd = open(src_fn, O_RDONLY);
		}
		if (src_fd == -1) {
			src_fn[orig_src_fn_len] = '\0';
			bb_error_msg_and_die("can't open %s, %s.zip, %s.ZIP", src_fn, src_fn, src_fn);
		}
	}

	/* Change dir if necessary */
	if (base_dir)
		xchdir(base_dir);

	if (verbose) {
		printf("Archive:  %s\n", src_fn);
		if (listing){
			puts("  Length     Date   Time    Name\n"
			     " --------    ----   ----    ----");
		}
	}

	total_size = 0;
	total_entries = 0;
	while (1) {
		uint32_t magic;

		/* Check magic number */
		xread(src_fd, &magic, 4);
		if (magic == ZIP_CDS_MAGIC)
			break;
		if (magic != ZIP_FILEHEADER_MAGIC)
			bb_error_msg_and_die("invalid zip magic %08X", magic);

		/* Read the file header */
		xread(src_fd, zip_header.raw, ZIP_HEADER_LEN);
		FIX_ENDIANNESS(zip_header);
		if ((zip_header.formatted.method != 0) && (zip_header.formatted.method != 8)) {
			bb_error_msg_and_die("unsupported method %d", zip_header.formatted.method);
		}

		/* Read filename */
		free(dst_fn);
		dst_fn = xzalloc(zip_header.formatted.filename_len + 1);
		xread(src_fd, dst_fn, zip_header.formatted.filename_len);

		/* Skip extra header bytes */
		unzip_skip(src_fd, zip_header.formatted.extra_len);

		/* Filter zip entries */
		if (find_list_entry(zreject, dst_fn)
		 || (zaccept && !find_list_entry(zaccept, dst_fn))
		) { /* Skip entry */
			i = 'n';

		} else { /* Extract entry */
			if (listing) { /* List entry */
				if (verbose) {
					unsigned dostime = zip_header.formatted.modtime | (zip_header.formatted.moddate << 16);
					printf("%9u  %02u-%02u-%02u %02u:%02u   %s\n",
					   zip_header.formatted.ucmpsize,
					   (dostime & 0x01e00000) >> 21,
					   (dostime & 0x001f0000) >> 16,
					   (((dostime & 0xfe000000) >> 25) + 1980) % 100,
					   (dostime & 0x0000f800) >> 11,
					   (dostime & 0x000007e0) >> 5,
					   dst_fn);
					total_size += zip_header.formatted.ucmpsize;
					total_entries++;
				} else {
					/* short listing -- filenames only */
					puts(dst_fn);
				}
				i = 'n';
			} else if (dst_fd == STDOUT_FILENO) { /* Extracting to STDOUT */
				i = -1;
			} else if (last_char_is(dst_fn, '/')) { /* Extract directory */
				if (stat(dst_fn, &stat_buf) == -1) {
					if (errno != ENOENT) {
						bb_perror_msg_and_die("cannot stat '%s'",dst_fn);
					}
					if (verbose) {
						printf("   creating: %s\n", dst_fn);
					}
					unzip_create_leading_dirs(dst_fn);
					if (bb_make_directory(dst_fn, 0777, 0)) {
						bb_error_msg_and_die("exiting");
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
						bb_perror_msg_and_die("cannot stat '%s'",dst_fn);
					}
					i = 'y';
				} else { /* File already exists */
					if (overwrite == O_NEVER) {
						i = 'n';
					} else if (S_ISREG(stat_buf.st_mode)) { /* File is regular file */
						if (overwrite == O_ALWAYS) {
							i = 'y';
						} else {
							printf("replace %s? [y]es, [n]o, [A]ll, [N]one, [r]ename: ", dst_fn);
							if (!fgets(key_buf, sizeof(key_buf), stdin)) {
								bb_perror_msg_and_die("cannot read input");
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
			overwrite = O_ALWAYS;
		case 'y': /* Open file and fall into unzip */
			unzip_create_leading_dirs(dst_fn);
			dst_fd = xopen(dst_fn, O_WRONLY | O_CREAT | O_TRUNC);
		case -1: /* Unzip */
			if (verbose) {
				printf("  inflating: %s\n", dst_fn);
			}
			unzip_extract(&zip_header, src_fd, dst_fd);
			if (dst_fd != STDOUT_FILENO) {
				/* closing STDOUT is potentially bad for future business */
				close(dst_fd);
			}
			break;

		case 'N':
			overwrite = O_NEVER;
		case 'n':
			/* Skip entry data */
			unzip_skip(src_fd, zip_header.formatted.cmpsize);
			break;

		case 'r':
			/* Prompt for new name */
			printf("new name: ");
			if (!fgets(key_buf, sizeof(key_buf), stdin)) {
				bb_perror_msg_and_die("cannot read input");
			}
			free(dst_fn);
			dst_fn = xstrdup(key_buf);
			chomp(dst_fn);
			goto _check_file;

		default:
			printf("error: invalid response [%c]\n",(char)i);
			goto _check_file;
		}

		/* Data descriptor section */
		if (zip_header.formatted.flags & 4) {
			/* skip over duplicate crc, compressed size and uncompressed size */
			unzip_skip(src_fd, 12);
		}
	}

	if (listing && verbose) {
		printf(" --------                   -------\n"
		       "%9d                   %d files\n",
		       total_size, total_entries);
	}

	return 0;
}
