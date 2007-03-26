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
	} while (1);
	return 0;
}

#define SPLIT_OPT_l (1<<0)
#define SPLIT_OPT_b (1<<1)
#define SPLIT_OPT_a (1<<2)

int split_main(int argc, char **argv);
int split_main(int argc, char **argv)
{
	char *pfx, *buf, *input_file;
	unsigned cnt = 1000, opt;
	bool ret = EXIT_SUCCESS;
	FILE *fp;
	char *count_p, *sfx;
//XXX: FIXME	opt_complementary = "+2"; /* at most 2 non-option arguments */
	opt = getopt32(argc, argv, "l:b:a:", &count_p, &count_p, &sfx);

	if (opt & SPLIT_OPT_l)
		cnt = xatoi(count_p);
	if (opt & SPLIT_OPT_b)
		cnt = xatoul_sfx(count_p, split_suffices);
	if (opt & SPLIT_OPT_a)
		suffix_len = xatoi(sfx);
	argv += optind;
	if (!*argv)
		*--argv = (char*) "-";
	input_file = *argv;
	sfx = *++argv;

	if (sfx && (NAME_MAX < strlen(sfx) + suffix_len))
			bb_error_msg_and_die("Suffix too long");

	{
		char *char_p = xzalloc(suffix_len);
		memset(char_p, 'a', suffix_len);
		pfx = xasprintf("%s%s", sfx ? sfx : "x", char_p);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(char_p);
	}
	fp = fopen_or_warn_stdin(input_file);
//XXX:FIXME: unify those two file-handling schemata below (FILE vs fd) !
	if (opt & SPLIT_OPT_b) {
		ssize_t i;
		ssize_t bytes = 0;
		int inp = fileno(fp);

		do {
			int out = xopen(pfx, O_WRONLY | O_CREAT | O_TRUNC);
			lseek(inp, bytes, SEEK_SET);
			buf = xzalloc(cnt);
			bytes += i = full_read(inp, buf, cnt);
			xwrite(out, buf, i);
			free(buf);
			close(out);
			if (next_file(&pfx)) {
				ret++;
				goto bail;
			}
		} while (i == cnt); /* if we read less than cnt, then nothing is left */
	} else { /* -l */
		do {
			unsigned i = cnt;
			int out = xopen(pfx, O_WRONLY | O_CREAT | O_TRUNC);
			buf = NULL;
			while (i--) {
			    buf = xmalloc_fgets(fp);
				if (buf == NULL)
					break;
				xwrite(out, buf, strlen(buf));
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
