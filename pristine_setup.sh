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


# if you use a ? in your path name, you lose.
DIR=`basedir ${0%%/pristine_setup}`
if [ ! -d $DIR ]; then
  echo "unexpected problem: $DIR is not a directory.  Aborting pristine setup"
  exit
fi

echo " "

if [ -e ./Config.h ]; then
  echo "./Config.h already exists: not overwriting"
else
    cp $DIR/Config.h Config.h
fi

if [ -e ./Makefile ]; then
    echo "./Makefile already exists: not overwriting"
else
    sed -e "s?BB_SRC_DIR =?BB_SRC_DIR = $DIR?" <$DIR/Makefile >Makefile || exit
fi


echo " "
echo "You may now type 'make' to build busybox in this directory"
echo "($PWD) using the pristine sources in $DIR"
echo " "

