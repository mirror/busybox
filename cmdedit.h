/*
 * Termios command line History and Editting for NetBSD sh (ash)
 * Copyright (c) 1999
 *      Main code:            Adam Rogoyski <rogoyski@cs.utexas.edu> 
 *      Etc:                  Dave Cinege <dcinege@psychosis.com>
 *      Adjusted for busybox: Erik Andersen <andersee@debian.org>
 *
 * You may use this code as you wish, so long as the original author(s)
 * are attributed in any redistributions of the source code.
 * This code is 'as is' with no warranty.
 * This code may safely be consumed by a BSD or GPL license.
 *
 */

extern int cmdedit_read_input(char* prompt, int inputFd, int outputFd, char command[BUFSIZ]);
extern void cmdedit_init(void);

