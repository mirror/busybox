/*
 * fsck.h
 */

#define FSCK_ATTR(x) __attribute__(x)

#define EXIT_OK          0
#define EXIT_NONDESTRUCT 1
#define EXIT_DESTRUCT    2
#define EXIT_UNCORRECTED 4
#define EXIT_ERROR       8
#define EXIT_USAGE       16
#define EXIT_LIBRARY     128
