#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef enum {
    HC_CMD_LOG_LINENO = 0,
    HC_CMD_LOG_SETFILE = 1
} hypercall_cmd;

// log that a given line number is about to execute in a given file
void hc_log_lineno(int lineno, int fd) {
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
                file_str = &current_file;
            } else {
                file_str = "[broken /proc/self/fd symlink]";
            }
        } else {
            // this really should never get hit--better to not crash and burn though
            file_str = "[vsnprintf failure]";
        }
    }

    int pid = getpid();

    void *hypercall_args[3] = {
        /* file */   (void*)file_str,
        /* lineno */ (void*)&lineno,
        //                  ^ workaround since libhc dereferences all arguments blindly
        /* pid */    (void*)&pid,
    };

    hc(HC_CMD_LOG_LINENO, hypercall_args, 3);
}
