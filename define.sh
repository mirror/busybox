#!/bin/sh
# define.sh - creates an include file full of #defines
#
# SYNOPSIS
#
#	define.sh POSIX_ME_HARDER ...
#
# <GPL>

for i ; do
    case $1 in
    !!*!!)
	echo "// #define" `echo $1 | sed 's/^!!\(.*\)!!$/\1/'` 
	shift
	;;
    *)
	echo "#define $1"
	shift
	;;
    esac
done

# </GPL>
