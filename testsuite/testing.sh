# Simple test harness infrastructurei for BusyBox
#
# Copyright 2005 by Rob Landley
#
# License is GPLv2, see LICENSE in the busybox tarball for full license text.

# This file defines two functions, "testing" and "optionflag"

# The "testing" function must have the following environment variable set:
#    COMMAND = command to execute
#
# The following environment variables may be set to enable optional behavior
# in "testing":
#    VERBOSE - Print the diff -u of each failed test case.
#    DEBUG - Enable command tracing.
#    SKIP - do not perform this test (this is set by "optionflag")
#
# The "testing" function takes five arguments:
#	$1) Description to display when running command
#	$2) Command line arguments to command"
#	$3) Expected result (on stdout)"
#	$4) Data written to file "input"
#	$5) Data written to stdin
#
# The exit value of testing is the exit value of the command it ran.
#
# The environment variable "FAILCOUNT" contains a cumulative total of the
# number of failed tests.

# The "optional" function is used to skip certain tests, ala:
#   optionflag CONFIG_FEATURE_THINGY
#
# The "optional" function checks the environment variable "OPTIONFLAGS",
# which is either empty (in which case it always clears SKIP) or
# else contains a colon-separated list of features (in which case the function
# clears SKIP if the flag was found, or sets it to 1 if the flag was not found).

export FAILCOUNT=0
export SKIP=

# Helper functions

optional()
{
  option="$OPTIONFLAGS" | egrep "(^|:)$1(:|\$)"
  # Not set?
  if [[ -z "$1" || -z "$OPTIONFLAGS" || ${#option} -ne 0 ]]
  then
    SKIP=""
    return
  fi
  SKIP=1
}

# The testing function

testing ()
{
  if [ $# -ne 5 ]
  then
    echo "Test $1 has the wrong number of arguments" >&2
    exit
  fi

  if [ -n "$DEBUG" ] ; then
    set -x
  fi

  if [ -n "$SKIP" ]
  then
    echo "SKIPPED: $1"
    return 0
  fi

  echo -ne "$3" > expected
  echo -ne "$4" > input
  echo -n -e "$5" | eval "$COMMAND $2" > actual
  RETVAL=$?

  cmp expected actual > /dev/null
  if [ $? -ne 0 ]
  then
    FAILCOUNT=$[$FAILCOUNT+1]
    echo "FAIL: $1"
    if [ -n "$VERBOSE" ]
    then
      diff -u expected actual
    fi
  else
    echo "PASS: $1"
  fi
  rm -f input expected actual

  if [ -n "$DEBUG" ]
  then
    set +x
  fi

  return $RETVAL
}
