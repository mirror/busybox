/*
 * Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * The "grep" command, taken from sash.
 * This provides basic file searching.
 *
 * Permission to distribute this code under the GPL has been granted.
 * Modified for busybox by Erik Andersen <andersee@debian.org> <andersen@lineo.com>
 */

#include "internal.h"
#ifdef BB_GREP

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>


const char grep_usage[] =
"Search the input file(s) for lines matching the given pattern.\n"
"\tI search stdin if no files are given.\n"
"\tI can't grok full regular expressions.\n"
"usage: grep [in] PATTERN [FILES]...\n"
"\ti=ignore case, n=list line numbers\n";



static	BOOL	search
	(const char * string, const char * word, BOOL ignoreCase);


extern int 
grep_main(struct FileInfo * unused, int argc, char ** argv)
{
	FILE *		fp;
	const char *	word;
	const char *	name;
	const char *	cp;
	BOOL		tellName;
	BOOL		ignoreCase;
	BOOL		tellLine;
	long		line;
	char		buf[BUF_SIZE];

	ignoreCase = FALSE;
	tellLine = FALSE;

	argc--;
	argv++;
	if (argc < 1)
	{
		fprintf(stderr, "%s", grep_usage);
		return 1;
	}

	if (**argv == '-')
	{
		argc--;
		cp = *argv++;

		while (*++cp) switch (*cp)
		{
			case 'i':
				ignoreCase = TRUE;
				break;

			case 'n':
				tellLine = TRUE;
				break;

			default:
				fprintf(stderr, "Unknown option\n");
				return 1;
		}
	}

	word = *argv++;
	argc--;

	tellName = (argc > 1);

	while (argc-- > 0)
	{
		name = *argv++;

		fp = fopen(name, "r");

		if (fp == NULL)
		{
			perror(name);

			continue;
		}

		line = 0;

		while (fgets(buf, sizeof(buf), fp))
		{
			line++;

			cp = &buf[strlen(buf) - 1];

			if (*cp != '\n')
				fprintf(stderr, "%s: Line too long\n", name);

			if (search(buf, word, ignoreCase))
			{
				if (tellName)
					printf("%s: ", name);

				if (tellLine)
					printf("%ld: ", line);

				fputs(buf, stdout);
			}
		}

		if (ferror(fp))
			perror(name);

		fclose(fp);
	}
	return 0;
}


/*
 * See if the specified word is found in the specified string.
 */
static BOOL
search(const char * string, const char * word, BOOL ignoreCase)
{
	const char *	cp1;
	const char *	cp2;
	int		len;
	int		lowFirst;
	int		ch1;
	int		ch2;

	len = strlen(word);

	if (!ignoreCase)
	{
		while (TRUE)
		{
			string = strchr(string, word[0]);

			if (string == NULL)
				return FALSE;

			if (memcmp(string, word, len) == 0)
				return TRUE;

			string++;
		}
	}

	/*
	 * Here if we need to check case independence.
	 * Do the search by lower casing both strings.
	 */
	lowFirst = *word;

	if (isupper(lowFirst))
		lowFirst = tolower(lowFirst);

	while (TRUE)
	{
		while (*string && (*string != lowFirst) &&
			(!isupper(*string) || (tolower(*string) != lowFirst)))
		{
			string++;
		}

		if (*string == '\0')
			return FALSE;

		cp1 = string;
		cp2 = word;

		do
		{
			if (*cp2 == '\0')
				return TRUE;

			ch1 = *cp1++;

			if (isupper(ch1))
				ch1 = tolower(ch1);

			ch2 = *cp2++;

			if (isupper(ch2))
				ch2 = tolower(ch2);

		}
		while (ch1 == ch2);

		string++;
	}
}

#endif
/* END CODE */
