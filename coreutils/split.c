/* vi: set sw=4 ts=4: */
/*
 * split - split a file into pieces
 * Copyright (c) 2007 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
/* BB_AUDIT: SUSv3 compliant
 * SUSv3 requirements:
 * http://www.opengroup.org/onlinepubs/009695399/utilities/split.html
 */
#include "busybox.h"
static unsigned suffix_len = 2;
static const struct suffix_mult split_suffices[] = {
#if ENABLE_FEATURE_SPLIT_FANCY
	{ "b", 512 },
#endif
	{ "k", 1024 },
	{ "m", 1024*1024 },
#if ENABLE_FEATURE_SPLIT_FANCY
	{ "g", 1024*1024*1024 },
#endif
	{ NULL, 0 }
};

/* Increment the suffix part of the filename.
 * Returns 0 on success and 1 on error (if we are out of files)
 */
static bool next_file(char **old)
{
	size_t end = strlen(*old);
	unsigned i = 1;
	char *curr;

	do {
		curr = *old + end - i;
		if (*curr < 'z') {
			*curr += 1;
			break;
		}
		i++;
		if (i > suffix_len) {
			bb_error_msg("Suffices exhausted");
			return 1;
		}
		*curr = 'a';
	} while (i <= suffix_len);
	return 0;
}

#define SPLIT_OPT_l (1<<0)
#define SPLIT_OPT_b (1<<1)
#define SPLIT_OPT_a (1<<2)

int split_main(int argc, char **argv);
int split_main(int argc, char **argv)
{
	char *pfx;
	char *count_p;
	char *sfx_len;
	unsigned cnt = 1000;
	char *input_file;
	bool ret = EXIT_SUCCESS;
	FILE *fp;

//XXX: FIXME	opt_complementary = "+2"; /* at most 2 non-option arguments */
	getopt32(argc, argv, "l:b:a:", &count_p, &count_p, &sfx_len);
	argv += optind;

	if (option_mask32 & SPLIT_OPT_l)
		cnt = xatoi(count_p);
	if (option_mask32 & SPLIT_OPT_b)
		cnt = xatoul_sfx(count_p, split_suffices);
	if (option_mask32 & SPLIT_OPT_a)
		suffix_len = xatoi(sfx_len);

	if (!*argv)
		*--argv = (char*) "-";
	input_file = *argv;

	if (NAME_MAX < strlen(*argv) + suffix_len)
		bb_error_msg_and_die("Suffix too long");

	fp = fopen_or_warn_stdin(input_file);
	{
		char *char_p = xzalloc(suffix_len);
		memset(char_p, 'a', suffix_len);
		pfx = xasprintf("%s%s", (argc > optind + 1) ? *++argv : "x", char_p);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(char_p);
	}
//XXX:FIXME: unify those two file-handling schemata below (FILE vs fd) !
	if (option_mask32 & SPLIT_OPT_b) {
		char *buf;
		ssize_t i;
		ssize_t bytes = 0;
		int flags = O_WRONLY | O_CREAT | O_TRUNC;
		int inp = fileno(fp);

		do {
			int out = xopen(pfx, flags);
			buf = xzalloc(cnt);
			lseek(inp, bytes, SEEK_SET);
			bytes += i = full_read(inp, buf, cnt);
			xwrite(out, buf, i);
			close(out);
			free(buf);
			if (next_file(&pfx)) {
				ret++;
				goto bail;
			}
		} while (i == cnt); /* if we read less than cnt, then nothing is left */
	} else { /* -l */
		char *buf;
		do {
			unsigned i = cnt;
			int flags = O_WRONLY | O_CREAT | O_TRUNC;
			int out = xopen(pfx, flags);
			buf = NULL;
			while (i--) {
			    buf = xmalloc_fgets(fp);
				if (buf == NULL)
					break;
				xwrite(out, buf, buf ? strlen(buf) : 0);
				free(buf);
			};
			close(out);

			if (next_file(&pfx)) {
				ret++;
				goto bail;
			}
		} while (buf);
	}
bail:
	if (ENABLE_FEATURE_CLEAN_UP) {
		free(pfx);
		fclose_if_not_stdin(fp);
	}
	return ret;
}
