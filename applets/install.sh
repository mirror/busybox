#!/bin/sh

set -e

if [ "$1" == "" ]; then
    echo "No installation directory, aborting."
    exit 1;
fi

# can't just use cat, rmdir is not unique
#h=`cat busybox.links`
h=`sort busybox.links | uniq`

mkdir -p $1/bin
for i in $h ; do
	[ ${verbose} ] && echo "  making link to $i"
	mkdir -p $1/`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\1/g' `
	ln -s busybox $1/bin/`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\2/g' `
done
rm -f $1/bin/busybox
install -m 755 busybox $1/bin/busybox

