#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libbb.h"

/*
 * Reads consecutive lines from file line that start with end_string
 * read finishes at an empty line or eof
 */
extern char *read_text_file_to_buffer(FILE *src_file)
{
	char *line = NULL;
	char *buffer = NULL;
	int buffer_length = 0;
	int line_length = 0;

	buffer = xmalloc(1);
	strcpy(buffer, "");

	/* Loop until line is empty, or just one char, which will be '\n' */
	do {
		line = get_line_from_file(src_file);
		if (line == NULL) {
			break;
		}
		line_length = strlen(line);
		buffer_length += line_length + 1;
		buffer = (char *) xrealloc(buffer, buffer_length + 1);
		strcat(buffer, line);
		free(line);
	} while (line_length > 1);

	if (strlen(buffer) == 0) {
		return(NULL);
	} else {
		return(strdup(buffer));
	}
}