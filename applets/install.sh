#!/bin/sh

if [ "$1" == "" ]; then
    echo "No installation directory.  aborting."
    exit 1;
fi

h=`cat busybox.links`

mkdir -p $1/bin
for i in $h ; do
	mkdir -p $1/`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\1/g' `
	(cd $1/bin ; ln -s busybox `echo $i | sed -e 's/\(^.*\/\)\(.*\)/\2/g' ` )
done
rm -f $1/bin/busybox
install -m 755 busybox $1/bin/busybox

