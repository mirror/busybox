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
VERSION   := 0.43
BUILDTIME := $(shell TZ=GMT date "+%Y%m%d-%H%M")

# Set the following to `true' to make a debuggable build.
# Leave this set to `false' for production use.
# eg: `make DODEBUG=true tests'
DODEBUG = true

# If you want a static binary, turn this on.  I can't think
# of many situations where anybody would ever want it static, 
# but...
DOSTATIC = false

# This will choke on a non-debian system
ARCH =`uname -m | sed -e 's/i.86/i386/' | sed -e 's/sparc.*/sparc/'`

CC = gcc

GCCMAJVERSION = $(shell $(CC) --version | sed -n "s/^[^0-9]*\([0-9]\)\.\([0-9].*\)[\.].*/\1/p")
GCCMINVERSION = $(shell $(CC) --version | sed -n "s/^[^0-9]*\([0-9]\)\.\([0-9].*\)[\.].*/\2/p")


GCCSUPPORTSOPTSIZE = $(shell			\
if ( test $(GCCMAJVERSION) -eq 2 ) ; then	\
    if ( test $(GCCMINVERSION) -ge 66 ) ; then	\
	echo "true";				\
    else					\
	echo "false";				\
    fi;						\
else						\
    if ( test $(GCCMAJVERSION) -gt 2 ) ; then	\
	echo "true";				\
    else					\
	echo "false";				\
    fi;						\
fi; )


ifeq ($(GCCSUPPORTSOPTSIZE), true)
    OPTIMIZATION = -Os
else
    OPTIMIZATION = -O2
endif

# Allow alternative stripping tools to be used...
ifndef $(STRIPTOOL)
    STRIPTOOL = strip
endif


# -D_GNU_SOURCE is needed because environ is used in init.c
ifeq ($(DODEBUG),true)
    CFLAGS += -Wall -g -D_GNU_SOURCE
    LDFLAGS =
    STRIP   =
else
    CFLAGS  += -Wall $(OPTIMIZATION) -fomit-frame-pointer -fno-builtin -D_GNU_SOURCE
    LDFLAGS  = -s
    STRIP    = $(STRIPTOOL) --remove-section=.note --remove-section=.comment $(PROG)
    #Only staticly link when _not_ debugging 
    ifeq ($(DOSTATIC),true)
	LDFLAGS += --static
    endif
endif

ifndef $(PREFIX)
    PREFIX = `pwd`/_install
endif

LIBRARIES =
OBJECTS   = $(shell ./busybox.sh) busybox.o messages.o utility.o
CFLAGS    += -DBB_VER='"$(VERSION)"'
CFLAGS    += -DBB_BT='"$(BUILDTIME)"'
ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif

all: busybox busybox.links
.PHONY: all

busybox: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBRARIES)
	$(STRIP)

busybox.links: busybox.def.h
	- ./busybox.mkll | sort >$@

regexp.o nfsmount.o: %.o: %.h
$(OBJECTS): %.o: busybox.def.h internal.h  %.c

.PHONY: test tests
test tests:
	cd tests && $(MAKE) all

.PHONY: clean
clean:
	- rm -f busybox.links *~ *.o core
	- rm -rf _install
	- cd tests && $(MAKE) clean

.PHONY: distclean
distclean: clean
	- rm -f busybox
	- cd tests && $(MAKE) distclean

.PHONY: install
install: busybox busybox.links
	./install.sh $(PREFIX)

.PHONY: dist release
dist release: distclean
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
	tar -cvzf busybox-$(VERSION).tar.gz busybox-$(VERSION)/;
