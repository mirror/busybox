#!/bin/sh

set -e

if [ "$1" == "" ]; then
    echo "No installation directory, aborting."
    exit 1;
fi

h=`sort busybox.links | uniq`

for i in $h ; do
	mypath=`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\1/g' `;
	myapp=`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\2/g' `;
	echo "  $1$mypath$myapp -> /bin/busybox"
	mkdir -p $1$mypath
	(cd $1$mypath && rm -f $1$mypath$myapp && ln -s /bin/busybox $1$mypath$myapp )
done
rm -f $1/bin/busybox
install -m 755 busybox $1/bin/busybox

exit 0
