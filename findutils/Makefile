# Makefile for busybox
#
# Copyright (C) 1999-2005 by Erik Andersen <andersen@codepoet.org>
#
# Licensed under the GPL v2, see the file LICENSE in this tarball.

ifndef top_srcdir
top_srcdir=..
endif
ifndef top_builddir
top_builddir=..
endif
srcdir=$(top_srcdir)/findutils
FINDUTILS_DIR:=./
include $(top_builddir)/Rules.mak
include $(top_builddir)/.config
include $(srcdir)/Makefile.in
all: $(libraries-y)
-include $(top_builddir)/.depend

clean:
	rm -f *.o *.a $(AR_TARGET)

