/* vi: set sw=4 ts=4: */
/*
 * Derived from
 * mapscrn.c - version 0.92
 *
 * Was taken from console-tools and adapted by 
 * Peter Novodvorsky <petya@logic.ru>
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/kd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "busybox.h"

typedef unsigned short unicode;

static long int ctoi(unsigned char *s, int *is_unicode);
static int old_screen_map_read_ascii(FILE * fp, unsigned char buf[]);
static int uni_screen_map_read_ascii(FILE * fp, unicode buf[], int *is_unicode);
static unicode utf8_to_ucs2(char *buf);
static int screen_map_load(int fd, FILE * fp);

int loadacm_main(int argc, char **argv)
{
	int fd;

	if (argc>=2 && *argv[1]=='-') {
		show_usage();
	}

	fd = open(CURRENT_VC, O_RDWR);
	if (fd < 0) {
		perror_msg_and_die("Error opening " CURRENT_VC);
	}

	if (screen_map_load(fd, stdin)) {
		perror_msg_and_die("Error loading acm");
	}

	write(fd, "\033(K", 3);

	return EXIT_SUCCESS;
}

static int screen_map_load(int fd, FILE * fp)
{
	struct stat stbuf;
	unicode wbuf[E_TABSZ];
	unsigned char buf[E_TABSZ];
	int parse_failed = 0;
	int is_unicode;

	if (fstat(fileno(fp), &stbuf))
		perror_msg_and_die("Cannot stat map file");

	/* first try a UTF screen-map: either ASCII (no restriction) or binary (regular file) */
	if (!
		(parse_failed =
		 (-1 == uni_screen_map_read_ascii(fp, wbuf, &is_unicode)))
|| (S_ISREG(stbuf.st_mode) && (stbuf.st_size == (sizeof(unicode) * E_TABSZ)))) {	/* test for binary UTF map by size */
		if (parse_failed) {
			if (-1 == fseek(fp, 0, SEEK_SET)) {
				if (errno == ESPIPE)
					error_msg_and_die("16bit screen-map MUST be a regular file.");
				else
					perror_msg_and_die("fseek failed reading binary 16bit screen-map");
			}

			if (fread(wbuf, sizeof(unicode) * E_TABSZ, 1, fp) != 1)
				perror_msg_and_die("Cannot read [new] map from file");
#if 0
			else
				error_msg("Input screen-map is binary.");
#endif
		}

		/* if it was effectively a 16-bit ASCII, OK, else try to read as 8-bit map */
		/* same if it was binary, ie. if parse_failed */
		if (parse_failed || is_unicode) {
			if (ioctl(fd, PIO_UNISCRNMAP, wbuf))
				perror_msg_and_die("PIO_UNISCRNMAP ioctl");
			else
				return 0;
		}
	}

	/* rewind... */
	if (-1 == fseek(fp, 0, SEEK_SET)) {
		if (errno == ESPIPE)
			error_msg("Assuming 8bit screen-map - MUST be a regular file."),
				exit(1);
		else
			perror_msg_and_die("fseek failed assuming 8bit screen-map");
	}

	/* ... and try an old 8-bit screen-map */
	if (!(parse_failed = (-1 == old_screen_map_read_ascii(fp, buf))) ||
		(S_ISREG(stbuf.st_mode) && (stbuf.st_size == E_TABSZ))) {	/* test for binary old 8-bit map by size */
		if (parse_failed) {
			if (-1 == fseek(fp, 0, SEEK_SET)) {
				if (errno == ESPIPE)
					/* should not - it succedeed above */
					error_msg_and_die("fseek() returned ESPIPE !");
				else
					perror_msg_and_die("fseek for binary 8bit screen-map");
			}

			if (fread(buf, E_TABSZ, 1, fp) != 1)
				perror_msg_and_die("Cannot read [old] map from file");
#if 0
			else
				error_msg("Input screen-map is binary.");
#endif
		}

		if (ioctl(fd, PIO_SCRNMAP, buf))
			perror_msg_and_die("PIO_SCRNMAP ioctl");
		else
			return 0;
	}
	error_msg("Error parsing symbolic map");
	return(1);
}


/*
 * - reads `fp' as a 16-bit ASCII SFM file.
 * - returns -1 on error.
 * - returns it in `unicode' in an E_TABSZ-elements array.
 * - sets `*is_unicode' flagiff there were any non-8-bit
 *   (ie. real 16-bit) mapping.
 *
 * FIXME: ignores everything after second word
 */
