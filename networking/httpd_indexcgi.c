/*
 * Copyright (c) 2007 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/*
 * This program is a CGI application. It creates directory index page.
 * Put it into cgi-bin/index.cgi and chmod 0755.
 */

/* Build a-la
i486-linux-uclibc-gcc \
-static -static-libgcc \
-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
-Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Werror \
-Wold-style-definition -Wdeclaration-after-statement -Wno-pointer-sign \
-Wmissing-prototypes -Wmissing-declarations \
-Os -fno-builtin-strlen -finline-limit=0 -fomit-frame-pointer \
-ffunction-sections -fdata-sections -fno-guess-branch-probability \
-funsigned-char \
-falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 \
-march=i386 -mpreferred-stack-boundary=2 \
-Wl,-Map -Wl,link.map -Wl,--warn-common -Wl,--sort-common -Wl,--gc-sections \
httpd_indexcgi.c -o index.cgi

Size (approximate):
 text    data     bss     dec     hex filename
22642     160    3052   25854    64fe index.cgi
*/

/* TODO: get rid of printf's: printf code is more than 50%
 * of the entire executable when built against static uclibc */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>

/* Appearance of the table is controlled by style sheet *ONLY*,
 * formatting code uses <TAG class=CLASS> to apply style
 * to elements. Edit stylesheet to your liking and recompile. */

static const char str_header[] =
"" /* Additional headers (currently none) */
"\r\n" /* Mandatory empty line after headers */
"<html><head><title>Index of %s</title>"                "\n"
"<style>"                                               "\n"
"table {"                                               "\n"
"  width: 100%%;"                                       "\n"
"  background-color: #fff5ee;"                          "\n"
"  border-width: 1px;" /* 1px 1px 1px 1px; */           "\n"
"  border-spacing: 2px;"                                "\n"
"  border-style: solid;" /* solid solid solid solid; */ "\n"
"  border-color: black;" /* black black black black; */ "\n"
"  border-collapse: collapse;"                          "\n"
"}"                                                     "\n"
"th {"                                                  "\n"
"  border-width: 1px;" /* 1px 1px 1px 1px; */           "\n"
"  padding: 1px;" /* 1px 1px 1px 1px; */                "\n"
"  border-style: solid;" /* solid solid solid solid; */ "\n"
"  border-color: black;" /* black black black black; */ "\n"
"}"                                                     "\n"
"td {"                                                  "\n"
            /* top right bottom left */
"  border-width: 0px 1px 0px 1px;"                      "\n"
"  padding: 1px;" /* 1px 1px 1px 1px; */                "\n"
"  border-style: solid;" /* solid solid solid solid; */ "\n"
"  border-color: black;" /* black black black black; */ "\n"
"}"                                                     "\n"
"tr.hdr { background-color:#eee5de; }"                  "\n"
"tr.o { background-color:#ffffff; }"                    "\n"
/* tr.e { ... } - for even rows (currently none) */
"tr.foot { background-color:#eee5de; }"                 "\n"
"th.cnt { text-align:left; }"                           "\n"
"th.sz { text-align:right; }"                           "\n"
"th.dt { text-align:right; }"                           "\n"
"td.sz { text-align:right; }"                           "\n"
"td.dt { text-align:right; }"                           "\n"
"col.nm { width: 98%%; }"                               "\n"
"col.sz { width: 1%%; }"                                "\n"
"col.dt { width: 1%%; }"                                "\n"
"</style>"                                              "\n"
"</head>"                                               "\n"
"<body>"                                                "\n"
"<h1>Index of %s</h1>"                                  "\n"
""                                                      "\n"
"<table>"                                               "\n"
"<col class=nm><col class=sz><col class=dt>"            "\n"
"<tr class=hdr><th class=cnt>Name<th class=sz>Size<th class=dt>Last modified" "\n"
;

static const char str_footer[] =
"<tr class=foot><th class=cnt>Files: %u, directories: %u<th class=sz>%llu<th class=dt>&nbsp;" "\n"
/* "</table></body></html>" - why bother? */
;

static int bad_url_char(unsigned c)
{
	return (c - '0') > 9 /* not a digit */
	    && ((c|0x20) - 'a') > 26 /* not A-Z or a-z */
	    && !strchr("._-+@", c);
}

static char *url_encode(const char *name)
{
	int i;
	int size = 0;
	int len = strlen(name);
	char *p, *result;

	i = 0;
	while (name[i]) {
		if (bad_url_char((unsigned)name[i]))
			size++;
		i++;
	}

	/* No %xx needed! */
	if (!size)
		return (char*)name;

	/* Each %xx requires 2 additional chars */
	size = size * 2 + len + 1;
	p = result = malloc(size);

	i = 0;
	while (name[i]) {
		*p = name[i];
		if (bad_url_char((unsigned)name[i])) {
			*p++ = '%';
			*p++ = "0123456789ABCDEF"[(uint8_t)(name[i]) >> 4];
			*p = "0123456789ABCDEF"[(uint8_t)(name[i]) & 0xf];
		}
		p++;
		i++;
	}
	*p = 0;
	return result;
}

