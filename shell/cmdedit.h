/* vi: set sw=4 ts=4: */
#ifndef CMDEDIT_H
#define CMDEDIT_H

int cmdedit_read_input(char* promptStr, char* command);

#ifdef CONFIG_ASH
extern const char *cmdedit_path_lookup;
#endif

#ifdef CONFIG_FEATURE_COMMAND_SAVEHISTORY
void load_history(const char *fromfile);
void save_history(const char *tofile);
#endif

#if ENABLE_FEATURE_COMMAND_EDITING_VI
void setvimode(int viflag);
#endif

#endif /* CMDEDIT_H */
