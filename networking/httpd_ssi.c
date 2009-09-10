/*
 * Copyright (c) 2009 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/*
 * This program is a CGI application. It processes server-side includes:
 * <!--#include file="file.html" -->
 *
 * Usage: put these lines in httpd.conf:
 *
 * *.html:/bin/httpd_ssi
 * *.htm:/bin/httpd_ssi
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
httpd_ssi.c -o httpd_ssi
*/

/* Size (i386, static uclibc, approximate):
   text    data     bss     dec     hex filename
   8931     164   68552   77647   12f4f httpd_ssi
*/

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

static char line[64 * 1024];

/*
 * Currently only handles directives which are alone on the line
 */
static void process_includes(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp)
		exit(1);

#define INCLUDE "<!--#include file=\""
	while (fgets(line, sizeof(line), fp)) {
		char *closing_dq;

		/* FIXME: output text leading to INCLUDE first */
		if (strncmp(line, INCLUDE, sizeof(INCLUDE)-1) != 0
		 || (closing_dq = strchr(line + sizeof(INCLUDE)-1, '"')) == NULL
		/* or strstr(line + sizeof(INCLUDE)-1, "\" -->")? */
		) {
			fputs(line, stdout);
			continue;
		}
		*closing_dq = '\0';
		/* FIXME:
		 * (1) are relative paths with /../ etc ok?
		 * (2) if we include a/1.htm and it includes b/2.htm,
		 * do we need to include a/b/2.htm or b/2.htm?
		 * IOW, do we need to "cd $dirname"?
		 */
		process_includes(line + sizeof(INCLUDE)-1);
		/* FIXME: this should be the tail of line after --> */
		putchar('\n');
	}
}

int main(int argc, char *argv[])
{
	if (!argv[1])
		return 1;

	/* Seen from busybox.net's Apache:
	 * HTTP/1.1 200 OK
	 * Date: Thu, 10 Sep 2009 18:23:28 GMT
	 * Server: Apache
	 * Accept-Ranges: bytes
	 * Connection: close
	 * Content-Type: text/html
	 */
	printf(
		/* "Date: Thu, 10 Sep 2009 18:23:28 GMT\r\n" */
		/* "Server: Apache\r\n" */
		/* "Accept-Ranges: bytes\r\n" - do we really accept bytes?! */
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
	);
	process_includes(argv[1]);
	return 0;
}
