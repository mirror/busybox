
VERSION=0.29alpha1
BUILDTIME=$(shell date "+%Y%m%d-%H%M")

#This will choke on a non-debian system
ARCH=`uname -m | sed -e 's/i.86/i386/' | sed -e 's/sparc.*/sparc/'`


STRIP= strip --remove-section=.note --remove-section=.comment busybox
LDFLAGS= -s

# -D_GNU_SOURCE is needed because environ is used in init.c
CFLAGS=-Wall -O2 -fomit-frame-pointer -fno-builtin -D_GNU_SOURCE
# For debugging only
#CFLAGS=-Wall -g -D_GNU_SOURCE
LIBRARIES=-lc
OBJECTS=$(shell ./busybox.sh) utility.o

CFLAGS+= -DBB_VER='"$(VERSION)"'
CFLAGS+= -DBB_BT='"$(BUILDTIME)"'

#all: busybox links
all: busybox

busybox: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o busybox $(OBJECTS) $(LIBRARIES)
	#$(STRIP)

links:
	- ./busybox.mkll | sort >busybox.links
	
clean:
	- rm -f busybox busybox.links *~ *.o 

distclean: clean
	- rm -f busybox

force:

$(OBJECTS):  busybox.def.h Makefile
