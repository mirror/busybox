#include <stdio.h>
#include <stdlib.h>
#include "libbb.h"

extern int seek_ared_file(FILE *in_file, ar_headers_t *headers, const char *tar_gz_file)
{
	/* find the headers for the specified .tar.gz file */
	while (headers->next != NULL) {
		if (strcmp(headers->name, tar_gz_file) == 0) {
			fseek(in_file, headers->offset, SEEK_SET);
			return(EXIT_SUCCESS);
		}
		headers = headers->next;
	}

	return(EXIT_FAILURE);
}