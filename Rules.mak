# Rules.make for busybox
#
# Copyright (C) 2002 Erik Andersen <andersee@debian.org>
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
HOSTCC    := gcc
HOSTCFLAGS:= -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer


# What OS are you compiling busybox for?  This allows you to include
# OS specific things, syscall overrides, etc.
TARGET_OS:=linux

# With a modern GNU make(1) (highly recommended, that's what all the
# developers use), all of the following configuration values can be
# overridden at the command line.  For example:
#   make CROSS=powerpc-linux- BB_SRC_DIR=$HOME/busybox PREFIX=/mnt/app

# If you want to add some simple compiler switches (like -march=i686),
# especially from the command line, use this instead of CFLAGS directly.
# For optimization overrides, it's better still to set OPTIMIZATION.
CFLAGS_EXTRA:=#-Werror
 
# If you want a static binary, turn this on.
DOSTATIC:=false

# Set the following to `true' to make a debuggable build.
# Leave this set to `false' for production use.
DODEBUG:=false

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
DODMALLOC:=false

# Electric-fence is another very useful malloc debugging library.
# Do not enable this for production builds...
DOEFENCE:=false

# If you want large file summit support, turn this on.
# This has no effect if you don't have a kernel with lfs
# support, and a system with libc-2.1.3 or later.
# Some of the programs that can benefit from lfs support
# are dd, gzip, mount, tar, and mkfs_minix.
# LFS allows you to use the above programs for files
# larger than 2GB!
DOLFS:=false

# If you have a "pristine" source directory, point BB_SRC_DIR to it.
# Experimental and incomplete; tell the mailing list
# <busybox@busybox.net> if you do or don't like it so far.
BB_SRC_DIR:=

# If you are running a cross compiler, you may want to set CROSS
# to something more interesting, like "arm-linux-".
CROSS:=
CC             := $(CROSS)gcc
AR             := $(CROSS)ar
AS             := $(CROSS)as
LD             := $(CROSS)ld
NM             := $(CROSS)nm
STRIP          := $(CROSS)strip
CPP            := $(CC) -E
MAKEFILES      := $(TOPDIR).config
export VERSION BUILDTIME TOPDIR HOSTCC HOSTCFLAGS CROSS CC AR AS LD NM STRIP CPP


# To compile vs uClibc, just use the compiler wrapper built by uClibc...
# Everything should compile and work as expected these days...
#CC:=/usr/i386-linux-uclibc/bin/i386-uclibc-gcc

# To compile vs some other alternative libc, you may need to use/adjust
# the following lines to meet your needs...
#
# If you are using Red Hat 6.x with the compatible RPMs (for developing under
# Red Hat 5.x and glibc 2.0) uncomment the following.  Be sure to read about
# using the compatible RPMs (compat-*) at http://www.redhat.com !
#LIBCDIR:=/usr/i386-glibc20-linux
#
# The following is used for libc5 (if you install altgcc and libc5-altdev
# on a Debian system).  
#LIBCDIR:=/usr/i486-linuxlibc1
#
# For other libraries, you are on your own...
#LDFLAGS+=-nostdlib
#LIBRARIES:=$(LIBCDIR)/lib/libc.a -lgcc
#CROSS_CFLAGS+=-nostdinc -I$(LIBCDIR)/include -I$(GCCINCDIR)
#GCCINCDIR:=$(shell gcc -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp")

WARNINGS:=-Wall -Wstrict-prototypes -Wshadow
CFLAGS:=-I$(TOPDIR)include
ARFLAGS:=-r

TARGET_ARCH:=${shell $(CC) -dumpmachine | sed -e s'/-.*//' \
		-e 's/i.86/i386/' \
		-e 's/sparc.*/sparc/' \
		-e 's/arm.*/arm/g' \
		-e 's/m68k.*/m68k/' \
		-e 's/ppc/powerpc/g' \
		-e 's/v850.*/v850/g' \
		-e 's/sh[234]/sh/' \
		-e 's/mips.*/mips/' \
		}

#--------------------------------------------------------
# Arch specific compiler optimization stuff should go here.
# Unless you want to override the defaults, do not set anything
# for OPTIMIZATION...

