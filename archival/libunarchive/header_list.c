/* vi: set sw=4 ts=4: */
#include <stdio.h>
#include "unarchive.h"

void header_list(const file_header_t *file_header)
{
	puts(file_header->name);
}
