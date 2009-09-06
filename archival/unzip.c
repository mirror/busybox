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
 * Zip64 + other methods
 */

#include "libbb.h"
#include "unarchive.h"

enum {
#if BB_BIG_ENDIAN
	ZIP_FILEHEADER_MAGIC = 0x504b0304,
	ZIP_CDS_MAGIC        = 0x504b0102,
	ZIP_CDE_MAGIC        = 0x504b0506,
	ZIP_DD_MAGIC         = 0x504b0708,
#else
	ZIP_FILEHEADER_MAGIC = 0x04034b50,
	ZIP_CDS_MAGIC        = 0x02014b50,
	ZIP_CDE_MAGIC        = 0x06054b50,
	ZIP_DD_MAGIC         = 0x08074b50,
#endif
};

#define ZIP_HEADER_LEN 26

typedef union {
	uint8_t raw[ZIP_HEADER_LEN];
	struct {
		uint16_t version;               /* 0-1 */
		uint16_t flags;                 /* 2-3 */
		uint16_t method;                /* 4-5 */
		uint16_t modtime;               /* 6-7 */
		uint16_t moddate;               /* 8-9 */
		uint32_t crc32 PACKED;          /* 10-13 */
		uint32_t cmpsize PACKED;        /* 14-17 */
		uint32_t ucmpsize PACKED;       /* 18-21 */
		uint16_t filename_len;          /* 22-23 */
		uint16_t extra_len;             /* 24-25 */
	} formatted PACKED;
} zip_header_t; /* PACKED - gcc 4.2.1 doesn't like it (spews warning) */

/* Check the offset of the last element, not the length.  This leniency
 * allows for poor packing, whereby the overall struct may be too long,
 * even though the elements are all in the right place.
 */
struct BUG_zip_header_must_be_26_bytes {
	char BUG_zip_header_must_be_26_bytes[
		offsetof(zip_header_t, formatted.extra_len) + 2
			== ZIP_HEADER_LEN ? 1 : -1];
};

#define FIX_ENDIANNESS_ZIP(zip_header) do { \
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

#define CDS_HEADER_LEN 42

typedef union {
	uint8_t raw[CDS_HEADER_LEN];
	struct {
		/* uint32_t signature; 50 4b 01 02 */
		uint16_t version_made_by;       /* 0-1 */
		uint16_t version_needed;        /* 2-3 */
		uint16_t cds_flags;             /* 4-5 */
		uint16_t method;                /* 6-7 */
		uint16_t mtime;                 /* 8-9 */
		uint16_t mdate;                 /* 10-11 */
		uint32_t crc32;                 /* 12-15 */
		uint32_t cmpsize;               /* 16-19 */
		uint32_t ucmpsize;              /* 20-23 */
		uint16_t file_name_length;      /* 24-25 */
		uint16_t extra_field_length;    /* 26-27 */
		uint16_t file_comment_length;   /* 28-29 */
		uint16_t disk_number_start;     /* 30-31 */
		uint16_t internal_file_attributes; /* 32-33 */
		uint32_t external_file_attributes PACKED; /* 34-37 */
		uint32_t relative_offset_of_local_header PACKED; /* 38-41 */
	} formatted PACKED;
} cds_header_t;

struct BUG_cds_header_must_be_42_bytes {
	char BUG_cds_header_must_be_42_bytes[
		offsetof(cds_header_t, formatted.relative_offset_of_local_header) + 4
			== CDS_HEADER_LEN ? 1 : -1];
};

#define FIX_ENDIANNESS_CDS(cds_header) do { \
	(cds_header).formatted.crc32        = SWAP_LE32((cds_header).formatted.crc32       ); \
	(cds_header).formatted.cmpsize      = SWAP_LE32((cds_header).formatted.cmpsize     ); \
	(cds_header).formatted.ucmpsize     = SWAP_LE32((cds_header).formatted.ucmpsize    ); \
	(cds_header).formatted.file_name_length = SWAP_LE16((cds_header).formatted.file_name_length); \
	(cds_header).formatted.extra_field_length = SWAP_LE16((cds_header).formatted.extra_field_length); \
	(cds_header).formatted.file_comment_length = SWAP_LE16((cds_header).formatted.file_comment_length); \
} while (0)

#define CDE_HEADER_LEN 16

typedef union {
	uint8_t raw[CDE_HEADER_LEN];
	struct {
		/* uint32_t signature; 50 4b 05 06 */
		uint16_t this_disk_no;
		uint16_t disk_with_cds_no;
		uint16_t cds_entries_on_this_disk;
		uint16_t cds_entries_total;
		uint32_t cds_size;
		uint32_t cds_offset;
		/* uint16_t file_comment_length; */
		/* .ZIP file comment (variable size) */
	} formatted PACKED;
} cde_header_t;

