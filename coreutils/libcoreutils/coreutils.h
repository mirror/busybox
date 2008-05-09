/* vi: set sw=4 ts=4: */
/*
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#ifndef COREUTILS_H
#define COREUTILS_H		1

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

typedef int (*stat_func)(const char *fn, struct stat *ps);

int cp_mv_stat2(const char *fn, struct stat *fn_stat, stat_func sf);
int cp_mv_stat(const char *fn, struct stat *fn_stat);

mode_t getopt_mk_fifo_nod(char **argv);

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif

#endif
