#!/usr/bin/perl -w

use strict;
use Getopt::Long;

# collect lines continued with a '\' into an array 
sub continuation {
	my $fh = shift;
	my @line;

	while (<$fh>) {
		my $s = $_;
		$s =~ s/\\\s*$//;
		$s =~ s/#.*$//;
		push @line, $s;
		last unless (/\\\s*$/);
	}
	return @line;
}

# regex && eval away unwanted strings from documentation
sub beautify {
	my $text = shift;
	$text =~ s/USAGE_\w+\([\s]*?(".*?").*?\)/$1/sg;
	$text =~ s/"[\s]*"//sg;
	my @line = split("\n", $text);
	$text = join('',
		map { eval }
		map { qq[ sprintf(qq#$_#) ] }
		map { 
			s/^\s*//;
			s/"//g;
			s/% /%% /g;
			$_
		}
		@line
	);
	return $text;
}

# generate POD for an applet
sub pod_for_usage {
	my $name  = shift;
	my $usage = shift;

	my $trivial = $usage->{trivial};
	$trivial =~s/(?<!\w)(-\w+)/B<$1>/sxg;

	my $full = 
		join("\n"
		map { $_ !~ /^\s/ && s/(?<!\w)(-\w+)/B<$1>/g; $_ }
		split("\n", $usage->{full}));

	return
		"-------------------------------\n".
		"\n".
		"=item $name".
		"\n\n".
		"$name $trivial".
		"\n\n".
		$full.
		"\n\n"
	;
}

# generate SGML for an applet
sub sgml_for_usage {
	my $name  = shift;
	my $usage = shift;
	return
	"FIXME";
}

# the keys are applet names, and 
# the values will contain hashrefs of the form:
#
# {
#     trivial => "...",
#     full    => "...",
# }
my %docs;

# get command-line options
my %opt;

GetOptions(
	\%opt,
	"help|h",
	"sgml|s",
	"pod|p",
	"verbose|v",
);

if (defined $opt{help}) {
	print
		"$0 [OPTION]... [FILE]...\n",
		"\t--help\n",
		"\t--sgml\n",
		"\t--pod\n",
		"\t--verbose\n",
	;
	exit 1;
}

# collect documenation into %docs
foreach (@ARGV) {
	open(USAGE, $_) || die("$0: $!");
	my $fh = *USAGE;
	my ($applet, $type, @line);
	while (<$fh>) {

		if (/^#define (\w+)_(\w+)_usage/) {
			$applet = $1;
			$type   = $2;
			@line   = continuation($fh);
			my $doc = $docs{$applet} ||= { };

			my $text      = join("\n", @line);
			$doc->{$type} = beautify($text);
		}

	}
}

#use Data::Dumper;
#print Data::Dumper->Dump([\%docs], [qw(docs)]);

foreach my $name (sort keys %docs) {
	print pod_for_usage($name, $docs{$name});
}

exit 0;

__END__

=head1 NAME

autodocifier.pl - generate docs for busybox based on usage.h

=head1 SYNOPSIS

autodocifier.pl usage.h > something

=head1 DESCRIPTION

The purpose of this script is to automagically generate documentation
for busybox using its usage.h as the original source for content.
Currently, the same content has to be duplicated in 3 places in
slightly different formats -- F<usage.h>, F<docs/busybox.pod>, and
F<docs/busybox.sgml>.  This is tedious, so Perl has come to the rescue.

This script was based on a script by Erik Andersen (andersen@lineo.com).

=head1 OPTIONS

these control my behaviour

=over 8

=item --help

This displays the help message.

=back

=head1 FILES

files that I manipulate

=head1 COPYRIGHT

Copyright (c) 2001 John BEPPU.  All rights reserved.  This program is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=head1 AUTHOR

John BEPPU <beppu@lineo.com>

=cut

# $Id: autodocifier.pl,v 1.4 2001/02/23 03:12:45 beppu Exp $
