# Makefile for busybox
#
# Copyright (C) 1999-2000 Erik Andersen <andersee@debian.org>
# Copyright (C) 2000 Karl M. Hegbloom <karlheg@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

PROG      := busybox
VERSION   := 0.47pre
BUILDTIME := $(shell TZ=UTC date --utc "+%Y.%m.%d-%H:%M%z")
export VERSION

# If you want a static binary, turn this on.
DOSTATIC = false

# Set the following to `true' to make a debuggable build.
# Leave this set to `false' for production use.
# eg: `make DODEBUG=true tests'
# Do not enable this for production builds...
DODEBUG = false

# This enables compiling with dmalloc ( http://dmalloc.com/ )
# which is an excellent public domain mem leak and malloc problem
# detector.  To enable dmalloc, before running busybox you will
# want to first set up your environment.
# eg: `export DMALLOC_OPTIONS=debug=0x14f47d83,inter=100,log=logfile`
# Do not enable this for production builds...
DODMALLOC = false

# If you want large file summit support, turn this on.
# This has no effect if you don't have a kernel with lfs
# support, and a system with libc-2.1.3 or later.
# Some of the programs that can benefit from lfs support
# are dd, gzip, mount, tar, and mkfs_minix.
# LFS allows you to use the above programs for files
# larger than 2GB!
DOLFS = false


# If you are running a cross compiler, you may want to set this
# to something more interesting...
CROSS = #powerpc-linux-
CC = $(CROSS)gcc
STRIPTOOL = $(CROSS)strip

# To compile vs an alternative libc, you may need to use/adjust
# the following lines to meet your needs.  This is how I make
# busybox compile with uC-Libc...
#LIBCDIR=/home/andersen/CVS/uC-libc
#GCCINCDIR = $(shell gcc -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp")
#CFLAGS+=-nostdinc -fno-builtin -I$(LIBCDIR)/include -I$(GCCINCDIR)
#LDFLAGS+=-nostdlib
#LIBRARIES = $(LIBCDIR)/libc.a

#--------------------------------------------------------

# use '-Os' optimization if available, else use -O2
OPTIMIZATION = $(shell if $(CC) -Os -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
    then echo "-Os"; else echo "-O2" ; fi)

WARNINGS = -Wall

ifeq ($(DOLFS),true)
    # For large file summit support
    CFLAGS+=-D_FILE_OFFSET_BITS=64
endif
ifeq ($(DODMALLOC),true)
    # For testing mem leaks with dmalloc
    CFLAGS+=-DDMALLOC
    LIBRARIES = -ldmalloc
    # Force debug=true, since this is useless when not debugging...
    DODEBUG = true
endif
# -D_GNU_SOURCE is needed because environ is used in init.c
ifeq ($(DODEBUG),true)
    CFLAGS += $(WARNINGS) -g -D_GNU_SOURCE
    LDFLAGS += 
    STRIP   =
else
    CFLAGS  += $(WARNINGS) $(OPTIMIZATION) -fomit-frame-pointer -D_GNU_SOURCE
    LDFLAGS  += -s
    STRIP    = $(STRIPTOOL) --remove-section=.note --remove-section=.comment $(PROG)
endif
ifeq ($(DOSTATIC),true)
    LDFLAGS += --static
    #
    #use '-ffunction-sections -fdata-sections' and '--gc-sections' (if they 
    # work) to try and strip out any unused junk.  Doesn't do much for me, 
    # but you may want to give it a shot...
    #
    #ifeq ($(shell $(CC) -ffunction-sections -fdata-sections -S \
    #	-o /dev/null -xc /dev/null 2>/dev/null && $(LD) \
    #			--gc-sections -v >/dev/null && echo 1),1)
    #	CFLAGS += -ffunction-sections -fdata-sections
    #	LDFLAGS += --gc-sections
    #endif
endif

ifndef $(PREFIX)
    PREFIX = `pwd`/_install
endif

OBJECTS   = $(shell ./busybox.sh) busybox.o messages.o usage.o utility.o
CFLAGS    += -DBB_VER='"$(VERSION)"'
CFLAGS    += -DBB_BT='"$(BUILDTIME)"'
ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif


all: loop.h busybox busybox.links doc

doc: olddoc

# Old Docs...
olddoc: docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/BusyBox.txt: docs/busybox.pod
	@echo
	@echo BusyBox Documentation
	@echo
	- pod2text docs/busybox.pod > docs/BusyBox.txt

docs/BusyBox.1: docs/busybox.pod
	- pod2man --center=BusyBox --release="version $(VERSION)" docs/busybox.pod > docs/BusyBox.1

docs/BusyBox.html: docs/busybox.lineo.com/BusyBox.html
	- rm -f docs/BusyBox.html
	- ln -s busybox.lineo.com/BusyBox.html docs/BusyBox.html

docs/busybox.lineo.com/BusyBox.html: docs/busybox.pod
	- pod2html --noindex docs/busybox.pod > docs/busybox.lineo.com/BusyBox.html
	- rm -f pod2html*


# New docs based on DOCBOOK SGML
newdoc: docs/busybox.txt docs/busybox.pdf docs/busybox/busybox.html

docs/busybox.txt: docs/busybox.sgml
	@echo
	@echo BusyBox Documentation
	@echo
	(cd docs; sgmltools -b txt busybox.sgml)

docs/busybox.dvi: docs/busybox.sgml
	(cd docs; sgmltools -b dvi busybox.sgml)

docs/busybox.ps: docs/busybox.sgml
	(cd docs; sgmltools -b ps busybox.sgml)

docs/busybox.pdf: docs/busybox.ps
	(cd docs; ps2pdf busybox.ps)

docs/busybox/busybox.html: docs/busybox.sgml
	(cd docs/busybox.lineo.com; sgmltools -b html ../busybox.sgml)



busybox: $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBRARIES)
	$(STRIP)

busybox.links: busybox.def.h
	- ./busybox.mkll | sort >$@

nfsmount.o cmdedit.o: %.o: %.h
$(OBJECTS): %.o: busybox.def.h internal.h  %.c Makefile

utility.o: 

loop.h:
	@./mk_loop_h.sh

test tests:
	cd tests && $(MAKE) all

clean:
	cd tests && $(MAKE) clean
	- rm -f docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.lineo.com/BusyBox.html
	- rm -f docs/busybox.txt docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pdf docs/busybox.lineo.com/busybox.html
	- rm -rf docs/busybox _install
	- rm -f busybox.links *~ *.o core

distclean: clean
	- rm -f busybox
	- cd tests && $(MAKE) distclean

install: busybox busybox.links
	./install.sh $(PREFIX)

install-hardlinks: busybox busybox.links
	./install.sh $(PREFIX) --hardlinks

dist release: distclean doc
	cd ..;					\
	rm -rf busybox-$(VERSION);		\
	cp -a busybox busybox-$(VERSION);	\
						\
	find busybox-$(VERSION)/ -type d	\
				 -name CVS	\
				 -print		\
		| xargs rm -rf;			\
						\
	find busybox-$(VERSION)/ -type f	\
				 -name .cvsignore \
				 -print		\
		| xargs rm -f;			\
						\
	find busybox-$(VERSION)/ -type f	\
				 -name .\#*	\
				 -print		\
		| xargs rm -f;			\
						\
	tar -cvzf busybox-$(VERSION).tar.gz busybox-$(VERSION)/;

.PHONY: tags
tags:
	ctags -R .