struct BUG_cde_header_must_be_16_bytes {
	char BUG_cde_header_must_be_16_bytes[
		sizeof(cde_header_t) == CDE_HEADER_LEN ? 1 : -1];
};

#define FIX_ENDIANNESS_CDE(cde_header) do { \
	(cde_header).formatted.cds_offset = SWAP_LE32((cde_header).formatted.cds_offset); \
} while (0)

enum { zip_fd = 3 };


#if ENABLE_DESKTOP
/* NB: does not preserve file position! */
static uint32_t find_cds_offset(void)
{
	unsigned char buf[1024];
	cde_header_t cde_header;
	unsigned char *p;
	off_t end;

	end = xlseek(zip_fd, 0, SEEK_END);
	if (end < 1024)
		end = 1024;
	end -= 1024;
	xlseek(zip_fd, end, SEEK_SET);
	full_read(zip_fd, buf, 1024);

	p = buf;
	while (p <= buf + 1024 - CDE_HEADER_LEN - 4) {
		if (*p != 'P') {
			p++;
			continue;
		}
		if (*++p != 'K')
			continue;
		if (*++p != 5)
			continue;
		if (*++p != 6)
			continue;
		/* we found CDE! */
		memcpy(cde_header.raw, p + 1, CDE_HEADER_LEN);
		FIX_ENDIANNESS_CDE(cde_header);
		return cde_header.formatted.cds_offset;
	}
	bb_error_msg_and_die("can't find file table");
};

static uint32_t read_next_cds(int count_m1, uint32_t cds_offset, cds_header_t *cds_ptr)
{
	off_t org;

	org = xlseek(zip_fd, 0, SEEK_CUR);

	if (!cds_offset)
		cds_offset = find_cds_offset();

	while (count_m1-- >= 0) {
		xlseek(zip_fd, cds_offset + 4, SEEK_SET);
		xread(zip_fd, cds_ptr->raw, CDS_HEADER_LEN);
		FIX_ENDIANNESS_CDS(*cds_ptr);
		cds_offset += 4 + CDS_HEADER_LEN
			+ cds_ptr->formatted.file_name_length
			+ cds_ptr->formatted.extra_field_length
			+ cds_ptr->formatted.file_comment_length;
	}

	xlseek(zip_fd, org, SEEK_SET);
	return cds_offset;
};
#endif

