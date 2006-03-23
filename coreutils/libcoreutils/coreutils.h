#ifndef COREUTILS_H
#define COREUTILS_H		1

typedef int (*stat_func)(const char *fn, struct stat *ps);

extern int cp_mv_stat2(const char *fn, struct stat *fn_stat, stat_func sf);
extern int cp_mv_stat(const char *fn, struct stat *fn_stat);

extern mode_t getopt_mk_fifo_nod(int argc, char **argv);
extern FILE *xgetoptfile_sort_uniq(char **argv, const char *mode);

#endif
