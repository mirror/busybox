#!/usr/bin/perl
#
# multifeat.pl
#
# Turns on all applets, then tests turning on one feature at a time through
# iterative compilations. Tests if any features depend on each other in any
# weird ways or such-like problems.
#
# Hacked by Mark Whitley, but based *heavily* on multibuild.pl which was
# written by Larry Doolittle.

$logfile = "multifeat.log";

# How to handle all the BB_APPLET lines
# (most thorough testing occurs when you call it with the -all switch)
if ($ARGV[0] eq "-all" ) { shift(@ARGV); $choice="all"; }
if ($ARGV[0] eq "-none") { shift(@ARGV); $choice="none"; }
# neither means, leave that part of Config.h alone

# Support building from pristine source
$make_opt = "-f $ARGV[0]/Makefile BB_SRC_DIR=$ARGV[0]" if ($ARGV[0] ne "");

# Move the config file to a safe place
-e "Config.h.orig" || 0==system("mv -f Config.h Config.h.orig") || die;

# Clear previous log file, if any
unlink($logfile);

# Parse the config file
open(C,"<Config.h.orig") || die;
$in_applist=1;
$in_features=0;
$in_olympus=0;
while (<C>) {
	if ($in_applist) {
		s/^\/\/#/#/ if ($choice eq "all");
		s/^#/\/\/#/ if ($choice eq "none");
		$header .= $_;
		if (/End of Applications List/) {
			$in_applist=0;
			$in_features=1
		}
	}
	elsif ($in_features) {
		if (/^\/*#define BB_FEATURE_([A-Z0-9_]*)/) {
			push @features, $1;
		}
		if (/End of Features List/) {
			$in_features=0;
			$in_olympus=1
		}
	} elsif ($in_olympus) {
		$trailer .= $_;
	}
}
close C;

# Do the real work ...
$failed_tests=0;
for $f (@features) {
	# print "Testing build with feature $f ...\n";
	open (O, ">Config.h") || die;
	print O $header, "#define BB_FEATURE_$f\n", $trailer;
	close O;
	system("echo -e '\n***\n$f\n***' >>$logfile");
	# With a fast computer and 1-second resolution on file timestamps, this
	# process pushes beyond the limits of what unix make can understand.
	# That's why need to weed out obsolete files before restarting make.
	$result{$f} = system("rm -f *.o applet_source_list; make $make_opt busybox >>$logfile 2>&1");
	$flag = $result{$f} ? "FAILED!!!" : "ok";
	printf("Feature %-20s: %s\n", $f, $flag);
	$total_tests++;
	$failed_tests++ if $flag eq "FAILED!!!";
	# pause long enough to let user stop us with a ^C
	select(undef, undef, undef, 0.03);
}

# Clean up our mess
system("mv -f Config.h.orig Config.h");

print "$total_tests applets tested, $failed_tests failures\n";
print "See $logfile for details.\n";

