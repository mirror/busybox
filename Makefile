# Makefile for busybox
#
# Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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
VERSION   := 0.61.pre
BUILDTIME := $(shell TZ=UTC date -u "+%Y.%m.%d-%H:%M%z")
TOPDIR    := ${shell /bin/pwd}
HOSTCC    := gcc
HOSTCFLAGS:= -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer


# What OS are you compiling busybox for?  This allows you to include
# OS specific things, syscall overrides, etc.
TARGET_OS := linux

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
DODEBUG = false

# This enables compiling with dmalloc ( http://dmalloc.com/ )
# which is an excellent public domain mem leak and malloc problem
# detector.  To enable dmalloc, before running busybox you will
# want to first set up your environment.
# eg: `export DMALLOC_OPTIONS=debug=0x34f47d83,inter=100,log=logfile`
# The debug= value is generated using the following command
# dmalloc -p log-stats -p log-non-free -p log-bad-space -p log-elapsed-time \
#      -p check-fence -p check-heap -p check-lists -p check-blank \
#      -p check-funcs -p realloc-copy -p allow-free-null
# Do not enable this for production builds...
DODMALLOC = false

# Electric-fence is another very useful malloc debugging library.
# Do not enable this for production builds...
DOEFENCE  = false

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
# <busybox@busybox.net> if you do or don't like it so far.
BB_SRC_DIR =

# If you are running a cross compiler, you may want to set CROSS
# to something more interesting, like "arm-linux-".
CROSS =
CC              = $(CROSS)gcc
AR              = $(CROSS)ar
AS              = $(CROSS)as
LD              = $(CROSS)ld
NM              = $(CROSS)nm
STRIP           = $(CROSS)strip
CPP             = $(CC) -E
MAKEFILES       = $(TOPDIR)/.config
export VERSION BUILDTIME TOPDIR HOSTCC HOSTCFLAGS CROSS CC AR AS LD NM STRIP CPP


# To compile vs uClibc, just use the compiler wrapper built by uClibc...
# Everything should compile and work as expected these days...
#CC = /usr/i386-linux-uclibc/usr/bin/i386-uclibc-gcc

# To compile vs some other alternative libc, you may need to use/adjust
# the following lines to meet your needs...
#
# If you are using Red Hat 6.x with the compatible RPMs (for developing under
# Red Hat 5.x and glibc 2.0) uncomment the following.  Be sure to read about
# using the compatible RPMs (compat-*) at http://www.redhat.com !
#LIBCDIR=/usr/i386-glibc20-linux
#
# The following is used for libc5 (if you install altgcc and libc5-altdev
# on a Debian system).  
#LIBCDIR=/usr/i486-linuxlibc1
#
# For other libraries, you are on your own...
#LDFLAGS+=-nostdlib
#LIBRARIES = $(LIBCDIR)/lib/libc.a -lgcc
#CROSS_CFLAGS+=-nostdinc -I$(LIBCDIR)/include -I$(GCCINCDIR)
#GCCINCDIR = $(shell gcc -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp")

# use '-Os' optimization if available, else use -O2
OPTIMIZATION := ${shell if $(CC) -Os -S -o /dev/null -xc /dev/null \
	>/dev/null 2>&1; then echo "-Os"; else echo "-O2" ; fi}
GCC_STACK_BOUNDRY := ${shell if $(CC) -mpreferred-stack-boundary=2 -S -o /dev/null -xc /dev/null \
	>/dev/null 2>&1; then echo "-mpreferred-stack-boundary=2"; else echo "" ; fi}
OPTIMIZATIONS=$(OPTIMIZATION) -fomit-frame-pointer $(GCC_STACK_BOUNDRY) #-fstrict-aliasing -march=i386 -mcpu=i386 -malign-functions=0 -malign-jumps=0
WARNINGS=-Wall -Wstrict-prototypes -Wshadow
CFLAGS = -I$(TOPDIR)/include
ARFLAGS = -r

#
#--------------------------------------------------------
# If you're going to do a lot of builds with a non-vanilla configuration,
# it makes sense to adjust parameters above, so you can type "make"
# by itself, instead of following it by the same half-dozen overrides
# every time.  The stuff below, on the other hand, is probably less
# prone to casual user adjustment.
# 

