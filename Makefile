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
VERSION   := 0.44
BUILDTIME := $(shell TZ=UTC date --utc "+%Y.%m.%d-%H:%M%z")
export VERSION

# Set the following to `true' to make a debuggable build.
# Leave this set to `false' for production use.
# eg: `make DODEBUG=true tests'
DODEBUG = true

# If you want a static binary, turn this on.
DOSTATIC = false

# To compile vs an alternative libc, you may need to use/adjust
# the following lines to meet your needs.  This is how I did it...
#CFLAGS+=-nostdinc -I/home/andersen/CVS/uC-libc/include -I/usr/include/linux
#LDFLAGS+=-nostdlib -L/home/andersen/CVS/libc.a


CC = gcc

# use '-Os' optimization if available, else use -O2
OPTIMIZATION = $(shell if $(CC) -Os -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
    then echo "-Os"; else echo "-O2" ; fi)


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
	#
	#use '-ffunction-sections -fdata-sections' and '--gc-sections' if they work
	#to try and strip out any unused junk.  Doesn't do much for me, but you may
	#want to give it a shot...
	#
	#ifeq ($(shell $(CC) -ffunction-sections -fdata-sections -S \
	#	-o /dev/null -xc /dev/null 2>/dev/null && $(LD) --gc-sections -v >/dev/null && echo 1),1)
	#	CFLAGS += -ffunction-sections -fdata-sections
	#	LDFLAGS += --gc-sections
	#endif
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

all: busybox busybox.links doc

doc: docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/BusyBox.txt: docs/busybox.pod
	@echo
	@echo BusyBox Documentation
	@echo
	pod2text docs/busybox.pod > docs/BusyBox.txt

docs/BusyBox.1: docs/busybox.pod
	pod2man --center=BusyBox --release="version $(VERSION)" docs/busybox.pod > docs/BusyBox.1

docs/BusyBox.html: docs/busybox.pod
	pod2html docs/busybox.pod > docs/busybox.lineo.com/BusyBox.html
	ln -s busybox.lineo.com/BusyBox.html docs/BusyBox.html
	- rm -f pod2html*

clean:

busybox: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBRARIES)
	$(STRIP)

busybox.links: busybox.def.h
	- ./busybox.mkll | sort >$@

regexp.o nfsmount.o cmdedit.o: %.o: %.h
$(OBJECTS): %.o: busybox.def.h internal.h  %.c Makefile

test tests:
	cd tests && $(MAKE) all

clean:
	- rm -f busybox.links *~ *.o core
	- rm -rf _install
	- cd tests && $(MAKE) clean
	- rm -f docs/BusyBox.html docs/busybox.lineo.com/BusyBox.html \
		docs/BusyBox.1 docs/BusyBox.txt pod2html*

distclean: clean
	- rm -f busybox
	- cd tests && $(MAKE) distclean

install: busybox busybox.links
	./install.sh $(PREFIX)

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
	tar -cvzf busybox-$(VERSION).tar.gz busybox-$(VERSION)/;
