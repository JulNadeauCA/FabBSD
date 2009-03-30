# ex:ts=8 sw=4:
# $OpenBSD: Error.pm,v 1.13 2008/06/21 13:23:09 espie Exp $
#
# Copyright (c) 2004 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Error;
our @ISA=qw(Exporter);
our @EXPORT=qw(System VSystem Copy Unlink Fatal Warn Usage set_usage 
    try throw catch catchall rethrow);

our ($FileName, $Line, $FullMessage);

my @signal_name = ();

sub fillup_names
{
	{
	# XXX force autoload
	package verylocal;

	require POSIX;
	POSIX->import(qw(signal_h));
	}

	for my $sym (keys %POSIX::) {
		next unless $sym =~ /^SIG([A-Z].*)/;
		$signal_name[eval "&POSIX::$sym()"] = $1;
	}
	# extra BSD signals
	$signal_name[5] = 'TRAP';
	$signal_name[7] = 'IOT';
	$signal_name[10] = 'BUS';
	$signal_name[12] = 'SYS';
	$signal_name[16] = 'URG';
	$signal_name[23] = 'IO';
	$signal_name[24] = 'XCPU';
	$signal_name[25] = 'XFSZ';
	$signal_name[26] = 'VTALRM';
	$signal_name[27] = 'PROF';
	$signal_name[28] = 'WINCH';
	$signal_name[29] = 'INFO';
}

sub find_signal
{
	my $number =  shift;

	if (@signal_name == 0) {
		fillup_names();
	}

	return $signal_name[$number] || $number;
}

sub child_error
{
	my $error = $?;

	my $extra = "";

	if ($error & 128) {
		$extra = " (core dumped)";
	} 
	if ($error & 127) {
		return "killed by signal ". find_signal($error & 127).$extra;
	} else {
		return "exit(". ($error >> 8) . ")$extra";
	}
}

sub System
{
	my $r = system(@_);
	if ($r != 0) {
		print "system(", join(", ", @_), ") failed: ", child_error(), 
		    "\n";
	}
	return $r;
}

sub VSystem
{
	my $verbose = shift;
	if (!$verbose) {
		&System;
	} else {
		print "Running ", join(' ', @_);
		my $r = system(@_);
		if ($r != 0) {
			print "... failed: ", child_error(), "\n";
		} else {
			print "\n";
		}
	}
}

sub Copy
{
	require File::Copy;

	my $r = File::Copy::copy(@_);
	if (!$r) {
		print "copy(", join(',', @_),") failed: $!\n";
	}
	return $r;
}

sub Unlink
{
	my $verbose = shift;
	my $r = unlink @_;
	if ($r != @_) {
		print "rm @_ failed: removed only $r targets, $!\n";
	} elsif ($verbose) {
		print "rm @_\n";
	}
	return $r;
}

sub Fatal
{
	require Carp;
	Carp::croak "Expected: @_";
}

sub Warn
{
	print STDERR @_;
}

sub new
{
	my $class = shift;
	bless {messages=>{}, dirs_okay=>{}}, $class;
}

sub set_pkgname
{
	my ($self, $pkgname) = @_;
	$self->{pkgname} = $pkgname;
	if (!defined $self->{messages}->{$pkgname}) {
		$self->{messages}->{$pkgname} = [];
	}
	$self->{output} = $self->{messages}->{$pkgname};
}

sub warn
{
	&OpenBSD::Error::print;
}

sub fatal
{
	my $self = shift;
	require Carp;
	if (defined $self->{pkgname}) {
		Carp::croak("Expected: ", $self->{pkgname}, ':', @_);
	} else {
		Carp::croak("Expected: ", @_);
	}
}

sub print
{
	my $self = shift;
	push(@{$self->{output}}, join('', @_));
}

sub delayed_output
{
	my $self = shift;
	for my $pkg (sort keys %{$self->{messages}}) {
		my $msgs = $self->{messages}->{$pkg};
		if (@$msgs > 0) {
			print "--- $pkg -------------------\n";
			print @$msgs;
		}
	}
}

sub system
{
	my $state = shift;
	if (open(my $grab, "-|", @_)) {
		local $_;
		while (<$grab>) {
			$state->print($_);
		}
		if (!close $grab) {
		    $state->print("system(", join(", ", @_), ") failed: $! ", 
		    	child_error(), "\n");
		}
		return $?;
	} else {
		    $state->print("system(", join(", ", @_), 
		    	") was not run: $!", child_error(), "\n");
	}
}

my @usage_line;

sub set_usage
{
	@usage_line = @_;
}

sub Usage
{
	my $code = 0;
	if (@_) {
		print STDERR "$0: ", @_, "\n";
		$code = 1;
	}
	print STDERR "Usage: ", shift(@usage_line), "\n";
	for my $l (@usage_line) {
		print STDERR "       $l\n";
	}
	exit($code);
}

sub dienow
{
	my ($error, $handler) = @_;
	if ($error) {
		if ($error =~ m/^(Expected:\s+)?(.*?)(?:\s+at\s+(.*)\s+line\s+(\d+)\.?)?$/o) {
			local $_ = $2;
			$FileName = $3;
			$Line = $4;
			$FullMessage = $error;

			$handler->exec($error, $1, $2, $3, $4);
		} else {
			die "Fatal error: can't parse $error";
		}
	}
}

sub try(&@) 
{
	my ($try, $catch) = @_;
	eval { &$try };
	dienow($@, $catch);
}

sub throw 
{
	require Carp;
	Carp::croak "Expected: @_";

}

sub rethrow
{
	my $e = shift;
	die $e if $e;
}

sub catch(&)
{
		bless $_[0], "OpenBSD::Error::catch";
}

sub catchall(&)
{
	bless $_[0], "OpenBSD::Error::catchall";
}

package OpenBSD::Error::catch;
sub exec
{
	my ($self, $full, $e) = @_;
	if ($e) {
		&$self;
	} else {
		die $full;
	}
}

package OpenBSD::Error::catchall;
sub exec
{
	my ($self, $full, $e) = @_;
	&$self;
}

1;
