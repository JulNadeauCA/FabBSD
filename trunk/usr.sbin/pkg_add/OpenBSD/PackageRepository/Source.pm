# ex:ts=8 sw=4:
# $OpenBSD: Source.pm,v 1.8 2008/07/04 10:47:13 espie Exp $
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

package OpenBSD::PackageRepository::Source;
our @ISA=(qw(OpenBSD::PackageRepository));
use OpenBSD::PackageInfo;
use OpenBSD::Paths;

sub urlscheme
{
	return 'src';
}

sub build_package
{
	my ($self, $pkgpath) = @_;

	my $dir;
	my $make;
	if (defined $ENV{'MAKE'}) {
		$make = $ENV{'MAKE'};
	} else {
		$make = OpenBSD::Paths->make;
	}
	if (defined $self->{baseurl} && $self->{baseurl} ne '') {
		$dir = $self->{baseurl}
	} elsif (defined $ENV{PORTSDIR}) {
		$dir = $ENV{PORTSDIR};
	} else {
		$dir = OpenBSD::Paths->portsdir;
	}
	# figure out the repository name and the pkgname
	my $pkgfile = `cd $dir && SUBDIR=$pkgpath ECHO_MSG=: $make show=PKGFILE`;
	chomp $pkgfile;
	if (! -f $pkgfile) {
		system "cd $dir && SUBDIR=$pkgpath $make package BULK=Yes PKGDB_LOCK='-F nolock' FETCH_PACKAGES=No";
	}
	if (! -f $pkgfile) {
		return undef;
	}
	$pkgfile =~ m|(.*/)([^/]*)|o;
	my ($base, $fname) = ($1, $2);

	my $repo = OpenBSD::PackageRepository::Local->_new($base);
	return $repo;
}

sub match
{
	my ($self, $search, @filters) = @_;
	my $built;

	if (defined $search->{pkgpath}) {
		$built = $self->build_package($search->{pkgpath});
	}
	if ($built) {
		return $built->match($search, @filters);
	}
	return ();
}

1;
