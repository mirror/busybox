# Simple test harness infrastructurei for BusyBox
#
# Copyright 2005 by Rob Landley
#
# License is GPLv2, see LICENSE in the busybox tarball for full license text.

# The "testing" function uses one environment variable:
#	COMMAND = command to execute
#
# The function takes five arguments:
#	$1) Description to display when running command
#	$2) Command line arguments to command"
#	$3) Expected result (on stdout)"
#	$4) Data written to file "input"
#	$5) Data written to stdin
#
# The exit value of testing is the exit value of the command it ran.
#
# The environment variable "FAILCOUNT" contains a cumulative total of the
# 

# The command line parsing is ugly and should be improved.

if [ "$1" == "-v" ]
then
  verbose=1
fi

export FAILCOUNT=0

# The testing function

function testing()
{
  if [ $# -ne 5 ]
  then
    echo "Test $1 has the wrong number of arguments" >&2
    exit
  fi

  f=$FAILCOUNT
  echo -ne "$3" > expected
  echo -ne "$4" > input
  echo -n -e "$5" | eval "$COMMAND $2" > actual
  RETVAL=$?

  cmp expected actual > /dev/null
  if [ $? -ne 0 ]
  then
	FAILCOUNT=$[$FAILCOUNT+1]
	echo "FAIL: $1"
	if [ $verbose ]
	then
		diff -u expected actual
	fi
  else
	echo "PASS: $1"
  fi
  rm -f input expected actual

  return $RETVAL
}
