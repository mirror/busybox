#!/usr/bin/perl -w
# vi: set sw=4 ts=4:

use strict;
use Getopt::Long;

# collect lines continued with a '\' into an array
sub continuation {
	my $fh = shift;
	my @line;

	while (<$fh>) {
		my $s = $_;
		$s =~ s/\\\s*$//;
		#$s =~ s/#.*$//;
		push @line, $s;
		last unless (/\\\s*$/);
	}
	return @line;
}

# regex && eval away unwanted strings from documentation
sub beautify {
	my $text = shift;
	for (;;) {
		my $text2 = $text;
		$text =~ s/SKIP_\w+\(.*?"\s*\)//sxg;
		$text =~ s/USE_\w+\(\s*?(.*?)"\s*\)/$1"/sxg;
		$text =~ s/USAGE_\w+\(\s*?(.*?)"\s*\)/$1"/sxg;
		last if ( $text2 eq $text );
	}
	$text =~ s/"\s*"//sg;
	my @line = split("\n", $text);
	$text = join('',
		map {
			s/^\s*"//;
			s/"\s*$//;
			s/%/%%/g;
			s/\$/\\\$/g;
			s/\@/\\\@/g;
			eval qq[ sprintf(qq{$_}) ]
		} @line
	);
	return $text;
}

# generate POD for an applet
sub pod_for_usage {
	my $name  = shift;
	my $usage = shift;

	# Sigh.  Fixup the known odd-name applets.
# Perhaps we can use some of APPLET_ODDNAME from include/applets.h ?
	$name =~ s/dpkg_deb/dpkg-deb/g;
	$name =~ s/fsck_minix/fsck.minix/g;
	$name =~ s/mkfs_minix/mkfs.minix/g;
	$name =~ s/run_parts/run-parts/g;
	$name =~ s/start_stop_daemon/start-stop-daemon/g;
	$name =~ s/ether_wake/ether-wake/g;

	# make options bold
	my $trivial = $usage->{trivial};
	if (!defined $usage->{trivial}) {
		$trivial = "";
	} else {
		$trivial =~ s/(?<!\w)(-\w+)/B<$1>/sxg;
	}
	my @f0 =
		map { $_ !~ /^\s/ && s/(?<!\w)(-\w+)/B<$1>/g; $_ }
		split("\n", (defined $usage->{full} ? $usage->{full} : ""));

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

	# prepare notes if they exist
	my $notes = (defined $usage->{notes})
		? "$usage->{notes}\n\n"
		: "";

	# prepare examples if they exist
	my $example = (defined $usage->{example})
		?
			"Example:\n\n" .
			join ("\n",
			map  { "\t$_" }
			split("\n", $usage->{example})) . "\n\n"
		: "";

	# Pad the name so that the applet name gets a line
	# by itself in BusyBox.txt
	my $spaces = 10 - length($name);
	if ($spaces > 0) {
		$name .= " " x $spaces;
	}

	return
		"=item B<$name>".
		"\n\n$name $trivial\n\n".
		"$full\n\n"   .
		"$notes"  .
		"$example" .
		"\n\n"
	;
}

# the keys are applet names, and
# the values will contain hashrefs of the form:
#
# {
#     trivial => "...",
#     full    => "...",
#     notes   => "...",
#     example => "...",
# }
my %docs;


# get command-line options

my %opt;

GetOptions(
	\%opt,
	"help|h",
	"pod|p",
	"verbose|v",
);

if (defined $opt{help}) {
	print
		"$0 [OPTION]... [FILE]...\n",
		"\t--help\n",
		"\t--pod\n",
		"\t--verbose\n",
	;
	exit 1;
}


# collect documenation into %docs

foreach (@ARGV) {
	open(USAGE, $_) || die("$0: $_: $!");
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

my @names = sort keys %docs;
my $line = "\t[, [[, ";
for (my $i = 0; $i < $#names; $i++) {
	if (length ($line.$names[$i]) >= 65) {
		print "$line\n\t";
		$line = "";
	}
	$line .= "$names[$i], ";
}
print $line . $names[-1];

print "\n\n=head1 COMMAND DESCRIPTIONS\n";
print "\n=over 4\n\n";

foreach my $applet (@names) {
	print $generator->($applet, $docs{$applet});
}

exit 0;

__END__

=head1 NAME

autodocifier.pl - generate docs for busybox based on usage.h

=head1 SYNOPSIS

autodocifier.pl [OPTION]... [FILE]...

Example:

    ( cat docs/busybox_header.pod; \
      docs/autodocifier.pl usage.h; \
      cat docs/busybox_footer.pod ) > docs/busybox.pod

=head1 DESCRIPTION

The purpose of this script is to automagically generate
documentation for busybox using its usage.h as the original source
for content.  It used to be that same content has to be duplicated
in 3 places in slightly different formats -- F<usage.h>,
F<docs/busybox.pod>.  This was tedious and error-prone, so it was
decided that F<usage.h> would contain all the text in a
machine-readable form, and scripts could be used to transform this
text into other forms if necessary.

F<autodocifier.pl> is one such script.  It is based on a script by
Erik Andersen <andersen@codepoet.org> which was in turn based on a
script by Mark Whitley <markw@codepoet.org>

=head1 OPTIONS

=over 4

=item B<--help>

This displays the help message.

=item B<--pod>

Generate POD (this is the default)

=item B<--verbose>

Be verbose (not implemented)

=back

=head1 FORMAT

The following is an example of some data this script might parse.

    #define length_trivial_usage \
            "STRING"
    #define length_full_usage \
            "Prints out the length of the specified STRING."
    #define length_example_usage \
            "$ length Hello\n" \
            "5\n"

Each entry is a cpp macro that defines a string.  The macros are
named systematically in the form:

    $name_$type_usage

$name is the name of the applet.  $type can be "trivial", "full", "notes",
or "example".  Every documentation macro must end with "_usage".

The definition of the types is as follows:

=over 4

=item B<trivial>

This should be a brief, one-line description of parameters that
the command expects.  This will be displayed when B<-h> is issued to
a command.  I<REQUIRED>

=item B<full>

This should contain descriptions of each option.  This will also
be displayed along with the trivial help if CONFIG_FEATURE_TRIVIAL_HELP
is disabled.  I<REQUIRED>

=item B<notes>

This is documentation that is intended to go in the POD or SGML, but
not be printed when a B<-h> is given to a command.  To see an example
of notes being used, see init_notes_usage in F<usage.h>.  I<OPTIONAL>

=item B<example>

This should be an example of how the command is actually used.
This will not be printed when a B<-h> is given to a command -- it
will only be included in the POD or SGML documentation.  I<OPTIONAL>

=back

=head1 FILES

F<usage.h>

=head1 COPYRIGHT

Copyright (c) 2001 John BEPPU.  All rights reserved.  This program is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=head1 AUTHOR

John BEPPU <b@ax9.org>

=cut

