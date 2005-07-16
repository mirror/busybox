/* vi: set sw=4 ts=4: */
/*
 * Mini wc implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>
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
 */

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include "busybox.h"

enum print_e {
	print_lines = 1,
	print_words = 2,
	print_chars = 4,
	print_length = 8
};

static unsigned int total_lines = 0;
static unsigned int total_words = 0;
static unsigned int total_chars = 0;
static unsigned int max_length = 0;
static char print_type = 0;

static void print_counts(const unsigned int lines, const unsigned int words,
	const unsigned int chars, const unsigned int length, const char *name)
{
	if (print_type & print_lines) {
		printf("%7d ", lines);
	}
	if (print_type & print_words) {
		printf("%7d ", words);
	}
	if (print_type & print_chars) {
		printf("%7d ", chars);
	}
	if (print_type & print_length) {
		printf("%7d ", length);
	}
	if (*name) {
		printf("%s", name);
	}
	putchar('\n');
}

static void wc_file(FILE * file, const char *name)
{
	unsigned int lines = 0;
	unsigned int words = 0;
	unsigned int chars = 0;
	unsigned int length = 0;
	unsigned int linepos = 0;
	char in_word = 0;
	int c;

	while ((c = getc(file)) != EOF) {
		chars++;
		switch (c) {
		case '\n':
			lines++;
		case '\r':
		case '\f':
			if (linepos > length)
				length = linepos;
			linepos = 0;
			goto word_separator;
		case '\t':
			linepos += 8 - (linepos % 8);
			goto word_separator;
		case ' ':
			linepos++;
		case '\v':
word_separator:
			if (in_word) {
				in_word = 0;
				words++;
			}
			break;
		default:
			linepos++;
			in_word = 1;
			break;
		}
	}
	if (linepos > length) {
		length = linepos;
	}
	if (in_word) {
		words++;
	}
	print_counts(lines, words, chars, length, name);
	total_lines += lines;
	total_words += words;
	total_chars += chars;
	if (length > max_length) {
		max_length = length;
	}
}

int wc_main(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "clLw")) > 0) {
		switch (opt) {
			case 'c':
				print_type |= print_chars;
				break;
			case 'l':
				print_type |= print_lines;
				break;
			case 'L':
				print_type |= print_length;
				break;
			case 'w':
				print_type |= print_words;
				break;
			default:
				show_usage();
		}
	}

	if (print_type == 0) {
		print_type = print_lines | print_words | print_chars;
	}

	if (argv[optind] == NULL || strcmp(argv[optind], "-") == 0) {
		wc_file(stdin, "");
	} else {
		unsigned short num_files_counted = 0;
		while (optind < argc) {
			if (print_type == print_chars) {
				struct stat statbuf;
				stat(argv[optind], &statbuf);
				print_counts(0, 0, statbuf.st_size, 0, argv[optind]);
				total_chars += statbuf.st_size;
			} else {
				FILE *file;
				file = xfopen(argv[optind], "r");
				wc_file(file, argv[optind]);
				fclose(file);
			}
			optind++;
			num_files_counted++;
		}
		if (num_files_counted > 1) {
			print_counts(total_lines, total_words, total_chars, max_length, "total");
		}
	}

	return(EXIT_SUCCESS);
}
