#!/bin/sh

set -e

if [ "$1" == "" ]; then
    echo "No installation directory, aborting."
    exit 1;
fi
rm -rf $1

h=`sort busybox.links | uniq`

for i in $h ; do
	echo "working on $i now"
	mypath=`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\1/g' `;
	myapp=`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\2/g' `;
	echo "mkdir -p $1$mypath"
	echo "(cd $1$mypath ; ln -s /bin/busybox $1$mypath$myapp )"
	mkdir -p $1$mypath
	(cd $1$mypath ; ln -s /bin/busybox $1$mypath$myapp )
done
rm -f $1/bin/busybox
install -m 755 busybox $1/bin/busybox


