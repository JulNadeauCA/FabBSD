#!/usr/bin/perl

# This script reorders config_h.SH after metaconfig
# Changing metaconfig is too complicated
#
# Copyright (C) 2005-2005 by H.Merijn Brand (m)'05 [25-05-2005]
#
# You may distribute under the terms of either the GNU General Public
# License or the Artistic License, as specified in the README file.

use strict;
use warnings;

my ($cSH, $ch, @ch, %ch) = ("config_h.SH");
open $ch, "<$cSH" or die "Cannot open $cSH: $!\n";
{   local $/ = "\n\n";
    @ch = <$ch>;
    close  $ch;
    }

sub ch_index ()
{
    %ch = ();
    foreach my $ch (0 .. $#ch) {
	while ($ch[$ch] =~ m{^/\* ([A-Z]\w+)}gm) {
	    $ch{$1} = $ch;
	    }
	}
    } # ch_index

my %dep = (
    # This symbol must be defined BEFORE ...
    BYTEORDER		=> [ qw( UVSIZE				) ],
    LONGSIZE		=> [ qw( BYTEORDER			) ],
    MULTIARCH		=> [ qw( BYTEORDER MEM_ALIGNBYTES	) ],
    USE_CROSS_COMPILE	=> [ qw( BYTEORDER MEM_ALIGNBYTES	) ],
    HAS_QUAD		=> [ qw( I64TYPE			) ],
    HAS_GETGROUPS	=> [ qw( Groups_t			) ],
    HAS_SETGROUPS	=> [ qw( Groups_t			) ],
    );

my $changed;
do {
    $changed = 0;
    foreach my $sym (keys %dep) {
	ch_index;
	foreach my $dep (@{$dep{$sym}}) {
	    print STDERR "Check if $sym\t($ch{$sym}) precedes $dep\t($ch{$dep})\n";
	    $ch{$sym} < $ch{$dep} and next;
	    my $ch = splice @ch, $ch{$sym}, 1;
	    splice @ch, $ch{$dep}, 0, $ch;
	    $changed++;
	    ch_index;
	    }
	}
    } while ($changed);

open  $ch, "> $cSH" or die "Cannot write $cSH: $!\n";
print $ch @ch;
close $ch;
