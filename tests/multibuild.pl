#!/usr/bin/perl

# multibuild.pl
# Tests BusyBox-0.48 (at least) to see if each applet builds
# properly on its own.  The most likely problems this will
# flush out are those involving preprocessor instructions in
# utility.c.
#
# TODO: some time it might be nice to list absolute and 
# differential object sizes for each option...
#

$logfile = "multibuild.log";

# How to handle all the BB_FEATURE_FOO lines
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
while (<C>) {
	if ($in_trailer) {
		if (!$in_olympus) {
			s/^\/\/#/#/ if ($choice eq "all" && !/USE_DEVPS_PATCH/);
			s/^#/\/\/#/ if ($choice eq "none");
		}
		$in_olympus=1 if /End of Features List/;
		$trailer .= $_;
	} else {
		$in_trailer=1 if /End of Applications List/;
		if (/^\/*#define BB_([A-Z0-9_]*)/) {
			push @apps, $1;
		}
	}
}
close C;

# Do the real work ...
$failed_tests=0;
for $a (@apps) {
	# print "Testing build of applet $a ...\n";
	open (O, ">Config.h") || die;
	print O "#define BB_$a\n", $trailer;
	close O;
	system("echo -e '\n***\n$a\n***' >>$logfile");
	# With a fast computer and 1-second resolution on file timestamps, this
	# process pushes beyond the limits of what unix make can understand.
	# That's why need to weed out obsolete files before restarting make.
	$result{$a} = system("rm -f *.o applet_source_list; make $make_opt busybox >>$logfile 2>&1");
	$flag = $result{$a} ? "FAILED!!!" : "ok";
	printf("Applet %-20s: %s\n", $a, $flag);
	$total_tests++;
	$failed_tests++ if $flag eq "FAILED!!!";
	# pause long enough to let user stop us with a ^C
	select(undef, undef, undef, 0.03);
}

# Clean up our mess
system("mv -f Config.h.orig Config.h");

print "$total_tests applets tested, $failed_tests failures\n";
print "See $logfile for details.\n";

