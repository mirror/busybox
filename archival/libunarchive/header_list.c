#include <stdio.h>
#include "unarchive.h"

extern void header_list(const file_header_t *file_header)
{
	puts(file_header->name);
}
