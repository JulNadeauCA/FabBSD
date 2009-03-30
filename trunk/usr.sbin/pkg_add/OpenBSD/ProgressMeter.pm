# ex:ts=8 sw=4:
# $OpenBSD: ProgressMeter.pm,v 1.11 2007/06/01 14:58:29 espie Exp $
#
# Copyright (c) 2004-2007 Marc Espie <espie@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#

use strict;
use warnings;

package OpenBSD::ProgressMeter;

my $header;
my $lastdisplay = '';
my $isatty;
my $continued = 0;

# unless we know better
my $width;
my $playfield;

my $wsz_format = 'SSSS';
our %sizeof;

sub new
{
	my $class = shift;
	$isatty = -t STDERR;
	return bless {}, $class;
}

sub find_window_size
{
	return if defined $width;
	# try to get exact window width
	my $r;
	$r = pack($wsz_format, 0, 0, 0, 0);
	$sizeof{'struct winsize'} = 8;
	require 'sys/ttycom.ph';
	$width = 80;
	if (ioctl(STDERR, &TIOCGWINSZ, $r)) {
		my ($rows, $cols, $xpix, $ypix) = 
		    unpack($wsz_format, $r);
		$width = $cols;
	}
}

sub compute_playfield
{
	return unless $isatty;
	# compute playfield
	$playfield = $width - length($header) - 10;
	if ($playfield < 5) {
		$playfield = 0;
	}
}

sub set_header
{
	my $self = shift;
	$header = shift;
	if ($isatty) {
		find_window_size();
		compute_playfield();
		$SIG{'WINCH'} = sub {
			$width = undef;
			find_window_size();
			compute_playfield();
		};
		$SIG{'CONT'} = sub {
			$continued = 1;
		};
	}
	return $isatty;
}

sub message
{
	my $self = shift;
	return unless $isatty;
	my $message = shift;
	my $d;
	if ($playfield > length($message)) {
		$message .= ' 'x($playfield - length($message));
	}
	if ($playfield) {
		$d = "$header|".substr($message, 0, $playfield);
	} else {
		$d = $header;
	}
	return if $d eq $lastdisplay && !$continued;
	$lastdisplay=$d;
	$continued = 0;
	print STDERR $d, "\r";
}

sub show
{
	my $self = shift;
	return unless $isatty;
	my ($current, $total) = @_;
	my $d;

	if ($playfield) {
	    my $stars = int (($current * $playfield) / $total + 0.5);
	    my $percent = int (($current * 100)/$total + 0.5);
	    $d = "$header|".'*'x$stars.' 'x($playfield-$stars)."| ".$percent."\%";
	} else {
	    $d = $header;
	}
	return if $d eq $lastdisplay;
        $lastdisplay=$d;
        print STDERR $d, "\r";
}

sub clear
{
	my $self = shift;
	return unless $isatty;
	print STDERR ' 'x length($lastdisplay), "\r";
}

sub next
{
	my $self = shift;
	return unless $isatty;
	$self->clear;
	print STDERR"$header: complete\n";
}

1;