static char *html_encode(const char *name)
{
	int i;
	int size = 0;
	int len = strlen(name);
	char *p, *result;

	i = 0;
	while (name[i]) {
		if (name[i] == '<'
		 || name[i] == '>'
		 || name[i] == '&'
		) {
			size++;
		}
		i++;
	}

	/* No &lt; etc needed! */
	if (!size)
		return (char*)name;

	/* &amp; requires 4 additional chars */
	size = size * 4 + len + 1;
	p = result = malloc(size);

	i = 0;
	while (name[i]) {
		char c;
		*p = c = name[i++];
		if (c == '<')
			strcpy(p, "&lt;");
		else if (c == '>')
			strcpy(p, "&gt;");
		else if (c == '&')
			strcpy(++p, "amp;");
		else {
			p++;
			continue;
		}
		p += 4;
	}
	*p = 0;
	return result;
}

typedef struct dir_list_t {
	char  *dl_name;
	mode_t dl_mode;
	off_t  dl_size;
	time_t dl_mtime;
} dir_list_t;

static int compare_dl(dir_list_t *a, dir_list_t *b)
{
	if (strcmp(a->dl_name, "..") == 0) {
		/* ".." is 'less than' any other dir entry */
		return -1;
	}
	if (strcmp(b->dl_name, "..") == 0) {
		return 1;
	}
	if (S_ISDIR(a->dl_mode) != S_ISDIR(b->dl_mode)) {
		/* 1 if b is a dir (and thus a is 'after' b, a > b),
		 * else -1 (a < b)*/
		return (S_ISDIR(b->dl_mode) != 0) ? 1 : -1;
	}
	return strcmp(a->dl_name, b->dl_name);
}

int main(void)
{
	dir_list_t *dir_list;
	dir_list_t *cdir;
	unsigned dir_list_count;
	unsigned count_dirs;
	unsigned count_files;
	unsigned long long size_total;
	int odd;
	DIR *dirp;
	char *QUERY_STRING;

	QUERY_STRING = getenv("QUERY_STRING");
	if (!QUERY_STRING
	 || QUERY_STRING[0] != '/'
	 || strstr(QUERY_STRING, "/../")
	 || strcmp(strrchr(QUERY_STRING, '/'), "/..") == 0
	) {
		return 1;
	}

	if (chdir("..")
	 || (QUERY_STRING[1] && chdir(QUERY_STRING + 1))
	) {
		return 1;
	}

	dirp = opendir(".");
	if (!dirp)
		return 1;

	dir_list = NULL;
	dir_list_count = 0;
	while (1) {
		struct dirent *dp;
		struct stat sb;

		dp = readdir(dirp);
		if (!dp)
			break;
		if (dp->d_name[0] == '.' && !dp->d_name[1])
			continue;
		if (stat(dp->d_name, &sb) != 0)
			continue;
		dir_list = realloc(dir_list, (dir_list_count + 1) * sizeof(dir_list[0]));
		dir_list[dir_list_count].dl_name = strdup(dp->d_name);
		dir_list[dir_list_count].dl_mode = sb.st_mode;
		dir_list[dir_list_count].dl_size = sb.st_size;
		dir_list[dir_list_count].dl_mtime = sb.st_mtime;
		dir_list_count++;
	}

	qsort(dir_list, dir_list_count, sizeof(dir_list[0]), (void*)compare_dl);

	/* Guard against directories wit &, > etc */
	QUERY_STRING = html_encode(QUERY_STRING);
	printf(str_header, QUERY_STRING, QUERY_STRING);

	odd = 0;
	count_dirs = 0;
	count_files = 0;
	size_total = 0;

	cdir = dir_list;
	while (dir_list_count--) {
		char size_str[sizeof(long long) * 3];
		const char *slash_if_dir;
		struct tm *tm;
		char *href;
		char *filename;
		char datetime_str[sizeof("2000-02-02&nbsp;02:02:02")];

		slash_if_dir = "/";
		if (S_ISDIR(cdir->dl_mode)) {
			count_dirs++;
			size_str[0] = '\0';
		} else if (S_ISREG(cdir->dl_mode)) {
			count_files++;
			size_total += cdir->dl_size;
			slash_if_dir++; /* points to "" now */
			sprintf(size_str, "%llu", (unsigned long long)(cdir->dl_size));
		} else
			goto next;
		href = url_encode(cdir->dl_name); /* %20 etc */
		filename = html_encode(cdir->dl_name); /* &lt; etc */
		tm = gmtime(&cdir->dl_mtime);
		sprintf(datetime_str, "%04u-%02u-%02u&nbsp;%02u:%02u:%02u",
			(unsigned)(1900 + tm->tm_year),
			(unsigned)(tm->tm_mon + 1),
			(unsigned)(tm->tm_mday),
			(unsigned)(tm->tm_hour),
			(unsigned)(tm->tm_min),
			(unsigned)(tm->tm_sec)
		);
		printf("<tr class=%c><td class=nm><a href='%s%s'>%s%s</a><td class=sz>%s<td class=dt>%s\n",
			odd ? 'o' : 'e',
			href, slash_if_dir,
			filename, slash_if_dir,
			size_str,
			datetime_str
		);
		if (cdir->dl_name != href)
			free(href);
		if (cdir->dl_name != filename)
			free(filename);
		odd = 1 - odd;
 next:
		cdir++;
	}

	/* count_dirs - 1: we don't want to count ".." */
	printf(str_footer, count_files, count_dirs - 1, size_total);
	return 0;
}
