#!/bin/sh

export LC_ALL=POSIX
export LC_CTYPE=POSIX

RAW=` \
    gcc -E -dM ${1:-Config.h} | \
    sed -n -e '/^.*BB_FEATURE.*$/d;s/^#define.*\<BB_\(.*\)\>/\1.c/gp;' \
    | tr A-Z a-z | sort
`
test "${RAW}" != "" ||  exit
if [ -d "$BB_SRC_DIR" ]; then cd $BB_SRC_DIR; fi
# By running $RAW through "ls", we avoid listing
# source files that don't exist.
ls $RAW 2>/dev/null | tr '\n' ' '

