#ifndef GETLINE_H
#define GETLINE_H

/* unix systems can #define POSIX to use termios, otherwise 
 * the bsd or sysv interface will be used 
 */

#ifdef __STDC__ 
#include <stddef.h>

typedef size_t (*cmdedit_strwidth_proc)(char *);

void cmdedit_read_input(char* promptStr, char* command);		/* read a line of input */
void cmdedit_setwidth(int);		/* specify width of screen */
void cmdedit_histadd(char *);		/* adds entries to hist */
void cmdedit_strwidth(cmdedit_strwidth_proc);	/* to bind cmdedit_strlen */

extern int 	(*cmdedit_in_hook)(char *);
extern int 	(*cmdedit_out_hook)(char *);
extern int	(*cmdedit_tab_hook)(char *, int, int *);

#else	/* not __STDC__ */

void cmdedit_read_input(char* promptStr, char* command);
void cmdedit_setwidth();
void cmdedit_histadd();
void cmdedit_strwidth();

extern int 	(*cmdedit_in_hook)();
extern int 	(*cmdedit_out_hook)();
extern int	(*cmdedit_tab_hook)();

#endif /* __STDC__ */

#endif /* GETLINE_H */
