# Makefile for busybox
#
# Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
#
# Licensed under GPLv2 or later, see file LICENSE in this tarball for details.

#ifndef top_srcdir
#top_srcdir=../..
#endif
#ifndef top_builddir
#top_builddir=../..
#endif
srcdir=$(top_srcdir)/networking/udhcp
UDHCP_DIR:=./
include $(top_srcdir)/Rules.mak
include $(top_builddir)/.config
include Makefile.in
all: $(libraries-y)
-include $(top_builddir)/.depend

clean:
	rm -f *.o *.a $(AR_TARGET)