ifeq ($(strip $(DOLFS)),true)
    # For large file summit support
    CFLAGS+=-D_FILE_OFFSET_BITS=64 -D__USE_FILE_OFFSET64
endif
ifeq ($(strip $(DODMALLOC)),true)
    # For testing mem leaks with dmalloc
    CFLAGS+=-DDMALLOC
    LIBRARIES = -ldmalloc
    # Force debug=true, since this is useless when not debugging...
    DODEBUG = true
else
    ifeq ($(strip $(DOEFENCE)),true)
	LIBRARIES = -lefence
	# Force debug=true, since this is useless when not debugging...
	DODEBUG = true
    endif
endif
ifeq ($(strip $(DODEBUG)),true)
    CFLAGS  += $(WARNINGS) -g -D_GNU_SOURCE
    LDFLAGS += -Wl,-warn-common
    STRIPCMD = /bin/true -Not_stripping_since_we_are_debugging
else
    CFLAGS  += $(WARNINGS) $(OPTIMIZATIONS) -D_GNU_SOURCE
    LDFLAGS += -s -Wl,-warn-common
    STRIPCMD    = $(STRIP) --remove-section=.note --remove-section=.comment
endif
ifeq ($(strip $(DOSTATIC)),true)
    LDFLAGS += --static
endif

ifndef $(PREFIX)
    PREFIX = `pwd`/_install
endif

# Additional complications due to support for pristine source dir.
# Include files in the build directory should take precedence over
# the copy in BB_SRC_DIR, both during the compilation phase and the
# shell script that finds the list of object files.
# Work in progress by <ldoolitt@recycle.lbl.gov>.
#
ifneq ($(strip $(BB_SRC_DIR)),)
    VPATH = $(BB_SRC_DIR)
endif

OBJECTS   = $(APPLET_SOURCES:.c=.o) busybox.o usage.o applets.o
CFLAGS    += $(CROSS_CFLAGS)
ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif

# Put user-supplied flags at the end, where they
# have a chance of winning.
CFLAGS += $(CFLAGS_EXTRA)

.EXPORT_ALL_VARIABLES:

all:    do-it-all

#
# Make "config" the default target if there is no configuration file or
# "depend" the target if there is no top-level dependency information.
ifeq (.config,$(wildcard .config))
include .config
ifeq (.depend,$(wildcard .depend))
include .depend 
do-it-all:      busybox busybox.links doc
else
CONFIGURATION = depend
do-it-all:      depend
endif
else
CONFIGURATION = menuconfig
do-it-all:      menuconfig
endif


SUBDIRS =applets archival archival/libunarchive console-tools editors \
	fileutils findutils init miscutils modutils networking procps \
	pwd_grp shell shellutils sysklogd textutils util-linux libbb

bbsubdirs: $(patsubst %, _dir_%, $(SUBDIRS))

$(patsubst %, _dir_%, $(SUBDIRS)) : dummy include/config/MARKER
	$(MAKE) CFLAGS="$(CFLAGS)" -C $(patsubst _dir_%, %, $@)

busybox: config.h dep-files bbsubdirs
	$(CC) $(LDFLAGS) -o $@ applets/busybox.o $(shell find $(SUBDIRS) -name \*.a) $(LIBRARIES)
	$(STRIPCMD) $(PROG)

busybox.links: applets/busybox.mkll
	- $(SHELL) $^ >$@

install: applets/install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX)

install-hardlinks: applets/install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX) --hardlinks


# Documentation Targets
doc: olddoc

# Old Docs...
olddoc: docs/busybox.pod docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/busybox.pod : docs/busybox_header.pod include/usage.h docs/busybox_footer.pod
	- ( cat docs/busybox_header.pod; \
	    docs/autodocifier.pl include/usage.h; \
	    cat docs/busybox_footer.pod ) > docs/busybox.pod

docs/BusyBox.txt: docs/busybox.pod
	@echo
	@echo BusyBox Documentation
	@echo
	-mkdir -p docs
	-pod2text $< > $@

docs/BusyBox.1: docs/busybox.pod
	- mkdir -p docs
	- pod2man --center=BusyBox --release="version $(VERSION)" \
		$< > $@

docs/BusyBox.html: docs/busybox.net/BusyBox.html
	- mkdir -p docs
	-@ rm -f docs/BusyBox.html
	-@ ln -s busybox.net/BusyBox.html docs/BusyBox.html