static int uni_screen_map_read_ascii(FILE * fp, unicode buf[], int *is_unicode)
{
	char buffer[256];			/* line buffer reading file */
	char *p, *q;				/* 1st + 2nd words in line */
	int in, on;					/* the same, as numbers */
	int tmp_is_unicode;			/* tmp for is_unicode calculation */
	int i;						/* loop index - result holder */
	int ret_code = 0;			/* return code */
	sigset_t acmsigset, old_sigset;

	assert(is_unicode);

	*is_unicode = 0;

	/* first 128 codes defaults to ASCII */
	for (i = 0; i < 128; i++)
		buf[i] = i;
	/* remaining defaults to replacement char (usually E_TABSZ = 256) */
	for (; i < E_TABSZ; i++)
		buf[i] = 0xfffd;

	/* block SIGCHLD */
	sigemptyset(&acmsigset);
	sigaddset(&acmsigset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &acmsigset, &old_sigset);

	do {
		if (NULL == fgets(buffer, sizeof(buffer), fp)) {
			if (feof(fp))
				break;
			else
				perror_msg_and_die("uni_screen_map_read_ascii() can't read line");
		}

		/* get "charset-relative charcode", stripping leading spaces */
		p = strtok(buffer, " \t\n");

		/* skip empty lines and comments */
		if (!p || *p == '#')
			continue;

		/* get unicode mapping */
		q = strtok(NULL, " \t\n");
		if (q) {
			in = ctoi(p, NULL);
			if (in < 0 || in > 255) {
				ret_code = -1;
				break;
			}

			on = ctoi(q, &tmp_is_unicode);
			if (in < 0 && on > 65535) {
				ret_code = -1;
				break;
			}

			*is_unicode |= tmp_is_unicode;
			buf[in] = on;
		} else {
			ret_code = -1;
			break;
		}
	}
	while (1);					/* terminated by break on feof() */

	/* restore sig mask */
	sigprocmask(SIG_SETMASK, &old_sigset, NULL);

	return ret_code;
}


static int old_screen_map_read_ascii(FILE * fp, unsigned char buf[])
{
	char buffer[256];
	int in, on;
	char *p, *q;

	for (in = 0; in < 256; in++)
		buf[in] = in;

	while (fgets(buffer, sizeof(buffer) - 1, fp)) {
		p = strtok(buffer, " \t\n");

		if (!p || *p == '#')
			continue;

		q = strtok(NULL, " \t\n#");
		if (q) {
			in = ctoi(p, NULL);
			if (in < 0 || in > 255)
				return -1;

			on = ctoi(q, NULL);
			if (in < 0 && on > 255)
				return -1;

			buf[in] = on;
		} else
			return -1;
	}

	return (0);
}


/*
 * - converts a string into an int.
 * - supports dec and hex bytes, hex UCS2, single-quoted byte and UTF8 chars.
 * - returns the converted value
 * - if `is_unicode != NULL', use it to tell whether it was unicode
 *
 * CAVEAT: will report valid UTF mappings using only 1 byte as 8-bit ones.
 */
static long int ctoi(unsigned char *s, int *is_unicode)
{
	int i;
	size_t ls;

	ls = strlen(s);
	if (is_unicode)
		*is_unicode = 0;

	/* hex-specified UCS2 */
	if ((strncmp(s, "U+", 2) == 0) &&
		(strspn(s + 2, "0123456789abcdefABCDEF") == ls - 2)) {
		sscanf(s + 2, "%x", &i);
		if (is_unicode)
			*is_unicode = 1;
	}

	/* hex-specified byte */
	else if ((ls <= 4) && (strncmp(s, "0x", 2) == 0) &&
			 (strspn(s + 2, "0123456789abcdefABCDEF") == ls - 2))
		sscanf(s + 2, "%x", &i);

	/* oct-specified number (byte) */
	else if ((*s == '0') && (strspn(s, "01234567") == ls))
		sscanf(s, "%o", &i);

	/* dec-specified number (byte) */
	else if (strspn(s, "0123456789") == ls)
		sscanf(s, "%d", &i);

	/* single-byte quoted char */
	else if ((strlen(s) == 3) && (s[0] == '\'') && (s[2] == '\''))
		i = s[1];

	/* multi-byte UTF8 quoted char */
	else if ((s[0] == '\'') && (s[ls - 1] == '\'')) {
		s[ls - 1] = 0;			/* ensure we'll not "parse UTF too far" */
		i = utf8_to_ucs2(s + 1);
		if (is_unicode)
			*is_unicode = 1;
	} else
		return (-1);

	return (i);
}


static unicode utf8_to_ucs2(char *buf)
{
	int utf_count = 0;
	long utf_char = 0;
	unicode tc = 0;
	unsigned char c;

	do {
		c = *buf;
		buf++;

		/* if byte should be part of multi-byte sequence */
		if (c & 0x80) {
			/* if we have already started to parse a UTF8 sequence */
			if (utf_count > 0 && (c & 0xc0) == 0x80) {
				utf_char = (utf_char << 6) | (c & 0x3f);
				utf_count--;
				if (utf_count == 0)
					tc = utf_char;
				else
					continue;
			} else {			/* Possibly 1st char of a UTF8 sequence */

				if ((c & 0xe0) == 0xc0) {
					utf_count = 1;
					utf_char = (c & 0x1f);
				} else if ((c & 0xf0) == 0xe0) {
					utf_count = 2;
					utf_char = (c & 0x0f);
				} else if ((c & 0xf8) == 0xf0) {
					utf_count = 3;
					utf_char = (c & 0x07);
				} else if ((c & 0xfc) == 0xf8) {
					utf_count = 4;
					utf_char = (c & 0x03);
				} else if ((c & 0xfe) == 0xfc) {
					utf_count = 5;
					utf_char = (c & 0x01);
				} else
					utf_count = 0;
				continue;
			}
		} else {				/* not part of multi-byte sequence - treat as ASCII
								   * this makes incomplete sequences to be ignored
								 */
			tc = c;
			utf_count = 0;
		}
	}
	while (utf_count);

	return tc;
}
