#include <stdio.h>
#include <string.h>
#include <time.h>
#include "libbb.h"
#include "unarchive.h"

extern void header_verbose_list(const file_header_t *file_header)
{
	struct tm *mtime = localtime(&file_header->mtime);

	printf("%s %d/%d%10u %4u-%02u-%02u %02u:%02u:%02u %s",
		bb_mode_string(file_header->mode),
		file_header->uid,
		file_header->gid,
		(unsigned int) file_header->size,
		1900 + mtime->tm_year,
		1 + mtime->tm_mon,
		mtime->tm_mday,
		mtime->tm_hour,
		mtime->tm_min,
		mtime->tm_sec,
		file_header->name);

	if (file_header->link_name) {
		printf(" -> %s", file_header->link_name);
	}
	/* putchar isnt used anywhere else i dont think */
	puts("");
}
