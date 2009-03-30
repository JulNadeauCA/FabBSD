# ex:ts=8 sw=4:
# $OpenBSD: PackageRepositoryList.pm,v 1.16 2007/05/19 09:50:31 espie Exp $
#
# Copyright (c) 2003-2006 Marc Espie <espie@openbsd.org>
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
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;

package OpenBSD::PackageRepositoryList;

sub new
{
	my $class = shift;
	return bless {list => [] }, $class;
}

sub add
{
	my $self = shift;
	push @{$self->{list}}, @_;
}

sub find
{
	my ($self, $pkgname, $arch) = @_;

	for my $repo (@{$self->{list}}) {
		my $pkg = $repo->find($pkgname, $arch);
		return $pkg if defined $pkg;
	}
	return;
}

sub grabPlist
{
	my ($self, $pkgname, $arch, $code) = @_;

	for my $repo (@{$self->{list}}) {
		my $plist = $repo->grabPlist($pkgname, $arch, $code);
		return $plist if defined $plist;
	}
	return;
}

sub match
{
	my ($self, @search) = @_;
	for my $repo (@{$self->{list}}) {
		my @l = $repo->match(@search);
		if (@l > 0) {
			return @l;
		}
	}
	return ();
}

sub match_locations
{
	my ($self, @search) = @_;
	for my $repo (@{$self->{list}}) {
		my @l = $repo->match_locations(@search);
		if (@l > 0) {
			return @l;
		}
	}
	return ();
}

sub cleanup
{
	my $self = shift;
	for my $repo (@{$self->{list}}) {
		$repo->cleanup;
	}
}

1;
