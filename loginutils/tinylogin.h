#ifndef BB_LOGINUTILS_SHELL_H
#define BB_LOGINUTILS_SHELL_H

extern void change_identity ( const struct passwd *pw );
extern void run_shell ( const char *shell, int loginshell, const char *command, char **additional_args );
extern int restricted_shell ( const char *shell );
extern void setup_environment ( const char *shell, int loginshell, int changeenv, const struct passwd *pw );
extern int correct_password ( const struct passwd *pw );

#endif
