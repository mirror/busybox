#!/bin/sh
#
# To compile BusyBox without touching the original sources
# (as might be interesting for multi-target builds), create 
# an empty directory, cd into it, and run this program by
# giving its explicit path (kind of like how you would run
# configure, if BusyBox had one).  Then you should be ready
# to "make".  Files in the build tree, in particular Config.h,
# will override those in the pristine source tree.
#
# If you use a ? in your path name, you lose, see sed command below.

export LC_ALL=POSIX
export LC_CTYPE=POSIX

DIR=${0%%/pristine_setup.sh}
if [ ! -d $DIR ]; then
  echo "unexpected problem: $DIR is not a directory.  Aborting pristine setup"
  exit
fi

echo " "

if [ -e ./Config.h ]; then
    echo "./Config.h already exists: not overwriting"
    exit
fi

if [ -e ./Makefile ]; then
    echo "./Makefile already exists: not overwriting"
fi

sed -e "s?BB_SRC_DIR =.*?BB_SRC_DIR = $DIR?" <$DIR/Makefile >Makefile || exit
cp $DIR/Config.h Config.h || exit
#mkdir -p pwd_grp

echo " "
echo "You may now type 'make' to build busybox in this directory"
echo "($PWD) using the pristine sources in $DIR"
echo " "

