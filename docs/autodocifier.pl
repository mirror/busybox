#!/usr/bin/perl -w
#
# autodocufier.pl - extracts usage messages from busybox usage.c and
# pretty-prints them to stdout.

use strict;

my $line;
my $applet;
my $count;
my $full_usage;

open(USAGE, 'usage.h') or die "usage.h: $!";

while (defined($line = <USAGE>)) {
	$count=0;
	if ($line =~ /^#define (\w+)_trivial_usage/) {
		# grab the applet name
		$applet = $1;
		print "\n$applet:\n";

		while (defined($line = <USAGE>)) {
			if ( $count==0 ) {
			    $count++;
			    print "\t$applet ";
			} else { print "\t"; }
			$full_usage = $applet . "_full_usage";
			last if ( $line =~ /$full_usage/ );
			# Skip preprocessor stuff
			next if $line =~ /^\s*#/;
			# Strip the continuation char
			$line =~ s/\\$//;
			# strip quotes off
			$line =~ s/^\s*"//;
			$line =~ s/"\s*$//;
			# substitute escape sequences
			# (there's probably a better way to do this...)
			$line =~ s/\\t/	/g;
			$line =~ s/\\n//g;
			# fix up preprocessor macros
			$line =~ s/USAGE_\w+\([\s]*?(".*?").*?\)/$1/sg;
			# Strip any empty quotes out
			$line =~ s/"[\s]*"//sg;
			# strip line end quotes, again
			$line =~ s/^\s*"//;
			$line =~ s/"\s*$//;

			# Finally, print it
			print "$line\n";
		}
		printf("\n");
		while (defined($line = <USAGE>)) {
			if ( $count==0 ) {
			    $count++;
			    print "\t$applet ";
			} else { print "\t"; }
			# we're done if we hit a line lacking a '\' at the end
			#last if ! $line !~ /\\$/;
			if ( $line !~ /\\$/ ) {
			    #print "Got one at $line\n";
			    last;
			}
			# Skip preprocessor stuff
			next if $line =~ /^\s*#/;
			# Strip the continuation char
			$line =~ s/\\$//;
			# strip quotes off
			$line =~ s/^\s*"//;
			$line =~ s/"\s*$//;
			# substitute escape sequences
			# (there's probably a better way to do this...)
			$line =~ s/\\t/	/g;
			$line =~ s/\\n//g;
			# Automagically #define all preprocessor lines
			#$line =~ s/USAGE_\w+\([\s]*?(".*?")\s,\s".*"\s\)/$1/sg;
			$line =~ s/USAGE_\w+\(\s*?(".*").*\)/$1/sg;
			# Strip any empty quotes out
			$line =~ s/"[\s]*"//sg;
			# strip line end quotes, again
			$line =~ s/^\s*"//;
			$line =~ s/"\s*$//;

			# Finally, print it
			print "$line\n";
		}
		printf("\n\n");
	}
}
