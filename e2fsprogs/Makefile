# Makefile for busybox
#
# Copyright (C) 1999-2005 by Erik Andersen <andersen@codepoet.org>
#
# Licensed under the GPL v2, see the file LICENSE in this tarball.

top_srcdir=..
top_builddir=..
srcdir=$(top_srcdir)/e2fsprogs
E2FSPROGS_DIR:=./
include $(top_builddir)/.config
include $(top_builddir)/Rules.mak
include Makefile.in
all: $(libraries-y)
-include $(top_builddir)/.depend

clean:
	rm -f *.o *.a */*.o $(AR_TARGET)
