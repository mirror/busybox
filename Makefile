# Makefile for busybox
#
# Copyright (C) 1999,2000,2001 Erik Andersen <andersee@debian.org>
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
VERSION   := 0.49
BUILDTIME := $(shell TZ=UTC date --utc "+%Y.%m.%d-%H:%M%z")
export VERSION

# With a modern GNU make(1) (highly recommended, that's what all the
# developers use), all of the following configuration values can be
# overridden at the command line.  For example:
#   make CROSS=powerpc-linux- BB_SRC_DIR=$HOME/busybox PREFIX=/mnt/app

# If you want to add some simple compiler switches (like -march=i686),
# especially from the command line, use this instead of CFLAGS directly.
# For optimization overrides, it's better still to set OPTIMIZATION.
CFLAGS_EXTRA =
 
# If you want a static binary, turn this on.
DOSTATIC = false

# Set the following to `true' to make a debuggable build.
# Leave this set to `false' for production use.
# eg: `make DODEBUG=true tests'
# Do not enable this for production builds...
DODEBUG = false

# Setting this to `true' will cause busybox to directly use the system's
# password and group functions.  Assuming you use GNU libc, when this is
# `true', you will need to install the /etc/nsswitch.conf configuration file
# and the required libnss_* libraries. This generally makes your embedded
# system quite a bit larger... If you leave this off, busybox will directly
# use the /etc/password, /etc/group files (and your system will be smaller, and
# I will get fewer emails asking about how glibc NSS works).  Enabling this adds
# just 1.4k to the binary size (which is a _lot_ less then glibc NSS costs),
# Most people will want to leave this set to false.
USE_SYSTEM_PWD_GRP = false

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

# To compile vs uClibc, just use the compiler wrapper built by uClibc...
# Isn't that easy?  Right now, uClibc needs BB_FEATURE_NFSMOUNT disabled 
# since uClibc's nfs support isn't ready yet.
#CC = ../uClibc/extra/gcc-uClibc/gcc-uClibc-i386

# To compile vs some other alternative libc, you may need to use/adjust
# the following lines to meet your needs...
#LIBCDIR=/usr/i486-linuxlibc1/
#LDFLAGS+=-nostdlib
#LIBRARIES = $(LIBCDIR)/libc.a -lgcc
#CROSS_CFLAGS+=-nostdinc -I$(LIBCDIR)/include -I$(GCCINCDIR)
#GCCINCDIR = $(shell gcc -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp")

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
ifeq ($(DODEBUG),true)
    CFLAGS  += $(WARNINGS) -g -D_GNU_SOURCE
    LDFLAGS += -Wl,-warn-common
    STRIP    =
else
    CFLAGS  += $(WARNINGS) $(OPTIMIZATION) -fomit-frame-pointer -D_GNU_SOURCE
    LDFLAGS += -s -Wl,-warn-common
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
    CFLAGS += -I- $(patsubst %,-I%,$(subst :, ,$(VPATH))) $(CROSS_CFLAGS)
endif

OBJECTS   = $(shell $(BB_SRC_DIR)/busybox.sh $(CONFIG_H) $(BB_SRC_DIR)) busybox.o messages.o usage.o utility.o
CFLAGS    += -DBB_VER='"$(VERSION)"'
CFLAGS    += -DBB_BT='"$(BUILDTIME)"'
ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif

ifneq ($(USE_SYSTEM_PWD_GRP),true)
    PWD_LIB   = pwd_grp/libpwd.a
    LIBRARIES += $(PWD_LIB)
else
    CFLAGS    += -DUSE_SYSTEM_PWD_GRP
endif


# Put user-supplied flags at the end, where they
# have a chance of winning.
CFLAGS += $(CFLAGS_EXTRA)

.EXPORT_ALL_VARIABLES:

all: busybox busybox.links doc

doc: olddoc

# Old Docs...
olddoc: docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/BusyBox.txt: docs/busybox.pod
	@echo
	@echo BusyBox Documentation
	@echo
	-mkdir -p docs
	-pod2text $(BB_SRC_DIR)/docs/busybox.pod > docs/BusyBox.txt

docs/BusyBox.1: docs/busybox.pod
	- mkdir -p docs
	- pod2man --center=BusyBox --release="version $(VERSION)" \
		$(BB_SRC_DIR)/docs/busybox.pod > docs/BusyBox.1

docs/BusyBox.html: docs/busybox.lineo.com/BusyBox.html
	-@ rm -f docs/BusyBox.html
	-@ ln -s busybox.lineo.com/BusyBox.html docs/BusyBox.html

docs/busybox.lineo.com/BusyBox.html: docs/busybox.pod
	-@ mkdir -p docs/busybox.lineo.com
	-  pod2html --noindex $(BB_SRC_DIR)/docs/busybox.pod > \
	    docs/busybox.lineo.com/BusyBox.html
	-@ rm -f pod2html*


# New docs based on DOCBOOK SGML
newdoc: docs/busybox.txt docs/busybox.pdf docs/busybox/busyboxdocumentation.html

docs/busybox.txt: docs/busybox.sgml
	@echo
	@echo BusyBox Documentation
	@echo
	- mkdir -p docs
	(cd docs; sgmltools -b txt busybox.sgml)

docs/busybox.dvi: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b dvi busybox.sgml)

docs/busybox.ps: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b ps busybox.sgml)

docs/busybox.pdf: docs/busybox.ps
	- mkdir -p docs
	(cd docs; ps2pdf busybox.ps)

docs/busybox/busyboxdocumentation.html: docs/busybox.sgml
	- mkdir -p docs
	(cd docs/busybox.lineo.com; sgmltools -b html ../busybox.sgml)



busybox: $(PWD_LIB) $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBRARIES)
	$(STRIP)

$(PWD_LIB):
	$(MAKE) -eC pwd_grp

busybox.links: Config.h applets.h
	- $(BB_SRC_DIR)/busybox.mkll $(CONFIG_H) $(BB_SRC_DIR)/applets.h >$@

nfsmount.o cmdedit.o: %.o: %.h
$(OBJECTS): %.o: %.c Config.h busybox.h applets.h Makefile
	$(CC) $(CFLAGS) -c $*.c -o $*.o

utility.o: loop.h

loop.h: mk_loop_h.sh
	@ sh $<

test tests:
	cd tests && $(MAKE) all

clean:
	- cd tests && $(MAKE) clean
	- cd pwd_grp && $(MAKE) clean
	- rm -f docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.lineo.com/BusyBox.html
	- rm -f docs/busybox.txt docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pdf docs/busybox.lineo.com/busybox.html
	- rm -f Config.h.ORG bb.def.h busybox.REGRESS.sh.results bb.OptionsAndFeatures
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
