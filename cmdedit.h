#ifndef CMDEDIT_H
#define CMDEDIT_H

#ifdef BB_FEATURE_SH_COMMAND_EDITING
#include <stddef.h>

void cmdedit_init(void);
void cmdedit_terminate(void);
void cmdedit_read_input(char* promptStr, char* command);		/* read a line of input */

#endif /* BB_FEATURE_SH_COMMAND_EDITING */

#endif /* CMDEDIT_H */
