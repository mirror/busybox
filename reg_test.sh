#!/bin/sh



rm -rf testdir
./busybox cp tar.c testdir

if ! eval diff -u tar.c testdir ; then
    echo "Bummer.  File copy failed."
    exit 0
else
    echo "Cool.  'cp tar.c testdir' is ok."
fi

rm -rf testdir
mkdir -p testdir/foo
./busybox cp tar.c testdir/foo

if ! eval diff -u tar.c testdir/foo/tar.c ; then
    echo "Bummer.  File copy to a directory failed."
    exit 0
else
    echo "Cool.  'cp tar.c testdir/foo' is ok."
fi


rm -rf testdir
mkdir -p testdir/foo
./busybox cp tar.c testdir/foo/

if ! eval diff -u tar.c testdir/foo/tar.c ; then
    echo "Bummer.  File copy to a directory w/ a '/' failed."
    exit 0
else
    echo "Cool.  'cp tar.c testdir/foo/' is ok."
fi


rm -rf testdir X11
cp -a /etc/X11 .
./busybox cp -a X11 testdir

if ! eval diff -ur X11 testdir ; then
    echo "Bummer.  Local dir copy failed."
    exit 0
else
    echo "Cool.  'cp -a X11 testdir' is ok."
fi

rm -rf testdir X11
cp -a /etc/X11 .
./busybox cp -a X11 testdir/

if ! eval diff -ur X11 testdir ; then
    echo "Bummer.  Local dir copy w/ a '/' failed."
    exit 0
else
    echo "Cool.  'cp -a X11 testdir/' is ok."
fi

rm -rf testdir X11
cp -a /etc/X11 .
./busybox cp -a X11/ testdir

if ! eval diff -ur X11 testdir ; then
    echo "Bummer.  Local dir copy w/ a src '/' failed."
    exit 0
else
    echo "Cool.  'cp -a X11/ testdir' is ok."
fi

rm -rf testdir X11
cp -a /etc/X11 .
./busybox cp -a X11/ testdir/

if ! eval diff -ur X11 testdir ; then
    echo "Bummer.  Local dir copy w/ 2x '/'s failed."
    exit 0
else
    echo "Cool.  'cp -a X11/ testdir/' is ok."
fi

rm -rf testdir X11
./busybox cp -a /etc/X11 testdir
if ! eval diff -ur /etc/X11 testdir ; then
    echo "Bummer.  Remote dir copy failed."
    exit 0
else
    echo "Cool.  'cp -a /etc/X11 testdir' is ok."
fi


rm -rf testdir X11
mkdir -p testdir/foo

./busybox cp -a /etc/X11 testdir/foo
if ! eval diff -ur /etc/X11 testdir/foo ; then
    echo "Bummer.  Remote dir copy to a directory failed."
    exit 0
else
    echo "Cool.  'cp -a /etc/X11 testdir/foo' is ok."
fi


rm -rf testdir X11
mkdir -p testdir/foo

./busybox cp -a /etc/X11 testdir/foo/
if ! eval diff -ur /etc/X11 testdir/foo ; then
    echo "Bummer.  Remote dir copy to a directory w/ a '/' failed."
    exit 0
else
    echo "Cool.  'cp -a /etc/X11 testdir/foo/' is ok."
fi

rm -rf testdir


rm -rf foo bar
mkdir foo
mkdir bar

if ! eval ./busybox cp README foo ; then
    echo "Bummer.  cp README foo failed."
    exit 0
else
    echo "Cool.  'cp README foo' is ok."
fi

if ! eval ./busybox cp foo/README bar ; then
    echo "Bummer.  cp foo/README bar failed."
    exit 0
else
    echo "Cool.  'cp foo/README bar' is ok."
fi

rm -f bar/README
ENVVAR1=foo
ENVVAR2=bar
if ! eval ./busybox cp $ENVVAR1/README $ENVVAR2 ; then
    echo "Bummer.  cp foo/README bar failed."
    exit 0
else
    echo "Cool.  'cp \$ENVVAR1/README \$ENVVAR2' is ok."
fi

rm -rf foo bar

