#!/bin/sh

# I added in the extra "ls" so only source files that
# actually exist will show up in the compile list.
ls -1 ` \
    gcc -E -dM busybox.def.h | \
    sed -n -e '/^.*BB_FEATURE.*$/d;s/^#define.*\<BB_\(.*\)\>/\1.c/gp;' \
    | tr '[:upper:]' '[:lower:]' | sort
` 2>/dev/null | sed -e 's/\.c$/\.o/g'

