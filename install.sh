#!/bin/sh

set -e

if [ "$1" == "" ]; then
    echo "No installation directory, aborting."
    exit 1;
fi

h=`sort busybox.links | uniq`

for i in $h ; do
	echo "  $1$i -> /bin/busybox"
	mkdir -p $1/`echo $i | sed -e 's/\/[^\/]*$//' `
	ln -fs /bin/busybox $1$i
done
rm -f $1/bin/busybox
install -m 755 busybox $1/bin/busybox

exit 0
