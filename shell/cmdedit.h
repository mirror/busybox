#ifndef CMDEDIT_H
#define CMDEDIT_H

#ifdef BB_FEATURE_SH_COMMAND_EDITING
/* unix systems can #define POSIX to use termios, otherwise 
 * the bsd or sysv interface will be used 
 */

#include <stddef.h>

typedef size_t (*cmdedit_strwidth_proc)(char *);

void cmdedit_init(void);
void cmdedit_terminate(void);
void cmdedit_read_input(char* promptStr, char* command);		/* read a line of input */
void cmdedit_setwidth(int);		/* specify width of screen */
void cmdedit_histadd(char *);		/* adds entries to hist */
void cmdedit_strwidth(cmdedit_strwidth_proc);	/* to bind cmdedit_strlen */

extern int 	(*cmdedit_in_hook)(char *);
extern int 	(*cmdedit_out_hook)(char *);
extern int	(*cmdedit_tab_hook)(char *, int, int *);

#endif /* BB_FEATURE_SH_COMMAND_EDITING */

#endif /* CMDEDIT_H */
