
VERSION=0.29alpha1
BUILDTIME=$(shell date "+%Y%m%d-%H%M")

# Comment out the following to make a debuggable build
# Leave this off for production use.
DODEBUG=true

#This will choke on a non-debian system
ARCH=`uname -m | sed -e 's/i.86/i386/' | sed -e 's/sparc.*/sparc/'`


# -D_GNU_SOURCE is needed because environ is used in init.c
ifeq ($(DODEBUG),true)
    CFLAGS=-Wall -g -D_GNU_SOURCE
    STRIP=
    LDFLAGS=
else
    CFLAGS=-Wall -O2 -fomit-frame-pointer -fno-builtin -D_GNU_SOURCE
    LDFLAGS= -s
    STRIP= strip --remove-section=.note --remove-section=.comment busybox
endif

ifndef $(prefix)
    prefix=`pwd`
endif
BINDIR=$(prefix)

LIBRARIES=-lc
OBJECTS=$(shell ./busybox.sh)
CFLAGS+= -DBB_VER='"$(VERSION)"'
CFLAGS+= -DBB_BT='"$(BUILDTIME)"'

all: busybox links

busybox: $(OBJECTS)
	$(CC) $(LDFLAGS) -o busybox $(OBJECTS) $(LIBRARIES)
	$(STRIP)

links:
	- ./busybox.mkll | sort >busybox.links
	
clean:
	- rm -f busybox busybox.links *~ *.o core 

distclean: clean
	- rm -f busybox

force:

$(OBJECTS):  busybox.def.h internal.h Makefile

install:    busybox
	install.sh $(BINDIR)