# use '-Os' optimization if available, else use -O2
OPTIMIZATION := ${shell if $(CC) -Os -S -o /dev/null -xc /dev/null \
	>/dev/null 2>&1; then echo "-Os"; else echo "-O2" ; fi}

# Some nice architecture specific optimizations
ifeq ($(strip $(TARGET_ARCH)),arm)
	OPTIMIZATION+=-fstrict-aliasing
endif
ifeq ($(strip $(TARGET_ARCH)),i386)
	OPTIMIZATION+=-march=i386
	OPTIMIZATION+=${shell if $(CC) -mpreferred-stack-boundary=2 -S -o /dev/null -xc \
		/dev/null >/dev/null 2>&1; then echo "-mpreferred-stack-boundary=2"; fi}
	OPTIMIZATION+=${shell if $(CC) -falign-functions=1 -falign-jumps=0 -falign-loops=0 \
		-S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo \
		"-falign-functions=1 -falign-jumps=0 -falign-loops=0"; else \
		if $(CC) -malign-functions=0 -malign-jumps=0 -S -o /dev/null -xc \
		/dev/null >/dev/null 2>&1; then echo "-malign-functions=0 -malign-jumps=0"; fi; fi}
endif
OPTIMIZATIONS:=$(OPTIMIZATION) -fomit-frame-pointer

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
    CFLAGS+=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
endif
ifeq ($(strip $(DODMALLOC)),true)
    # For testing mem leaks with dmalloc
    CFLAGS+=-DDMALLOC
    LIBRARIES:=-ldmalloc
    # Force debug=true, since this is useless when not debugging...
    DODEBUG:=true
else
    ifeq ($(strip $(DOEFENCE)),true)
	LIBRARIES:=-lefence
	# Force debug=true, since this is useless when not debugging...
	DODEBUG:=true
    endif
endif
ifeq ($(strip $(DODEBUG)),true)
    CFLAGS  +=$(WARNINGS) -g -D_GNU_SOURCE
    LDFLAGS +=-Wl,-warn-common
    STRIPCMD:=/bin/true -Not_stripping_since_we_are_debugging
else
    CFLAGS  += $(WARNINGS) $(OPTIMIZATIONS) -D_GNU_SOURCE
    LDFLAGS += -s -Wl,-warn-common
    STRIPCMD:=$(STRIP) --remove-section=.note --remove-section=.comment
endif
ifeq ($(strip $(DOSTATIC)),true)
    LDFLAGS += --static
endif

ifeq ($(strip $(PREFIX)),)
    PREFIX:=`pwd`/_install
endif

# Additional complications due to support for pristine source dir.
# Include files in the build directory should take precedence over
# the copy in BB_SRC_DIR, both during the compilation phase and the
# shell script that finds the list of object files.
# Work in progress by <ldoolitt@recycle.lbl.gov>.
#
ifneq ($(strip $(BB_SRC_DIR)),)
    VPATH:=$(BB_SRC_DIR)
endif

CFLAGS    += -DBB_VER='"$(VERSION)"'
CFLAGS    += -DBB_BT='"$(BUILDTIME)"'
OBJECTS:=$(APPLET_SOURCES:.c=.o) busybox.o usage.o applets.o
CFLAGS    += $(CROSS_CFLAGS)
ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif

# Put user-supplied flags at the end, where they
# have a chance of winning.
CFLAGS += $(CFLAGS_EXTRA)

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

ifdef _FASTDEP_ALL_SUB_DIRS
fastdep: dummy
	$(TOPDIR)scripts/mkdep $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -- $(wildcard *.[chS]) > .depend
ifdef ALL_SUB_DIRS
	$(MAKE) $(patsubst %,_sfdep_%,$(ALL_SUB_DIRS)) _FASTDEP_ALL_SUB_DIRS="$(ALL_SUB_DIRS)"
endif

$(patsubst %,_sfdep_%,$(_FASTDEP_ALL_SUB_DIRS)):
	$(MAKE) -C $(patsubst _sfdep_%,%,$@) fastdep
endif

.PHONY: dummy



.EXPORT_ALL_VARIABLES:

