/*
 *  Copyright (C) 2003 Glenn L. McGrath
 *  Copyright (C) 2003 Erik Andersen
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busybox.h"


#define FLAG_SILENT	1
#define FLAG_CHECK	2
#define FLAG_WARN	4

/* This might be usefull elsewhere */
static unsigned char *hash_bin_to_hex(unsigned char *hash_value,
									  unsigned char hash_length)
{
	int x, len, max;
	unsigned char *hex_value;

	max = (hash_length * 2) + 2;
	hex_value = xmalloc(max);
	for (x = len = 0; x < hash_length; x++) {
		len += snprintf(hex_value + len, max - len, "%02x", hash_value[x]);
	}
	return (hex_value);
}

static uint8_t *hash_file(const char *filename, uint8_t hash_algo)
{
	uint8_t *hash_value_bin;
	uint8_t *hash_value = NULL;
	uint8_t hash_length;
	int src_fd;

	if (strcmp(filename, "-") == 0) {
		src_fd = fileno(stdin);
	} else {
		src_fd = open(filename, O_RDONLY);
	}

	if (hash_algo == HASH_MD5) {
		hash_length = 16;
	} else {
		hash_length = 20;
	}

	hash_value_bin = xmalloc(hash_length);

	if ((src_fd != -1) && (hash_fd(src_fd, -1, hash_algo, hash_value_bin) != -2)) {
		hash_value = hash_bin_to_hex(hash_value_bin, hash_length);
	} else {
		bb_perror_msg("%s", filename);
	}

	close(src_fd);

	return(hash_value);
}

/* This could become a common function for md5 as well, by using md5_stream */
extern int hash_files(int argc, char **argv, const uint8_t hash_algo)
{
	int return_value = EXIT_SUCCESS;
	uint8_t *hash_value;

#ifdef CONFIG_FEATURE_MD5_SHA1_SUM_CHECK
	unsigned int flags;

	flags = bb_getopt_ulflags(argc, argv, "scw");
#endif

#ifdef CONFIG_FEATURE_MD5_SHA1_SUM_CHECK
	if (!(flags & FLAG_CHECK)) {
		if (flags & FLAG_SILENT) {
			bb_error_msg_and_die
				("the -s option is meaningful only when verifying checksums");
		} else if (flags & FLAG_WARN) {
			bb_error_msg_and_die
				("the -w option is meaningful only when verifying checksums");
		}
	}
#endif

	if (argc == optind) {
		argv[argc++] = "-";
	}
#ifdef CONFIG_FEATURE_MD5_SHA1_SUM_CHECK
	if (flags & FLAG_CHECK) {
		FILE *pre_computed_stream;
		int count_total = 0;
		int count_failed = 0;
		unsigned char *file_ptr = argv[optind];
		char *line;

		if (optind + 1 != argc) {
			bb_error_msg_and_die
				("only one argument may be specified when using -c");
		}

		if (strcmp(file_ptr, "-") == 0) {
			pre_computed_stream = stdin;
		} else {
			pre_computed_stream = bb_xfopen(file_ptr, "r");
		}

		while ((line = bb_get_chomped_line_from_file(pre_computed_stream)) != NULL) {
			char *filename_ptr;

			count_total++;
			filename_ptr = strstr(line, "  ");
			if (filename_ptr == NULL) {
				if (flags & FLAG_WARN) {
					bb_error_msg("Invalid format");
				}
				free(line);
				continue;
			}
			*filename_ptr = '\0';
			filename_ptr += 2;

			hash_value = hash_file(filename_ptr, hash_algo);

			if (hash_value && (strcmp(hash_value, line) == 0)) {
				if (!(flags & FLAG_SILENT))
					printf("%s: OK\n", filename_ptr);
			} else {
				if (!(flags & FLAG_SILENT))
					printf("%s: FAILED\n", filename_ptr);
				count_failed++;
			}
			/* possible free(NULL) */
			free(hash_value);
			free(line);
		}
		if (count_failed && !(flags & FLAG_SILENT)) {
			bb_error_msg("WARNING: %d of %d computed checksums did NOT match",
						 count_failed, count_total);
		}
		if (bb_fclose_nonstdin(pre_computed_stream) == EOF) {
			bb_perror_msg_and_die("Couldnt close file %s", file_ptr);
		}
	} else
#endif
	{
		uint8_t hash_length;

		if (hash_algo == HASH_MD5) {
			hash_length = 16;
		} else {
			hash_length = 20;
		}
		hash_value = xmalloc(hash_length);

		while (optind < argc) {
			unsigned char *file_ptr = argv[optind++];

			hash_value = hash_file(file_ptr, hash_algo);
			if (hash_value == NULL) {
				return_value++;
			} else {
				printf("%s  %s\n", hash_value, file_ptr);
				free(hash_value);
			}
		}
	}
	return (return_value);
}

#ifdef CONFIG_MD5SUM
extern int md5sum_main(int argc, char **argv)
{
	return(hash_files(argc, argv, HASH_MD5));
}
#endif

#ifdef CONFIG_SHA1SUM
extern int sha1sum_main(int argc, char **argv)
{
	return(hash_files(argc, argv, HASH_SHA1));
}
#endif
