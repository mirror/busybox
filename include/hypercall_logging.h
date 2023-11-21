#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MAGIC_VALUE 0x8507fae1 /* crc32("busybox") */
#include "libhc/hypercall.h"

typedef enum {
    // log the line number of each command before it executes
    HC_CMD_LOG_LINENO = 0,

    // log environment variable expansions in commands
    HC_CMD_LOG_ENV_ARGS = 1,

    // log checks in the form of `[[ -n "$ENV_VAR" ]]`
    HC_CMD_LOG_CHECK_IF_ENV_EXISTS = 2,

    // log checks in the form of `[[ "$ENV_VAR" == "val" ]]`
    HC_CMD_LOG_CHECK_IF_ENV_EQUALS = 3,

    // log checks in the form of `[[ "$ENV_VAR" != "val" ]]`
    HC_CMD_LOG_CHECK_IF_ENV_NOT_EQUAL = 4,

    HC_CMD_LOG_FILE_CHECK = 5
} hypercall_cmd;

typedef enum {
    // -e
    HC_FILE_TYPE_ANY = 0,
    // -b
    HC_FILE_TYPE_BLOCK_SPECIAL = 1,
    // -c
    HC_FILE_TYPE_CHARACTER_SPECIAL = 2,
    // -d
    HC_FILE_TYPE_DIRECTORY = 3,
    // -f
    HC_FILE_TYPE_REGULAR_FILE = 4,
    // -g
    HC_FILE_TYPE_SET_GID = 5,
    // -G
    HC_FILE_TYPE_OWNED_BY_EGID = 6,
    // -h
    HC_FILE_TYPE_SYM_LINK = 7,
    // -k
    HC_FILE_TYPE_STICKY_BIT = 8,
    // -L
    HC_FILE_TYPE_SYM_LINK2 = 9,
    // -O
    HC_FILE_TYPE_OWNED_BY_EUID = 10,
    // -p
    HC_FILE_TYPE_NAMED_PIPE = 11,
    // -r
    HC_FILE_TYPE_READABLE = 12,
    // -s
    HC_FILE_TYPE_NON_EMPTY = 13,
    // -S
    HC_FILE_TYPE_SOCKET = 14,
    // -t
    HC_FILE_TYPE_TTY = 15,
    // -u
    HC_FILE_TYPE_SET_UID = 16,
    // -w
    HC_FILE_WRITABLE = 17,
    // -x
    HC_FILE_EXECUTABLE = 18
} hc_file_type;

#define HC_FILE_TYPE_MAX 19

const char *hc_file_types[HC_FILE_TYPE_MAX] = {
    "-e", // 0
    "-b", // 1
    "-c", // 2
    "-d", // 3
    "-f", // 4
    "-g", // 5
    "-G", // 6
    "-h", // 7
    "-k", // 8
    "-L", // 9
    "-O", // 10
    "-p", // 11
    "-r", // 12
    "-s", // 13
    "-S", // 14
    "-t", // 15
    "-u", // 16
    "-w", // 17
    "-x"  // 18
};

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

char *find_var_in_arg(const char *arg) {
    int len = strlen(arg);

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

                return var_name;
            }

            default:
                continue;
        }
    }

    return NULL;
}

void hc_log_check_if_env_exists(const char *file, int line, int pid, const char *var, int status) {
    void *hypercall_args[5] = {
        /* file */   (void*)file,
        /* lineno */ (void*)&line,
        //                  ^ workaround since libhc dereferences all arguments blindly
        /* pid */    (void*)&pid,
        /* var */    (void*)var,
        /* status */ (void*)&status,
    };

    hc(HC_CMD_LOG_CHECK_IF_ENV_EXISTS, hypercall_args, 5);
}

void hc_log_check_if_env_equals(const char *file, int line, int pid, const char *var, const char *value, int status, hypercall_cmd cmd) {
    void *hypercall_args[6] = {
        /* file */   (void*)file,
        /* lineno */ (void*)&line,
        //                  ^ workaround since libhc dereferences all arguments blindly
        /* pid */    (void*)&pid,
        /* var */    (void*)var,
        /* status */ (void*)&status,
        /* value */  (void*)value,
    };

    hc(cmd, hypercall_args, 6);
}

void hc_log_file_type_check(const char *file, int line, int pid, const char *path, hc_file_type file_type, int status) {
    void *hypercall_args[6] = {
        /* file */       (void*)file,
        /* lineno */     (void*)&line,
        //                      ^ workaround since libhc dereferences all arguments blindly
        /* pid */        (void*)&pid,
        /* path */       (void*)path,
        /* status */     (void*)&status,
        /* file_type */  (void*)&file_type,
    };

    hc(HC_CMD_LOG_FILE_CHECK, hypercall_args, 6);
}

void hc_log_if_cond(const struct ncmd *cond, int status) {
    int fd = g_parsefile->pf_fd;
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

    if(strcmp(cond->args->narg.text, "[") == 0
        || strcmp(cond->args->narg.text, "[[") == 0
        || strcmp(cond->args->narg.text, "test") == 0)
    {
        int argc = 0;
        union node *argp = cond->args->narg.next;
        for (; argp; argp = argp->narg.next)
            argc++;

        const char **arg_list = (const char**)malloc( sizeof(char*) * argc );

        int arg_i = 0;
        argp = cond->args->narg.next;
		for (argp = argp; argp; argp = argp->narg.next) {
            arg_list[arg_i++] = argp->narg.text;
        }

        if(argc > 1) {
            if(strcmp(arg_list[0], "-n") == 0) {
                char *var = find_var_in_arg(arg_list[1]);

                if(var != NULL) {
                    hc_log_check_if_env_exists(file_str, cond->linno, pid, var, status);

                    free(var);
                }
            }

            if(arg_list[0][0] == '-') {
                for(int i = 0; i < HC_FILE_TYPE_MAX; i++) {
                    if(strcmp(arg_list[0], hc_file_types[i]) == 0) {

                        hc_log_file_type_check(file_str, cond->linno, pid, arg_list[1], i, status);
                        break;
                    }
                }
            }
        }

        if(argc >= 3) {
            if(strcmp(arg_list[1], "=") == 0 || strcmp(arg_list[1], "==") == 0) {
                char *left = find_var_in_arg(arg_list[0]);
                char *right = find_var_in_arg(arg_list[2]);

                if(left != NULL || right != NULL) {
                    if(left == NULL)
                        hc_log_check_if_env_equals(file_str, cond->linno, pid, right, arg_list[0], status, HC_CMD_LOG_CHECK_IF_ENV_EQUALS);
                    else if(right == NULL)
                        hc_log_check_if_env_equals(file_str, cond->linno, pid, left, arg_list[2], status, HC_CMD_LOG_CHECK_IF_ENV_EQUALS);
                }

                if(left != NULL)
                    free(left);
                if(right != NULL)
                    free(right);
            } else if(strcmp(arg_list[1], "!=") == 0) {
                char *left = find_var_in_arg(arg_list[0]);
                char *right = find_var_in_arg(arg_list[2]);

                if(left != NULL || right != NULL) {
                    if(left == NULL)
                        hc_log_check_if_env_equals(file_str, cond->linno, pid, right, arg_list[0], status, HC_CMD_LOG_CHECK_IF_ENV_NOT_EQUAL);
                    else if(right == NULL)
                        hc_log_check_if_env_equals(file_str, cond->linno, pid, left, arg_list[2], status, HC_CMD_LOG_CHECK_IF_ENV_NOT_EQUAL);
                }

                if(left != NULL)
                    free(left);
                if(right != NULL)
                    free(right);
            }
        }
    }
}
