#!/bin/sh

export LC_ALL=POSIX
export LC_CTYPE=POSIX

prefix=$1
if [ "$prefix" = "" ]; then
    echo "No installation directory, aborting."
    exit 1;
fi
if [ "$2" = "--hardlinks" ]; then
    linkopts="-f"
else
    linkopts="-fs"
fi
h=`sort busybox.links | uniq`


rm -f $prefix/bin/busybox || exit 1
mkdir -p $prefix/bin || exit 1
install -m 755 busybox $prefix/bin/busybox || exit 1

for i in $h ; do
	appdir=`dirname $i`
	mkdir -p $prefix/$appdir || exit 1
	if [ "$2" = "--hardlinks" ]; then
	    bb_path="$prefix/bin/busybox"
	else
	    case "$appdir" in
		/)
		    bb_path="bin/busybox"
		;;
		/bin)
		    bb_path="busybox"
		;;
		/sbin)
		    bb_path="../bin/busybox"
		;;
		/usr/bin|/usr/sbin)
		    bb_path="../../bin/busybox"
		;;
		*)
		echo "Unknown installation directory: $appdir"
		exit 1
		;;
	    esac
	fi
	echo "  $prefix$i -> $bb_path"
	ln $linkopts $bb_path $prefix$i || exit 1
done

exit 0
