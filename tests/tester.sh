#!/bin/bash
#
# tester.sh - reads testcases from file and tests busybox applets vs GNU
# counterparts
#
# This should be run from within the tests/ directory. Before you run it, you
# should compile up a busybox that has all applets and all features turned on.

# set up defaults (can be changed with cmd-line options)
BUSYBOX=../busybox
TESTCASES=testcases
LOGFILE=tester.log
CONFIG_OUT=bb.out
GNU_OUT=gnu.out
SETUP=""
CLEANUP=""
KEEPTMPFILES="no"
DEBUG=2


#while getopts 'p:t:l:b:g:s:c:kd:' opt
while getopts 'p:t:l:s:c:kd:' opt
do
	case $opt in
		p) BUSYBOX=$OPTARG; ;;
		t) TESTCASES=$OPTARG; ;;
		l) LOGFILE=$OPTARG; ;;
#		b) CONFIG_OUT=$OPTARG; ;;
#		g) GNU_OUT=$OPTARG; ;;
		s) SETUP=$OPTARG; ;;
		c) CLEANUP=$OPTARG; ;;
		k) KEEPTMPFILES="yes"; ;;
		d) DEBUG=$OPTARG; ;;
		*)
			echo "usage: $0 [-ptlbgsc]"
			echo "  -p PATH  path to busybox executable (default=$BUSYBOX)"
			echo "  -t FILE  run testcases in FILE (default=$TESTCASES)"
			echo "  -l FILE  log test results in FILE (default=$LOGFILE)"
#			echo "  -b FILE  store temporary busybox output in FILE"
#			echo "  -g FILE  store temporary GNU output in FILE"
			echo "  -s FILE  (setup) run commands in FILE before testcases"
			echo "  -c FILE  (cleanup) run commands in FILE after testcases"
			echo "  -k       keep temporary output files (don't delete them)"
			echo "  -d NUM   set level of debugging output"
			echo "           0 = no output"
			echo "           1 = output failures / whoops lines only"
			echo "           2 = (default) output setup / cleanup msgs and testcase lines"
			echo "           3+= other debug noise (internal stuff)"
			exit 1
			;;
	esac
done
#shift `expr $OPTIND - 1`


# maybe print some debug output
if [ $DEBUG -ge 3 ]
then
	echo "BUSYBOX=$BUSYBOX"
	echo "TESTCASES=$TESTCASES"
	echo "LOGFILE=$LOGFILE"
	echo "CONFIG_OUT=$CONFIG_OUT"
	echo "GNU_OUT=$GNU_OUT"
	echo "SETUP=$SETUP"
	echo "CLEANUP=$CLEANUP"
	echo "DEBUG=$DEBUG"
fi


# do sanity checks
if [ ! -e $BUSYBOX ]
then
	echo "Busybox executable: $BUSYBOX not found!"
	exit 1
fi

if [ ! -e $TESTCASES ]
then
	echo "Testcases file: $TESTCASES not found!"
	exit 1
fi


# do normal setup
[ -e $LOGFILE ] && rm $LOGFILE
unalias -a	# gets rid of aliases that might create different output


# do extra setup (if any)
if [ ! -z "$SETUP" ] 
then
	[ $DEBUG -ge 2 ] && echo "running setup commands in $SETUP"
	source $SETUP
fi


# go through each line in the testcase file
cat $TESTCASES | while read line
do
	#echo $line
	# only process non-blank lines and non-comment lines
	if [ "$line" ]
	then
		if [ `echo "$line" | cut -c1` != "#" ]
		then

			# test if the applet was compiled into busybox
			# (this only tests the applet at the beginning of the line)
			#applet=`echo $line | cut -d' ' -f1`
			applet=`echo $line | sed 's/\(^[^ ;]*\)[ ;].*/\1/'`
			$BUSYBOX 2>&1 | grep -qw $applet
			if [ $? -eq 1 ]
			then
				echo "WHOOPS: $applet not compiled into busybox" | tee -a $LOGFILE
			else

				# execute line using gnu / system programs
				[ $DEBUG -ge 2 ] && echo "testing: $line" | tee -a $LOGFILE
				sh -c "$line" > $GNU_OUT

				# change line to include "busybox" before every statement
				line="$BUSYBOX $line"
				# is this a bash-2-ism?
				# line=${line//;/; $BUSYBOX }
				# line=${line//|/| $BUSYBOX }
				# assume $BUSYBOX has no commas
				line=`echo "$line" | sed -e 's,;,; '$BUSYBOX, \
				                       -e 's, |, | '$BUSYBOX,`

				# execute line using busybox programs
				[ $DEBUG -ge 2 ] && echo "testing: $line" | tee -a $LOGFILE
				sh -c "$line" > $CONFIG_OUT

				# see if they match
				diff -q $CONFIG_OUT $GNU_OUT > /dev/null
				if [ $? -eq 1 ]
				then
					[ $DEBUG -ge 1 ] && echo "FAILED: $line" | tee -a $LOGFILE
					diff -u $CONFIG_OUT $GNU_OUT >> $LOGFILE 
				fi
			fi
		fi
	fi
done

[ $DEBUG -gt 0 ] && echo "Finished. Results are in $LOGFILE"


# do normal cleanup
[ "$KEEPTMPFILES" = "no" ] && rm -f $CONFIG_OUT $GNU_OUT


# do extra cleanup (if any)
if [ ! -z "$CLEANUP" ] 
then
	[ $DEBUG -ge 2 ] && echo "running cleanup commands in $CLEANUP"
	source $CLEANUP
fi