static void unzip_skip(off_t skip)
{
	if (lseek(zip_fd, skip, SEEK_CUR) == (off_t)-1)
		bb_copyfd_exact_size(zip_fd, -1, skip);
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

static void unzip_extract(zip_header_t *zip_header, int dst_fd)
{
	if (zip_header->formatted.method == 0) {
		/* Method 0 - stored (not compressed) */
		off_t size = zip_header->formatted.ucmpsize;
		if (size)
			bb_copyfd_exact_size(zip_fd, dst_fd, size);
	} else {
		/* Method 8 - inflate */
		inflate_unzip_result res;
		if (inflate_unzip(&res, zip_header->formatted.cmpsize, zip_fd, dst_fd) < 0)
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
	smallint quiet = 0;
	IF_NOT_DESKTOP(const) smallint verbose = 0;
	smallint listing = 0;
	smallint overwrite = O_PROMPT;
#if ENABLE_DESKTOP
	uint32_t cds_offset;
	unsigned cds_entries;
#endif
	unsigned long total_usize;
	unsigned long total_size;
	unsigned total_entries;
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

/* -q, -l and -v: UnZip 5.52 of 28 February 2005, by Info-ZIP:
 *
 * # /usr/bin/unzip -qq -v decompress_unlzma.i.zip
 *   204372  Defl:N    35278  83%  09-06-09 14:23  0d056252  decompress_unlzma.i
 * # /usr/bin/unzip -q -v decompress_unlzma.i.zip
 *  Length   Method    Size  Ratio   Date   Time   CRC-32    Name
 * --------  ------  ------- -----   ----   ----   ------    ----
 *   204372  Defl:N    35278  83%  09-06-09 14:23  0d056252  decompress_unlzma.i
 * --------          -------  ---                            -------
 *   204372            35278  83%                            1 file
 * # /usr/bin/unzip -v decompress_unlzma.i.zip
 * Archive:  decompress_unlzma.i.zip
 *  Length   Method    Size  Ratio   Date   Time   CRC-32    Name
 * --------  ------  ------- -----   ----   ----   ------    ----
 *   204372  Defl:N    35278  83%  09-06-09 14:23  0d056252  decompress_unlzma.i
 * --------          -------  ---                            -------
 *   204372            35278  83%                            1 file
 * # unzip -v decompress_unlzma.i.zip
 * Archive:  decompress_unlzma.i.zip
 *   Length     Date   Time    Name
 *  --------    ----   ----    ----
 *    204372  09-06-09 14:23   decompress_unlzma.i
 *  --------                   -------
 *    204372                   1 files
 * # /usr/bin/unzip -l -qq decompress_unlzma.i.zip
 *    204372  09-06-09 14:23   decompress_unlzma.i
 * # /usr/bin/unzip -l -q decompress_unlzma.i.zip
 *   Length     Date   Time    Name
 *  --------    ----   ----    ----
 *    204372  09-06-09 14:23   decompress_unlzma.i
 *  --------                   -------
 *    204372                   1 file
 * # /usr/bin/unzip -l decompress_unlzma.i.zip
 * Archive:  decompress_unlzma.i.zip
 *   Length     Date   Time    Name
 *  --------    ----   ----    ----
 *    204372  09-06-09 14:23   decompress_unlzma.i
 *  --------                   -------
 *    204372                   1 file
 */

	/* '-' makes getopt return 1 for non-options */
	while ((opt = getopt(argc, argv, "-d:lnopqxv")) != -1) {
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
				quiet++;
				break;

			case 'v': /* Verbose list */
				IF_DESKTOP(verbose++;)
				listing = 1;
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
		xdup2(STDIN_FILENO, zip_fd);
		/* Cannot use prompt mode since zip data is arriving on STDIN */
		if (overwrite == O_PROMPT)
			overwrite = O_NEVER;
	} else {
		static const char extn[][5] = {"", ".zip", ".ZIP"};
		int orig_src_fn_len = strlen(src_fn);
		int src_fd = -1;

		for (i = 0; (i < 3) && (src_fd == -1); i++) {
			strcpy(src_fn + orig_src_fn_len, extn[i]);
			src_fd = open(src_fn, O_RDONLY);
		}
		if (src_fd == -1) {
			src_fn[orig_src_fn_len] = '\0';
			bb_error_msg_and_die("can't open %s, %s.zip, %s.ZIP", src_fn, src_fn, src_fn);
		}
		xmove_fd(src_fd, zip_fd);
	}

	/* Change dir if necessary */
	if (base_dir)
		xchdir(base_dir);

	if (quiet <= 1) { /* not -qq */
		if (quiet == 0)
			printf("Archive:  %s\n", src_fn);
		if (listing) {
			puts(verbose ?
				" Length   Method    Size  Ratio   Date   Time   CRC-32    Name\n"
				"--------  ------  ------- -----   ----   ----   ------    ----"
				:
				"  Length     Date   Time    Name\n"
				" --------    ----   ----    ----"
				);
		}
	}

	total_usize = 0;
	total_size = 0;
	total_entries = 0;
#if ENABLE_DESKTOP
	cds_entries = 0;
	cds_offset = 0;
#endif
	while (1) {
		uint32_t magic;

		/* Check magic number */
		xread(zip_fd, &magic, 4);
		/* Central directory? It's at the end, so exit */
		if (magic == ZIP_CDS_MAGIC)
			break;
#if ENABLE_DESKTOP
		/* Data descriptor? It was a streaming file, go on */
		if (magic == ZIP_DD_MAGIC) {
			/* skip over duplicate crc32, cmpsize and ucmpsize */
			unzip_skip(3 * 4);
			continue;
		}
#endif
		if (magic != ZIP_FILEHEADER_MAGIC)
			bb_error_msg_and_die("invalid zip magic %08X", (int)magic);

		/* Read the file header */
		xread(zip_fd, zip_header.raw, ZIP_HEADER_LEN);
		FIX_ENDIANNESS_ZIP(zip_header);
		if ((zip_header.formatted.method != 0) && (zip_header.formatted.method != 8)) {
			bb_error_msg_and_die("unsupported method %d", zip_header.formatted.method);
		}
#if !ENABLE_DESKTOP
		if (zip_header.formatted.flags & 0x0009) {
			bb_error_msg_and_die("zip flags 1 and 8 are not supported");
		}
#else
		if (zip_header.formatted.flags & 0x0001) {
			/* 0x0001 - encrypted */
			bb_error_msg_and_die("zip flag 1 (encryption) is not supported");
		}
		if (zip_header.formatted.flags & 0x0008) {
			cds_header_t cds_header;
			/* 0x0008 - streaming. [u]cmpsize can be reliably gotten
			 * only from Central Directory. See unzip_doc.txt */
			cds_offset = read_next_cds(total_entries - cds_entries, cds_offset, &cds_header);
			cds_entries = total_entries + 1;
			zip_header.formatted.crc32    = cds_header.formatted.crc32;
			zip_header.formatted.cmpsize  = cds_header.formatted.cmpsize;
			zip_header.formatted.ucmpsize = cds_header.formatted.ucmpsize;
		}
#endif

		/* Read filename */
		free(dst_fn);
		dst_fn = xzalloc(zip_header.formatted.filename_len + 1);
		xread(zip_fd, dst_fn, zip_header.formatted.filename_len);

		/* Skip extra header bytes */
		unzip_skip(zip_header.formatted.extra_len);

		/* Filter zip entries */
		if (find_list_entry(zreject, dst_fn)
		 || (zaccept && !find_list_entry(zaccept, dst_fn))
		) { /* Skip entry */
			i = 'n';

		} else { /* Extract entry */
			if (listing) { /* List entry */
				unsigned dostime = zip_header.formatted.modtime | (zip_header.formatted.moddate << 16);
				if (!verbose) {
					//      "  Length     Date   Time    Name\n"
					//      " --------    ----   ----    ----"
					printf(       "%9u  %02u-%02u-%02u %02u:%02u   %s\n",
						(unsigned)zip_header.formatted.ucmpsize,
						(dostime & 0x01e00000) >> 21,
						(dostime & 0x001f0000) >> 16,
						(((dostime & 0xfe000000) >> 25) + 1980) % 100,
						(dostime & 0x0000f800) >> 11,
						(dostime & 0x000007e0) >> 5,
						dst_fn);
					total_usize += zip_header.formatted.ucmpsize;
				} else {
					unsigned long percents = zip_header.formatted.ucmpsize - zip_header.formatted.cmpsize;
					percents = percents * 100;
					if (zip_header.formatted.ucmpsize)
						percents /= zip_header.formatted.ucmpsize;
					//      " Length   Method    Size  Ratio   Date   Time   CRC-32    Name\n"
					//      "--------  ------  ------- -----   ----   ----   ------    ----"
					printf(      "%8u  Defl:N"    "%9u%4u%%  %02u-%02u-%02u %02u:%02u  %08x  %s\n",
						(unsigned)zip_header.formatted.ucmpsize,
						(unsigned)zip_header.formatted.cmpsize,
						(unsigned)percents,
						(dostime & 0x01e00000) >> 21,
						(dostime & 0x001f0000) >> 16,
						(((dostime & 0xfe000000) >> 25) + 1980) % 100,
						(dostime & 0x0000f800) >> 11,
						(dostime & 0x000007e0) >> 5,
						zip_header.formatted.crc32,
						dst_fn);
					total_usize += zip_header.formatted.ucmpsize;
					total_size += zip_header.formatted.cmpsize;
				}
				i = 'n';
			} else if (dst_fd == STDOUT_FILENO) { /* Extracting to STDOUT */
				i = -1;
			} else if (last_char_is(dst_fn, '/')) { /* Extract directory */
				if (stat(dst_fn, &stat_buf) == -1) {
					if (errno != ENOENT) {
						bb_perror_msg_and_die("can't stat '%s'", dst_fn);
					}
					if (!quiet) {
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
 check_file:
				if (stat(dst_fn, &stat_buf) == -1) { /* File does not exist */
					if (errno != ENOENT) {
						bb_perror_msg_and_die("can't stat '%s'", dst_fn);
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
								bb_perror_msg_and_die("can't read input");
							}
							i = key_buf[0];
						}
					} else { /* File is not regular file */
						bb_error_msg_and_die("'%s' exists but is not regular file", dst_fn);
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
			if (!quiet) {
				printf("  inflating: %s\n", dst_fn);
			}
			unzip_extract(&zip_header, dst_fd);
			if (dst_fd != STDOUT_FILENO) {
				/* closing STDOUT is potentially bad for future business */
				close(dst_fd);
			}
			break;

		case 'N':
			overwrite = O_NEVER;
		case 'n':
			/* Skip entry data */
			unzip_skip(zip_header.formatted.cmpsize);
			break;

		case 'r':
			/* Prompt for new name */
			printf("new name: ");
			if (!fgets(key_buf, sizeof(key_buf), stdin)) {
				bb_perror_msg_and_die("can't read input");
			}
			free(dst_fn);
			dst_fn = xstrdup(key_buf);
			chomp(dst_fn);
			goto check_file;

		default:
			printf("error: invalid response [%c]\n", (char)i);
			goto check_file;
		}

		total_entries++;
	}

	if (listing && quiet <= 1) {
		if (!verbose) {
			//      "  Length     Date   Time    Name\n"
			//      " --------    ----   ----    ----"
			printf( " --------                   -------\n"
				"%9lu"   "                   %u files\n",
				total_usize, total_entries);
		} else {
			unsigned long percents = total_usize - total_size;
			percents = percents * 100;
			if (total_usize)
				percents /= total_usize;
			//      " Length   Method    Size  Ratio   Date   Time   CRC-32    Name\n"
			//      "--------  ------  ------- -----   ----   ----   ------    ----"
			printf( "--------          -------  ---                            -------\n"
				"%8lu"              "%17lu%4u%%                            %u files\n",
				total_usize, total_size, (unsigned)percents,
				total_entries);
		}
	}

	return 0;
}
