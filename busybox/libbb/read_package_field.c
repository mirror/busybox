#include <stdlib.h>
#include <string.h>
#include "libbb.h"

/*
 * Gets the next package field from package_buffer, seperated into the field name
 * and field value, it returns the int offset to the first character of the next field
 */
int read_package_field(const char *package_buffer, char **field_name, char **field_value)
{
	int offset_name_start = 0;
	int offset_name_end = 0;
	int offset_value_start = 0;
	int offset_value_end = 0;
	int offset = 0;
	int next_offset;
	int name_length;
	int value_length;
	int exit_flag = FALSE;

	if (package_buffer == NULL) {
		*field_name = NULL;
		*field_value = NULL;
		return(-1);
	}
	while (1) {
		next_offset = offset + 1;
		switch (package_buffer[offset]) {
			case('\0'):
				exit_flag = TRUE;
				break;
			case(':'):
				if (offset_name_end == 0) {
					offset_name_end = offset;
					offset_value_start = next_offset;
				}
				/* TODO: Name might still have trailing spaces if ':' isnt
				 * immediately after name */
				break;
			case('\n'):
				/* TODO: The char next_offset may be out of bounds */
				if (package_buffer[next_offset] != ' ') {
					exit_flag = TRUE;
					break;
				}
			case('\t'):
			case(' '):
				/* increment the value start point if its a just filler */
				if (offset_name_start == offset) {
					offset_name_start++;
				}
				if (offset_value_start == offset) {
					offset_value_start++;
				}
				break;
		}
		if (exit_flag == TRUE) {
			/* Check that the names are valid */
			offset_value_end = offset;
			name_length = offset_name_end - offset_name_start;
			value_length = offset_value_end - offset_value_start;
			if (name_length == 0) {
				break;
			}
			if ((name_length > 0) && (value_length > 0)) {
				break;
			}

			/* If not valid, start fresh with next field */
			exit_flag = FALSE;
			offset_name_start = offset + 1;
			offset_name_end = 0;
			offset_value_start = offset + 1;
			offset_value_end = offset + 1;
			offset++;
		}
		offset++;
	}
	if (name_length == 0) {
		*field_name = NULL;
	} else {
		*field_name = xstrndup(&package_buffer[offset_name_start], name_length);
	}
	if (value_length > 0) {
		*field_value = xstrndup(&package_buffer[offset_value_start], value_length);
	} else {
		*field_value = NULL;
	}
	return(next_offset);
}

