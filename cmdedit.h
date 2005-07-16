#ifndef CMDEDIT_H
#define CMDEDIT_H

int     cmdedit_read_input(char* promptStr, char* command);

#ifdef CONFIG_FEATURE_COMMAND_SAVEHISTORY
void    load_history ( const char *fromfile );
void    save_history ( const char *tofile );
#endif

#endif /* CMDEDIT_H */
