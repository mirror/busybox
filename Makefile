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
VERSION   := 0.48pre
BUILDTIME := $(shell TZ=UTC date --utc "+%Y.%m.%d-%H:%M%z")
export VERSION

# With a modern GNU make(1) (highly recommended, that's what all the
# developers use), all of the following configuration values can be
# overridden at the command line.  For example:
#   make CROSS=powerpc-linux- BB_SRC_DIR=$HOME/busybox PREFIX=/mnt/app

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

# If you have a "pristine" source directory, point BB_SRC_DIR to it.
# Experimental and incomplete; tell the mailing list
# <busybox@opensource.lineo.com> if you do or don't like it so far.
BB_SRC_DIR = .

# If you are running a cross compiler, you may want to set this
# to something more interesting, like "powerpc-linux-".
CROSS =
CC = $(CROSS)gcc
STRIPTOOL = $(CROSS)strip

# To compile vs an alternative libc, you may need to use/adjust
# the following lines to meet your needs.  This is how I make
# busybox compile with uC-Libc...
#LIBCDIR=/home/andersen/CVS/uC-libc
#GCCINCDIR = $(shell gcc -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp")
#CFLAGS+=-nostdinc -I$(LIBCDIR)/include -I$(GCCINCDIR)
#LDFLAGS+=-nostdlib
#LIBRARIES = $(LIBCDIR)/libc.a -lgcc

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

# Additional complications due to support for pristine source dir.
# Config.h in the build directory should take precedence over the
# copy in BB_SRC_DIR, both during the compilation phase and the
# shell script that finds the list of object files.
#
# Work in progress by <ldoolitt@recycle.lbl.gov>.
# If it gets in your way, set DISABLE_VPATH=yes
ifeq ($(DISABLE_VPATH),yes)
    CONFIG_H = Config.h
else
    VPATH = .:$(BB_SRC_DIR)
    CONFIG_LIST = $(addsuffix /Config.h,$(subst :, ,$(VPATH)))
    CONFIG_H    = $(word 1,$(shell ls -f -1 $(CONFIG_LIST) 2>/dev/null))
    CFLAGS += -I- $(patsubst %,-I%,$(subst :, ,$(VPATH)))
endif

OBJECTS   = $(shell $(BB_SRC_DIR)/busybox.sh $(CONFIG_H) $(BB_SRC_DIR)) busybox.o messages.o usage.o utility.o
CFLAGS    += -DBB_VER='"$(VERSION)"'
CFLAGS    += -DBB_BT='"$(BUILDTIME)"'
ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif


all: busybox busybox.links doc

doc: olddoc

# Old Docs...
olddoc: docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/BusyBox.txt: docs/busybox.pod
	@echo
	@echo BusyBox Documentation
	@echo
	- mkdir -p docs
	- (cd $(BB_SRC_DIR); pod2text docs/busybox.pod) > docs/BusyBox.txt

docs/BusyBox.1: docs/busybox.pod
	- mkdir -p docs
	- (cd $(BB_SRC_DIR); pod2man --center=BusyBox \
		--release="version $(VERSION)" \
		docs/busybox.pod ) > docs/BusyBox.1

docs/BusyBox.html: docs/busybox.lineo.com/BusyBox.html
	-@ rm -f docs/BusyBox.html
	-@ ln -s docs/busybox.lineo.com/BusyBox.html docs/BusyBox.html

docs/busybox.lineo.com/BusyBox.html: docs/busybox.pod
	-@ mkdir -p docs/busybox.lineo.com
	- (cd $(BB_SRC_DIR); pod2html --noindex docs/busybox.pod ) \
		> docs/busybox.lineo.com/BusyBox.html
	-@ rm -f pod2html*


# New docs based on DOCBOOK SGML
newdoc: docs/busybox.txt docs/busybox.pdf docs/busybox/busyboxdocumentation.html

docs/busybox.txt: docs/busybox.sgml
	@echo
	@echo BusyBox Documentation
	@echo
	- mkdir -p docs
	(cd docs; sgmltools -b txt $(BB_SRC_DIR)/busybox.sgml)

docs/busybox.dvi: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b dvi $(BB_SRC_DIR)/busybox.sgml)

docs/busybox.ps: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b ps $(BB_SRC_DIR)/busybox.sgml)

docs/busybox.pdf: docs/busybox.ps
	- mkdir -p docs
	(cd docs; ps2pdf $(BB_SRC_DIR)/busybox.ps)

docs/busybox/busyboxdocumentation.html: docs/busybox.sgml
	- mkdir -p docs
	(cd docs/busybox.lineo.com; sgmltools -b html ../busybox.sgml)



busybox: $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBRARIES)
	$(STRIP)

busybox.links: Config.h
	-$(BB_SRC_DIR)/busybox.mkll $(BB_SRC_DIR)/applets.h | sort >$@

nfsmount.o cmdedit.o: %.o: %.h
$(OBJECTS): %.o: %.c Config.h busybox.h Makefile

utility.o: loop.h

loop.h: mk_loop_h.sh
	@ sh $<

test tests:
	cd tests && $(MAKE) all

clean:
	- cd tests && $(MAKE) clean
	- rm -f docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.lineo.com/BusyBox.html
	- rm -f docs/busybox.txt docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pdf docs/busybox.lineo.com/busybox.html
	- rm -rf docs/busybox _install
	- rm -f busybox.links loop.h *~ *.o core

distclean: clean
	- rm -f busybox
	- cd tests && $(MAKE) distclean

install: busybox busybox.links
	$(BB_SRC_DIR)/install.sh $(PREFIX)

install-hardlinks: busybox busybox.links
	$(BB_SRC_DIR)/install.sh $(PREFIX) --hardlinks

debug_pristine:
	@ echo VPATH=\"$(VPATH)\"
	@ echo CONFIG_LIST=\"$(CONFIG_LIST)\"
	@ echo CONFIG_H=\"$(CONFIG_H)\"
	@ echo OBJECTS=\"$(OBJECTS)\"

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
