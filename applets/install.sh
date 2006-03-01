#!/bin/sh

export LC_ALL=POSIX
export LC_CTYPE=POSIX

prefix=${1}
if [ -z "$prefix" ]; then
    echo "usage: applets/install.sh DESTINATION [--symlinks/--hardlinks]"
    exit 1;
fi
h=`sort busybox.links | uniq`
case "$2" in
    --hardlinks) linkopts="-f";;
    --symlinks)  linkopts="-fs";;
    "")          h="";;
    *)           echo "Unknown install option: $2"; exit 1;;
esac

if [ "$DO_INSTALL_LIBS" != "n" ]; then
	# get the target dir for the libs
	# This is an incomplete/incorrect list for now
	case $(uname -m) in
	x86_64|ppc64*|sparc64*|ia64*|hppa*64*|s390x*) libdir=/lib64 ;;
	*) libdir=/lib ;;
	esac

	mkdir -p $prefix/$libdir || exit 1
	for i in $DO_INSTALL_LIBS; do
		rm -f $prefix/$libdir/$i || exit 1
		if [ -f $i ]; then
			cp -a $i $prefix/$libdir/ || exit 1
			chmod 0644 $prefix/$libdir/$i || exit 1
		fi
	done
fi
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
