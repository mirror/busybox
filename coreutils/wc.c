/* vi: set sw=4 ts=4: */
/*
 * Mini wc implementation for busybox
 *
 * by Edward Betts <edward@debian.org>
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

#include "internal.h"
#include <stdio.h>

static const char wc_usage[] = "wc [OPTION]... [FILE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nPrint line, word, and byte counts for each FILE, and a total line if\n"
	"more than one FILE is specified.  With no FILE, read standard input.\n\n"
	"Options:\n"
	"\t-c\tprint the byte counts\n"
	"\t-l\tprint the newline counts\n"

	"\t-L\tprint the length of the longest line\n"
	"\t-w\tprint the word counts\n"
#endif
	;

static int total_lines, total_words, total_chars, max_length;
static int print_lines, print_words, print_chars, print_length;

void print_counts(int lines, int words, int chars, int length,
				  const char *name)
{
	char const *space = "";

	if (print_lines) {
		printf("%7d", lines);
		space = " ";
	}
	if (print_words) {
		printf("%s%7d", space, words);
		space = " ";
	}
	if (print_chars) {
		printf("%s%7d", space, chars);
		space = " ";
	}
	if (print_length)
		printf("%s%7d", space, length);
	if (*name)
		printf(" %s", name);
	putchar('\n');
}

static void wc_file(FILE * file, const char *name)
{
	int lines, words, chars, length;
	int in_word = 0, linepos = 0;
	int c;

	lines = words = chars = length = 0;
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
	if (linepos > length)
		length = linepos;
	if (in_word)
		words++;
	print_counts(lines, words, chars, length, name);
	total_lines += lines;
	total_words += words;
	total_chars += chars;
	if (length > max_length)
		max_length = length;
	fclose(file);
	fflush(stdout);
}

int wc_main(int argc, char **argv)
{
	FILE *file;

	total_lines = total_words = total_chars = max_length = 0;
	print_lines = print_words = print_chars = print_length = 0;

	while (--argc && **(++argv) == '-') {
		while (*++(*argv))
			switch (**argv) {
			case 'c':
				print_chars = 1;
				break;
			case 'l':
				print_lines = 1;
				break;
			case 'L':
				print_length = 1;
				break;
			case 'w':
				print_words = 1;
				break;
			default:
				usage(wc_usage);
			}
	}

	if (!print_lines && !print_words && !print_chars && !print_length)
		print_lines = print_words = print_chars = 1;

	if (argc == 0) {
		wc_file(stdin, "");
		exit(TRUE);
	} else if (argc == 1) {
		file = fopen(*argv, "r");
		if (file == NULL) {
			perror(*argv);
			exit(FALSE);
		}
		wc_file(file, *argv);
	} else {
		while (argc-- > 0) {
			file = fopen(*argv, "r");
			if (file == NULL) {
				perror(*argv);
				exit(FALSE);
			}
			wc_file(file, *argv);
			argv++;
		}
		print_counts(total_lines, total_words, total_chars,
					 max_length, "total");
	}
	return(TRUE);
}
