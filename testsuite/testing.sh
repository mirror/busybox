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

verbose=0
debug=0
force=0
for x in "$@" ; do
	case "$x" in
	-v|--verbose)  verbose=1; shift;;
	-d|--debug)    debug=1; shift;;
	-f|--force)    force=1; shift;;
	--)            break;;
	-*)            echo "Unknown option '$x'"; exit 1;;
	*)             break;;
	esac
done

if [ -n "$VERBOSE" ] ; then
	verbose=1
fi
if [ -n "$DEBUG" ] ; then
	debug=1
fi

export FAILCOUNT=0

# Helper functions

config_is_set ()
{
  local uc_what=$(echo ${1?} | tr a-z A-Z)
  grep -q "^[ 	]*CONFIG_${uc_what}" ${bindir:-..}/.config || \
    grep -q "^[ 	]*BB_CONFIG_${uc_what}" ${bindir:-..}/.config
  return $?
}

# The testing function

testing ()
{
  if [ $# -ne 5 ]
  then
    echo "Test $1 has the wrong number of arguments" >&2
    exit
  fi

  if [ $debug -eq 1 ] ; then
    set -x
  fi

  if [ -n "$_BB_CONFIG_DEP" ] && [ ${force} -eq 0 ]
  then
    if ! config_is_set "$_BB_CONFIG_DEP"
    then
      echo "SKIPPED: $1"
      return 0
    fi
  fi

  echo -ne "$3" > expected
  echo -ne "$4" > input
  echo -n -e "$5" | eval "$COMMAND $2" > actual
  RETVAL=$?

  cmp expected actual > /dev/null
  if [ $? -ne 0 ]
  then
	((FAILCOUNT++))
	echo "FAIL: $1"
	if [ $verbose -eq 1 ]
	then
		diff -u expected actual
	fi
  else
	echo "PASS: $1"
  fi
  rm -f input expected actual

  if [ $debug -eq 1 ] ; then
    set +x
  fi

  return $RETVAL
}
