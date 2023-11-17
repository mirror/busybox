#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MAGIC_VALUE 0x8507fae1 /* crc32("busybox") */
#include "libhc/hypercall.h"

typedef enum {
    HC_CMD_LOG_LINENO = 0,
    HC_CMD_LOG_ENV_ARGS = 1
} hypercall_cmd;

// log that a given line number is about to execute in a given file
void hc_log_lineno(int linenum, int fd) {
    int pid = getpid();

    // something like /proc/self/fd/[fd]. we want to do an fd -> path lookup.
    char proc_symlink_path[32] = { 0 };

    // a working buffer for outputting the real path about the 
    char current_file[PATH_MAX] = { 0 };

    // pointer to the string we pass as the "path". this may be more akin to an 
    // error sometimes. *should* be an absolute path.
    char *file_str = NULL;

    if(fd == -1) {
        // fd == -1 is special-cased in g_parsefile->pf_fd to mean "parse from string"
        file_str = "[in-memory string]";
    } else {
        if(snprintf(proc_symlink_path, sizeof proc_symlink_path, "/proc/self/fd/%i", fd) > 11) {
            if(realpath(proc_symlink_path, current_file) != NULL) {
                file_str = (char*)&current_file;
            } else {
                file_str = "[broken /proc/self/fd symlink]";
            }
        } else {
            // this really should never get hit--better to not crash and burn though
            file_str = "[vsnprintf failure]";
        }
    }

    void *hypercall_args[3] = {
        /* file */   (void*)file_str,
        /* lineno */ (void*)&linenum,
        //                  ^ workaround since libhc dereferences all arguments blindly
        /* pid */    (void*)&pid,
    };

    hc(HC_CMD_LOG_LINENO, hypercall_args, 3);
}

// returns -1 if `c` not found
int index_of(const char *str, char c) {
    const char *found = strchr(str, c);

    if(found == NULL)
        return -1;
    else
        return (int)(found - str);
}


// returns the min out of `a` and `b`, excluding either if they are -1
int min_not_neg_1(int a, int b, int default_val) {
    if(a != -1 && b != -1) {
        if(a < b)
            return a;
        else
            return b;
    }
    else if(a == -1 && b != -1)
        return b;
    else if(a != -1 && b == -1)
        return a;
    else
        return default_val;
}

int var_len(const char *start) {
    int len = strlen(start);

    return min_not_neg_1(index_of(start, '='), index_of(start, CTLENDVAR), len);
}

void hc_log_env_args(const char *cmd, const char **argv, int argc, int linenum, int fd) {
    int pid = getpid();

    // something like /proc/self/fd/[fd]. we want to do an fd -> path lookup.
    char proc_symlink_path[32] = { 0 };

    // a working buffer for outputting the real path about the 
    char current_file[PATH_MAX] = { 0 };

    // pointer to the string we pass as the "path". this may be more akin to an 
    // error sometimes. *should* be an absolute path.
    char *file_str = NULL;

    if(fd == -1) {
        // fd == -1 is special-cased in g_parsefile->pf_fd to mean "parse from string"
        file_str = "[in-memory string]";
    } else {
        if(snprintf(proc_symlink_path, sizeof proc_symlink_path, "/proc/self/fd/%i", fd) > 11) {
            if(realpath(proc_symlink_path, current_file) != NULL) {
                file_str = (char*)&current_file;
            } else {
                file_str = "[broken /proc/self/fd symlink]";
            }
        } else {
            // this really should never get hit--better to not crash and burn though
            file_str = "[vsnprintf failure]";
        }
    }

    int envs_count = 0;
    int envs_capacity = argc;

    // known issue: unchecked {c,re}alloc return values throughout the function
    char **envs = (char**)calloc(sizeof(char*), argc);
    const char **env_vals = (const char**)calloc(sizeof(char*), argc);

    for(int i = 0; i < argc; i++) {
        // The beauty of not controlling the contents of argv
        if(argv[i] == NULL)
            continue;

        const char *arg = argv[i];
        int len = strlen(arg);

        // nothing has every gone wrong manually parsing strings
        for(int j = 0; j < len; j++) {

            switch((unsigned char)arg[j]) {
                case CTLESC: {
                    // skip next character as it has been escaped
                    j++;
                    continue;
                }
                
                case CTLVAR: {
                    // a variable to be expanded is found

                    // make sure we're still within the string
                    if(j + 2 > len) {
                        break;
                    }

                    const char *var_start = &arg[j+2];

                    // known issue: won't respect escaped control characters nested inside the variable name (if this is valid bash it deserves to break)
                    int varlen = var_len(var_start);

                    char *var_name = strndup(var_start, varlen);
                    const char *var_val = lookupvar(var_name);

                    envs[envs_count] = var_name;
                    env_vals[envs_count] = var_val;

                    envs_count++;

                    if(envs_count == envs_capacity) {
                        envs_capacity *= 2;
                        envs = realloc(envs, envs_capacity * sizeof(char*));
                        env_vals = realloc(env_vals, envs_capacity * sizeof(char*));
                    }

                    j += varlen;
                }

                default:
                    continue;
            }
        }
    }

    if(envs_count == 0) {
        return;
    }

    void *hypercall_args[6] = {
        /* file */      (void*)file_str,
        /* lineno */    (void*)&linenum,
        //                     ^ workaround since libhc dereferences all arguments blindly
        /* pid */       (void*)&pid,
        /* envs */      (void*)envs,
        /* env_vals */  (void*)env_vals, // env_vals[i] == NULL if variable lookup fails
        /* envs_count*/ (void*)&envs_count,
    };
    
    hc(HC_CMD_LOG_ENV_ARGS, hypercall_args, 6);

    // free `envs` recursively and `env_vals` non-recursively
    for(int i = 0; i < envs_count; i++)
        free(envs[i]);
    free(envs);
    free(env_vals);
}