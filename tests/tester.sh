#!/bin/bash
#
# tester.sh - reads testcases from file and tests busybox applets vs GNU
# counterparts
#

# set up defaults (can be changed with cmd-line options)
BUSYBOX=../busybox
TESTCASES=testcases
LOGFILE=tester.log
BB_OUT=bb.out
GNU_OUT=gnu.out
SETUP=""
CLEANUP=""

# internal-use vars
fail_only=0


while getopts 'p:t:l:b:g:s:c:f' opt
do
	case $opt in
		p) BUSYBOX=$OPTARG; ;;
		t) TESTCASES=$OPTARG; ;;
		l) LOGFILE=$OPTARG; ;;
		b) BB_OUT=$OPTARG; ;;
		g) GNU_OUT=$OPTARG; ;;
		s) SETUP=$OPTARG; ;;
		c) CLEANUP=$OPTARG; ;;
		f) fail_only=1; ;;
		*)
			echo "usage: $0 [-ptlbgsc]"
			echo "  -p PATH  path to busybox executable"
			echo "  -t FILE  run testcases in FILE"
			echo "  -l FILE  log test results in FILE"
			echo "  -b FILE  store temporary busybox output in FILE"
			echo "  -g FILE  store temporary GNU output in FILE"
			echo "  -s FILE  (setup) run commands in FILE before testcases"
			echo "  -c FILE  (cleanup) run commands in FILE after testcases"
			echo "  -f       display only testcases that fail"
			exit 1
			;;
	esac
done
#shift `expr $OPTIND - 1`


# do normal setup
[ -e $LOGFILE ] && rm $LOGFILE
unalias -a	# gets rid of aliases that might create different output

# do extra setup (if any)
if [ ! -z $SETUP ] 
then
	echo "running setup commands in $SETUP"
	sh $SETUP
	# XXX: Would 'eval' or 'source' work better instead of 'sh'?
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
			[ $fail_only -eq 0 ] && echo "testing: $line" | tee -a $LOGFILE

			# test if the applet was compiled into busybox
			applet=`echo $line | cut -d' ' -f1`
			$BUSYBOX 2>&1 | grep -qw $applet
			if [ $? -eq 1 ]
			then
				echo "WHOOPS: $applet not compiled into busybox" | tee -a $LOGFILE
			else
				$BUSYBOX $line > $BB_OUT
				$line > $GNU_OUT
				diff -q $BB_OUT $GNU_OUT > /dev/null
				if [ $? -eq 1 ]
				then
					echo "FAILED: $line" | tee -a $LOGFILE
					diff -u $BB_OUT $GNU_OUT >> $LOGFILE 
				fi
			fi
		fi
	fi
done

echo "Finished. Results are in $LOGFILE"


# do normal cleanup
rm -f $BB_OUT $GNU_OUT

# do extra cleanup (if any)
if [ ! -z $CLEANUP ] 
then
	echo "running cleanup commands in $CLEANUP"
	sh $CLEANUP
	# XXX: Would 'eval' or 'source' work better instead of 'sh'?
fi