docs/busybox.net/BusyBox.html: docs/busybox.pod
	-@ mkdir -p docs/busybox.net
	-  pod2html --noindex $< > \
	    docs/busybox.net/BusyBox.html
	-@ rm -f pod2htm*


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
	(cd docs/busybox.net; sgmltools -b html ../busybox.sgml)

# The nifty new buildsystem stuff
scripts/mkdep: scripts/mkdep.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/mkdep scripts/mkdep.c

scripts/split-include: scripts/split-include.c
	$(HOSTCC) $(HOSTCFLAGS) -o scripts/split-include scripts/split-include.c

dep-files: scripts/mkdep
	@if [ ! -f .depend ] ; then \
		rm -f .depend .hdepend; \
		mkdir -p $(TOPDIR)/include/config; \
		scripts/mkdep -I $(TOPDIR)/include -- \
			`find $(TOPDIR) -name \*.c -print` >> .depend; \
		scripts/mkdep -I $(TOPDIR)/include -- \
			`find $(TOPDIR) -name \*.h -print` >> .hdepend; \
		$(MAKE) $(patsubst %,_sfdep_%,$(SUBDIRS)) _FASTDEP_ALL_SUB_DIRS="$(SUBDIRS)" ; \
	fi;


depend dep: config.h dep-files
	@ echo -e "\n\nNow run 'make' to build BusyBox\n\n"

BB_SHELL := ${shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi ; fi}

include/config/MARKER: scripts/split-include include/config.h
	scripts/split-include include/config.h include/config
	@ touch include/config/MARKER

config.h:
	@if [ ! -f include/config.h ] ; then \
		make oldconfig; \
	fi;

menuconfig:
	mkdir -p $(TOPDIR)/include/config
	$(MAKE) -C scripts/lxdialog all
	$(BB_SHELL) scripts/Menuconfig sysdeps/$(TARGET_OS)/config.in

config:
	mkdir -p $(TOPDIR)/include/config
	$(BB_SHELL) scripts/Configure sysdeps/$(TARGET_OS)/config.in

oldconfig:
	mkdir -p $(TOPDIR)/include/config
	$(BB_SHELL) scripts/Configure -d sysdeps/$(TARGET_OS)/config.in


ifdef CONFIGURATION
..$(CONFIGURATION):
	@echo
	@echo "You have a bad or nonexistent" .$(CONFIGURATION) ": running 'make" $(CONFIGURATION)"'"
	@echo
	$(MAKE) $(CONFIGURATION)
	@echo
	@echo "Successful. Try re-making (ignore the error that follows)"
	@echo
	exit 1

dummy:

else

dummy:

endif

include Rules.mak

# Testing...
test tests:
	# old way of doing it
	#cd tests && $(MAKE) all
	# new way of doing it
	cd tests && ./tester.sh

# Cleanup
clean:
	- $(MAKE) -C tests clean
	- $(MAKE) -C scripts/lxdialog clean
	- rm -f docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.net/BusyBox.html
	- rm -f docs/busybox.txt docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pdf docs/busybox.net/busybox.html \
	    docs/busybox _install pod2htm* *.gdb *.elf *~ core
	- rm -f busybox busybox.links libbb/loop.h .config.old .hdepend
	- rm -f scripts/split-include scripts/mkdep .*config.log
	- rm -rf include/config include/config.h
	- find -name .\*.flags -o -name .depend -exec rm -f {} \;
	- find -name \*.o -exec rm -f {} \;
	- find -name \*.a -exec rm -f {} \;

distclean: clean
	- rm -f busybox 
	- cd tests && $(MAKE) distclean

dist release: distclean doc
	cd ..;					\
	rm -rf busybox-$(VERSION);		\
	cp -a busybox busybox-$(VERSION);	\
						\
	find busybox-$(VERSION)/ -type d	\
				 -name CVS	\
				 -print		\
		-exec rm -rf {} \; ;            \
						\
	find busybox-$(VERSION)/ -type f	\
				 -name .\#*	\
				 -print		\
		-exec rm -f {} \;  ;            \
						\
	tar -cvzf busybox-$(VERSION).tar.gz busybox-$(VERSION)/;



.PHONY: tags check
tags:
	ctags -R .

check: busybox
	cd testsuite && ./runtest
