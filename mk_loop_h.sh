#!/bin/sh
#
# Figure out (i) the type of dev_t (ii) the defines for loop stuff
#
# Output of this script is normally redirected to "loop.h".

# Since 1.3.79 there is an include file <asm/posix_types.h>
# that defines __kernel_dev_t.
# (The file itself appeared in 1.3.78, but there it defined __dev_t.)
# If it exists, we use it, or, rather, <linux/posix_types.h> which
# avoids namespace pollution.  Otherwise we guess that __kernel_dev_t
# is an unsigned short (which is true on i386, but false on alpha).

# BUG: This test is actually broken if your gcc is not configured to
# search /usr/include, as may well happen with cross-compilers.
# It would be better to ask $(CC) if these files can be found.

if [ -f ${LINUXDIR:-/usr}/include/linux/posix_types.h ]; then
   echo '#include <linux/posix_types.h>'
   echo '#undef dev_t'
   echo '#define dev_t __kernel_dev_t'
else
   echo '#undef dev_t'
   echo '#define dev_t unsigned short'
fi

# Next we have to find the loop stuff itself.
# First try kernel source, then a private version.

if [ -f ${LINUXDIR:-/usr}/include/linux/loop.h ]; then
   echo '#include <linux/loop.h>'
else
   echo '#include "real_loop.h"'
fi

echo '#undef dev_t'

