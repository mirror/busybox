#!/usr/bin/perl

# multibuild.pl
# Tests BusyBox-0.48 (at least) to see if each applet builds
# properly on its own.  The most likely problems this will
# flush out are those involving preprocessor instructions in
# utility.c.

$logfile = "multibuild.log";

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
for $a (@apps) {
	# print "Testing build of applet $a ...\n";
	open (O, ">Config.h") || die;
	print O "#define BB_$a\n", $trailer;
	close O;
	system("echo -e '\n***\n$a\n***' >>$logfile");
	# todo: figure out why the "rm -f *.o" is needed
	$result{$a} = system("rm -f *.o; make $make_opt busybox >>$logfile 2>&1");
	$flag = $result{$a} ? "FAIL" : "OK";
	print "Applet $a: $flag\n";
}

# Clean up our mess
system("mv -f Config.h.orig Config.h");

print "See $logfile for details.\n";

