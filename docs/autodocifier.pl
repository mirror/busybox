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
	$text =~ s/USAGE_NOT\w+\(.*?"\s*\)//sxg;
	$text =~ s/USAGE_\w+\(\s*?(.*?)"\s*\)/$1"/sxg;
	$text =~ s/"\s*"//sg;
	my @line = split("\n", $text);
	$text = join('',
		map { 
			s/^\s*//;
			s/"//g;
			s/%/%%/g;
			eval qq[ sprintf(qq#$_#) ]
		} @line
	);
	return $text;
}

# generate POD for an applet
sub pod_for_usage {
	my $name  = shift;
	my $usage = shift;

	# make options bold
	my $trivial = $usage->{trivial};
	$trivial =~s/(?<!\w)(-\w+)/B<$1>/sxg;
	my @f0 = 
		map { $_ !~ /^\s/ && s/(?<!\w)(-\w+)/B<$1>/g; $_ }
		split("\n", $usage->{full});

	# add "\n" prior to certain lines to make indented
	# lines look right
	my @f1;
	my $len = @f0;
	for (my $i = 0; $i < $len; $i++) {
		push @f1, $f0[$i];
		if (($i+1) != $len && $f0[$i] !~ /^\s/ && $f0[$i+1] =~ /^\s/) {
			next if ($f0[$i] =~ /^$/);
			push(@f1, "") unless ($f0[$i+1] =~ /^\s*$/s);
		}
	}

	my $full = join("\n", @f1);
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

# FIXME | generate SGML for an applet
sub sgml_for_usage {
	my $name  = shift;
	my $usage = shift;
	return
		"<fixme>\n".
		"  $name\n".
		"</fixme>\n"
	;
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


# generate structured documentation

my $generator = \&pod_for_usage;
if (defined $opt{sgml}) {
	$generator = \&sgml_for_usage;
}

foreach my $applet (sort keys %docs) {
	print $generator->($applet, $docs{$applet});
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

=over 4

=item --help

This displays the help message.

=item --pod

Generate POD (this is the default)

=item --sgml

Generate SGML

=item --verbose

Be verbose (not implemented)

=back

=head1 FILES

F<usage.h>

=head1 COPYRIGHT

Copyright (c) 2001 John BEPPU.  All rights reserved.  This program is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=head1 AUTHOR

John BEPPU <beppu@lineo.com>

=cut

# $Id: autodocifier.pl,v 1.11 2001/02/24 14:37:48 beppu Exp $
