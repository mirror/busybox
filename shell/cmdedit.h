#ifndef CMDEDIT_H
#define CMDEDIT_H

int     cmdedit_read_input(char* promptStr, char* command);

void    load_history ( char *fromfile );
void    save_history ( char *tofile );

#endif /* CMDEDIT_H */
