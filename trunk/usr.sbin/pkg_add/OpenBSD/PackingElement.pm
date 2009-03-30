# ex:ts=8 sw=4:
# $OpenBSD: PackingElement.pm,v 1.148 2008/06/11 12:21:03 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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
use OpenBSD::PackageInfo;
use OpenBSD::Paths;

# perl ipc
require 5.008_000;

# This is the basic class, which is mostly abstract, except for
# create and register_with_factory.
# It does provide base methods for stuff under it, though.
package OpenBSD::PackingElement;
our %keyword;

sub create
{
	my ($class, $line, $plist) = @_;
	if ($line =~ m/^\@(\S+)\s*(.*)$/o) {
		if (defined $keyword{$1}) {
			$keyword{$1}->add($plist, $2);
		} else {
			die "Unknown element: $line";
		}
	} else {
		chomp $line;
		OpenBSD::PackingElement::File->add($plist, $line);
	}
}

sub register_with_factory 
{
	my ($class, $k, $o) = @_;
	if (!defined $k) {
		$k = $class->keyword;
	}
	if (!defined $o) {
		$o = $class;
	} 
	$keyword{$k} = $o;
}

sub category() { 'items' }

sub new
{
	my ($class, $args) = @_;
	bless { name => $args }, $class;
}

sub clone
{
	my $object = shift;
	# shallow copy
	my %h = %$object;
	bless \%h, ref($object);
}
	

sub register_manpage
{
}

sub destate
{
}

sub add_object
{
	my ($self, $plist) = @_;
	$self->destate($plist->{state});
	$plist->add2list($self);
	return $self;
}

sub add
{
	my ($class, $plist, @args) = @_;

	my $self = $class->new(@args);
	return $self->add_object($plist);
}

sub needs_keyword() { 1 }
	
sub write
{
	my ($self, $fh) = @_;
	my $s = $self->stringize;
	if ($self->needs_keyword) {
		$s = " $s" unless $s eq '';
		print $fh "\@", $self->keyword, "$s\n";
	} else {
		print $fh "$s\n";
	}
}

sub write_no_sig
{
	my ($self, $fh) = @_;
	$self->write($fh);
}

# needed for comment checking
sub fullstring
{
	my ($self, $fh) = @_;
	my $s = $self->stringize;
	if ($self->needs_keyword) {
		$s = " $s" unless $s eq '';
		return "\@".$self->keyword.$s;
	} else {
		return $s;
	}
}

sub stringize
{
	my $self = shift;
	return $self->{name};
}

sub IsFile() { 0 }

sub NoDuplicateNames() { 0 }

sub signature {}

sub copy_shallow_if
{
	my ($self, $copy, $h) = @_;
	$self->add_object($copy) if defined $h->{$self};
}

sub copy_deep_if
{
	my ($self, $copy, $h) = @_;
	$self->clone->add_object($copy) if defined $h->{$self};
}

# Basic class hierarchy

# various stuff that's only linked to objects before/after them
# this class doesn't have real objects: no valid new nor clone...
package OpenBSD::PackingElement::Annotation;
our @ISA=qw(OpenBSD::PackingElement);
sub new { die "Can't create annotation objects" }

# concrete objects
package OpenBSD::PackingElement::Object;
our @ISA=qw(OpenBSD::PackingElement);

sub cwd
{
	return ${$_[0]->{cwd}};
}

sub compute_fullname
{
	my ($self, $state, $absolute_okay) = @_;

	$self->{cwd} = $state->{cwd};
	$self->{name} = File::Spec->canonpath($self->{name});
	if ($self->{name} =~ m|^/|) {
		unless ($absolute_okay) {
			die "Absolute name forbidden: ", $self->{name};
		}
	}
}

sub fullname
{
	my $self = $_[0];
	my $fullname = $self->{name};
	if ($fullname !~ m|^/|o && $self->cwd ne '.') {
		$fullname = $self->cwd."/".$fullname;
	}
	return $fullname;
}

sub compute_modes
{
	my ($self, $state) = @_;
	if (defined $state->{mode}) {
		$self->{mode} = $state->{mode};
	}
	if (defined $state->{owner}) {
		$self->{owner} = $state->{owner};
	}
	if (defined $state->{group}) {
		$self->{group} = $state->{group};
	}
}

# concrete objects with file-like behavior
package OpenBSD::PackingElement::FileObject;
our @ISA=qw(OpenBSD::PackingElement::Object);

sub NoDuplicateNames() { 1 }

sub dirclass() { undef }

sub new
{
	my ($class, $args) = @_;
	if ($args =~ m/^(.*?)\/+$/o and defined $class->dirclass) {
		bless { name => $1 }, $class->dirclass;
	} else {
		bless { name => $args }, $class;
	}
}

sub destate
{
	my ($self, $state) = @_;
	$state->{lastfileobject} = $self;
	$self->compute_fullname($state);
}

sub set_tempname
{
	my ($self, $tempname) = @_;
	$self->{tempname} = $tempname;
}

sub realname
{
	my ($self, $state) = @_;

	my $name = $self->fullname;
	if (defined $self->{tempname}) {
		$name = $self->{tempname};
	}
	return $state->{destdir}.$name;
}

sub compute_md5
{
	my ($self, $filename) = @_;
	require OpenBSD::md5;
	return OpenBSD::md5::fromfile($filename);
}

sub write
{
	my ($self, $fh) = @_;

	$self->SUPER::write($fh);
	if (defined $self->{tags}) {
		for my $tag (sort keys %{$self->{tags}}) {
			print $fh "\@tag ", $tag, "\n";
		}
	}
}

# exec/unexec and friends
package OpenBSD::PackingElement::Action;
our @ISA=qw(OpenBSD::PackingElement::Object);

# persistent state for following objects
package OpenBSD::PackingElement::State;
our @ISA=qw(OpenBSD::PackingElement::Object);

# meta information, stored elsewhere
package OpenBSD::PackingElement::Meta;
our @ISA=qw(OpenBSD::PackingElement);

package OpenBSD::PackingElement::Unique;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub add_object
{
	my ($self, $plist) = @_;

	$self->destate($plist->{state});
	$plist->addunique($self);
	return $self;
}

sub category
{
	return ref(shift);
}

# all dependency information
package OpenBSD::PackingElement::Depend;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub signature
{
	my ($self, $hash) = @_;
	$hash->{$self->{name}} = 1;
}

# Abstract class for all file-like elements
package OpenBSD::PackingElement::FileBase;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

use File::Basename;

sub write
{
	my ($self, $fh) = @_;
	print $fh "\@comment no checksum\n" if defined $self->{nochecksum};
	$self->SUPER::write($fh);
	if (defined $self->{md5}) {
		print $fh "\@md5 ", unpack('H*', $self->{md5}), "\n";
	}
	if (defined $self->{size}) {
		print $fh "\@size ", $self->{size}, "\n";
	}
	if (defined $self->{symlink}) {
		print $fh "\@symlink ", $self->{symlink}, "\n";
	}
	if (defined $self->{link}) {
		print $fh "\@link ", $self->{link}, "\n";
	}
	if (defined $self->{tempname}) {
		print $fh "\@temp ", $self->{tempname}, "\n";
	}
}

sub destate
{
	my ($self, $state) = @_;
	$self->SUPER::destate($state);
	$state->{lastfile} = $self;
	$state->{lastchecksummable} = $self;
	$self->compute_modes($state);
	if (defined $state->{nochecksum}) {
		$self->{nochecksum} = 1;
		undef $state->{nochecksum};
	}
}

sub add_md5
{
	my ($self, $md5) = @_;
	$self->{md5} = $md5;
}

sub add_size
{
	my ($self, $sz) = @_;
	$self->{size} = $sz;
}

# XXX symlink/hardlinks are properties of File,
# because we want to use inheritance for other stuff.

sub make_symlink
{
	my ($self, $linkname) = @_;
	$self->{symlink} = $linkname;
}

sub make_hardlink
{
	my ($self, $linkname) = @_;
	$self->{link} = $linkname;
}

sub IsFile() { 1 }

package OpenBSD::PackingElement::File;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

use OpenBSD::PackageInfo qw(is_info_name);
sub keyword() { "file" }
__PACKAGE__->register_with_factory;

sub dirclass() { "OpenBSD::PackingElement::Dir" }

sub needs_keyword
{
	my $self = shift;
	return $self->stringize =~ m/\^@/;
}

sub add_object
{
	my ($self, $plist) = @_;

	$self->destate($plist->{state});
	my $j = is_info_name($self->fullname);
	if ($j) {
		bless $self, "OpenBSD::PackingElement::$j";
		$self->add_object($plist);
	} else {
		$plist->add2list($self);
	}
	return $self;
}

package OpenBSD::PackingElement::Sample;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

sub keyword() { "sample" }
__PACKAGE__->register_with_factory;
sub destate
{
	my ($self, $state) = @_;
	$self->{copyfrom} = $state->{lastfile};
	$self->compute_fullname($state, 1);
	$self->compute_modes($state);
}

sub dirclass() { "OpenBSD::PackingElement::Sampledir" }

package OpenBSD::PackingElement::Sampledir;
our @ISA=qw(OpenBSD::PackingElement::DirBase OpenBSD::PackingElement::Sample);

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state, 1);
	$self->compute_modes($state);
}

package OpenBSD::PackingElement::InfoFile;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "info" }
__PACKAGE__->register_with_factory;
sub dirclass() { "OpenBSD::PackingElement::Infodir" }

package OpenBSD::PackingElement::Shell;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "shell" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::Manpage;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "man" }
__PACKAGE__->register_with_factory;

sub register_manpage
{
	my ($self, $state) = @_;
	return if defined $self->{tempname};
	my $fname = $self->fullname;
	if ($fname =~ m,^(.*/man)/(?:man|cat).*?/,) {
		my $d = $1;
		push(@{$state->{mandirs}->{$d}}, $fname);
    	}
}

sub is_source
{
	my $self = shift;
	return $self->{name} =~ m/man\/man[^\/]+\/[^\/]+\.[\dln][^\/]?$/o;
}

sub source_to_dest
{
	my $self = shift;
	my $v = $self->{name};
	$v =~ s/(man\/)man([^\/]+\/[^\/]+)\.[\dln][^\/]?$/$1cat$2.0/;
	return $v;
}

# assumes the source is nroff, launches nroff
sub format
{
	my ($self, $base, $out) = @_;
	my $fname = $base."/".$self->fullname;
	open(my $fh, '<', $fname) or die "Can't read $fname";
	my $line = <$fh>;
	close $fh;
	my @extra = ();
	# extra preprocessors as described in man.
	if ($line =~ m/^\'\\\"\s+(.*)$/o) {
		for my $letter (split $1) {
			if ($letter =~ m/[ept]/o) {
				push(@extra, "-$letter");
			} elsif ($letter eq 'r') {
				push(@extra, "-R");
			}
		}
	}
	open my $oldout, '>&STDOUT';
	open STDOUT, '>', "$base/$out" or die "Can't write to $base/$out";
	system(OpenBSD::Paths->groff,
	    '-Tascii', '-mandoc', '-Wall', '-mtty-char', @extra, $fname);
	open STDOUT, '>&', $oldout;
}

package OpenBSD::PackingElement::Lib;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

our $todo = 0;

sub keyword() { "lib" }
__PACKAGE__->register_with_factory;

sub mark_ldconfig_directory
{
	require OpenBSD::SharedLibs;

	my ($self, $destdir) = @_;
	OpenBSD::SharedLibs::mark_ldconfig_directory($self->fullname, 
	    $destdir);
}

sub ensure_ldconfig
{
	if ($todo) {
		require OpenBSD::SharedLibs;

		&OpenBSD::SharedLibs::ensure_ldconfig;
	}
}

sub parse
{
	my ($self, $filename) = @_;
	if ($filename =~ m/^(.*?)\/?lib([^\/]+)\.so\.(\d+)\.(\d+)$/o) {
		return ($2, $3, $4, $1);
	} else {
		return undef;
	}
}

package OpenBSD::PackingElement::PkgConfig;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "pkgconfig" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::LibtoolLib;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "ltlib" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::Binary;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "bin" }
__PACKAGE__->register_with_factory;

# Comment is very special
package OpenBSD::PackingElement::Comment;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "comment" }
__PACKAGE__->register_with_factory;

sub add
{
	my ($class, $plist, $args) = @_;

	if ($args =~ m/^\$OpenBSD.*\$\s*$/o) {
		return OpenBSD::PackingElement::CVSTag->add($plist, $args);
	} elsif ($args =~ m/^subdir\=(.*?)\s+cdrom\=(.*?)\s+ftp\=(.*?)\s*$/o) {
		return OpenBSD::PackingElement::ExtraInfo->add($plist, $1, $2, $3);
	} elsif ($args eq 'no checksum') {
		$plist->{state}->{nochecksum} = 1;
		return;
	} else {
		return $class->SUPER::add($plist, $args);
	}
}

package OpenBSD::PackingElement::CVSTag;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { 'comment' }

sub category() { 'cvstags'}

package OpenBSD::PackingElement::md5;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('md5');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastchecksummable}->add_md5(pack('H*', $args));
	return;
}

package OpenBSD::PackingElement::tag;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('tag');

sub add
{
	my ($class, $plist, $args) = @_;

	if ($args eq 'no checksum') {
		$plist->{state}->{lastfile}->{nochecksum} = 1;
	} elsif ($args eq 'no shadow') {
		$plist->{state}->{lastdir}->{noshadow} = 1;
	} else {
		my $object = $plist->{state}->{lastfileobject};
		$object->{tags}->{$args} = 1;
		push(@{$plist->{tags}->{$args}}, $object);
	}
}

package OpenBSD::PackingElement::DefineTag;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub category() { 'define-tag' }
sub keyword() { 'define-tag' }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my ($tag, $condition, @command) = split(/\s+/, $args);
	bless { 
		name => $tag, 
		when => $condition, 
		command => join(' ', @command)
	}, $class;
}

sub stringize
{
	my $self = shift;
	return join(' ', map { $self->{$_}} 
		(qw(name when command)));
}

package OpenBSD::PackingElement::symlink;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('symlink');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastfile}->make_symlink($args);
	return;
}

package OpenBSD::PackingElement::hardlink;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('link');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastfile}->make_hardlink($args);
	return;
}

package OpenBSD::PackingElement::temp;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('temp');

sub add
{
	my ($class, $plist, $args) = @_;
	$plist->{state}->{lastfile}->set_tempname($args);
	return;
}

package OpenBSD::PackingElement::size;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('size');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastfile}->add_size($args);
	return;
}

package OpenBSD::PackingElement::Option;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { 'option' }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	if ($args eq 'no-default-conflict') {
		return OpenBSD::PackingElement::NoDefaultConflict->new;
	} elsif ($args eq 'manual-installation') {
		return OpenBSD::PackingElement::ManualInstallation->new;
	} elsif ($args eq 'system-package') {
		return OpenBSD::PackingElement::SystemPackage->new;
	} else {
		die "Unknown option: $args";
	}
}

package OpenBSD::PackingElement::UniqueOption;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::Option);

sub stringize 
{ 
	my $self = shift;
	return $self->category; 
}

sub new
{
	my ($class, @args) = @_;
	bless {}, $class;
}

package OpenBSD::PackingElement::NoDefaultConflict;
our @ISA=qw(OpenBSD::PackingElement::UniqueOption);

sub category() { 'no-default-conflict' }

package OpenBSD::PackingElement::ManualInstallation;
our @ISA=qw(OpenBSD::PackingElement::UniqueOption);

sub category() { 'manual-installation' }

package OpenBSD::PackingElement::SystemPackage;
our @ISA=qw(OpenBSD::PackingElement::UniqueOption);

sub category() { 'system-package' }

# The special elements that don't end in the right place
package OpenBSD::PackingElement::ExtraInfo;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::Comment);

sub category() { 'extrainfo' }

sub new
{
	my ($class, $subdir, $cdrom, $ftp) = @_;

	$cdrom =~ s/^\"(.*)\"$/$1/;
	$cdrom =~ s/^\'(.*)\'$/$1/;
	$ftp =~ s/^\"(.*)\"$/$1/;
	$ftp =~ s/^\'(.*)\'$/$1/;
	bless { subdir => $subdir, cdrom => $cdrom, ftp => $ftp}, $class;
}

sub may_quote
{
	my $s = shift;
	if ($s =~ m/\s/) {
		return '"'.$s.'"';
	} else {
		return $s;
	}
}

sub stringize
{
	my $self = shift;
	return "subdir=".$self->{subdir}." cdrom=".may_quote($self->{cdrom}).
	    " ftp=".may_quote($self->{ftp});
}

package OpenBSD::PackingElement::Name;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub keyword() { "name" }
__PACKAGE__->register_with_factory;
sub category() { "name" }

package OpenBSD::PackingElement::LocalBase;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub keyword() { "localbase" }
__PACKAGE__->register_with_factory;
sub category() { "localbase" }

package OpenBSD::PackingElement::Conflict;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "conflict" }
__PACKAGE__->register_with_factory;
sub category() { "conflict" }

package OpenBSD::PackingElement::Dependency;
our @ISA=qw(OpenBSD::PackingElement::Depend);

sub keyword() { "depend" }
__PACKAGE__->register_with_factory;
sub category() { "depend" }

sub new
{
	my ($class, $args) = @_;
	my ($pkgpath, $pattern, $def) = split /\:/o, $args;
	bless { name => $def, pkgpath => $pkgpath, pattern => $pattern, 
	    def => $def }, $class;
}

sub stringize
{
	my $self = shift;
	return join(':', map { $self->{$_}} 
	    (qw(pkgpath pattern def)));
}

sub spec
{
	my $self = shift;
	if (!defined $self->{spec}) {
		require OpenBSD::Search;
		$self->{spec} = OpenBSD::Search::PkgSpec->new($self->{pattern});
		$self->{spec}->add_pkgpath_hint($self->{pkgpath});
	}
	return $self->{spec};
}

package OpenBSD::PackingElement::Wantlib;
our @ISA=qw(OpenBSD::PackingElement::Depend);

sub category() { "wantlib" }
sub keyword() { "wantlib" }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$state->{lastchecksummable} = $self;
}

sub write
{
	my ($self, $fh) = @_;
	$self->SUPER::write($fh);
	if (defined $self->{md5}) {
		print $fh "\@md5 ", unpack('H*', $self->{md5}), "\n";
	}
}

sub add_md5
{
	&OpenBSD::PackingElement::FileBase::add_md5;
}

package OpenBSD::PackingElement::PkgPath;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "pkgpath" }
__PACKAGE__->register_with_factory;
sub category() { "pkgpath" }

package OpenBSD::PackingElement::Incompatibility;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "incompatibility" }
__PACKAGE__->register_with_factory;
sub category() { "incompatibility" }

package OpenBSD::PackingElement::UpdateSet;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "updateset" }
__PACKAGE__->register_with_factory;
sub category() { "updateset" }

package OpenBSD::PackingElement::NewAuth;
our @ISA=qw(OpenBSD::PackingElement::Action);

package OpenBSD::PackingElement::NewUser;
our @ISA=qw(OpenBSD::PackingElement::NewAuth);

sub type() { "user" }
sub category() { "users" }
sub keyword() { "newuser" }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my ($name, $uid, $group, $loginclass, $comment, $home, $shell) = 
	    split /\:/o, $args;
	bless { name => $name, uid => $uid, group => $group, 
	    class => $loginclass, 
	    comment => $comment, home => $home, shell => $shell }, $class;
}

sub check
{
	my $self = shift;
	my ($name, $passwd, $uid, $gid, $quota, $class, $gcos, $dir, $shell, 
	    $expire) = getpwnam($self->{name});
	return unless defined $name;
	if ($self->{uid} =~ m/^\!(.*)$/o) {
		return 0 unless $uid == $1;
	}
	if ($self->{group} =~ m/^\!(.*)$/o) {
		my $g = $1;
		unless ($g =~ m/^\d+$/o) {
			$g = getgrnam($g);
			return 0 unless defined $g;
		}
		return 0 unless $gid eq $g;
	}
	if ($self->{class} =~ m/^\!(.*)$/o) {
		return 0 unless $class eq $1;
	}
	if ($self->{comment} =~ m/^\!(.*)$/o) {
		return 0 unless $gcos eq $1;
	}
	if ($self->{home} =~ m/^\!(.*)$/o) {
		return 0 unless $dir eq $1;
	}
	if ($self->{shell} =~ m/^\!(.*)$/o) {
		return 0 unless $shell eq $1;
	}
	return 1;
}

sub stringize
{
	my $self = shift;
	return join(':', map { $self->{$_}} 
	    (qw(name uid group class comment home shell)));
}

package OpenBSD::PackingElement::NewGroup;
our @ISA=qw(OpenBSD::PackingElement::NewAuth);


sub type() { "group" }
sub category() { "groups" }
sub keyword() { "newgroup" }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my ($name, $gid) = split /\:/o, $args;
	bless { name => $name, gid => $gid }, $class;
}

sub check
{
	my $self = shift;
	my ($name, $passwd, $gid, $members) = getgrnam($self->{name});
	return unless defined $name;
	if ($self->{gid} =~ m/^\!(.*)$/o) {
		return 0 unless $gid == $1;
	}
	return 1;
}

sub stringize($)
{
	my $self = $_[0];
	return join(':', map { $self->{$_}} 
	    (qw(name gid)));
}

package OpenBSD::PackingElement::Cwd;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement::State);


sub keyword() { 'cwd' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$state->set_cwd($self->{name});
}

package OpenBSD::PackingElement::EndFake;
our @ISA=qw(OpenBSD::PackingElement::State);


sub keyword() { 'endfake' }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, @args) = @_;
	bless {}, $class;
}

sub stringize() { '' }

package OpenBSD::PackingElement::Owner;
our @ISA=qw(OpenBSD::PackingElement::State);

sub keyword() { 'owner' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;

	if ($self->{name} eq '') {
		undef $state->{owner};
	} else {
		$state->{owner} = $self->{name};
	}
}

package OpenBSD::PackingElement::Group;
our @ISA=qw(OpenBSD::PackingElement::State);

sub keyword() { 'group' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;

	if ($self->{name} eq '') {
		undef $state->{group};
	} else {
		$state->{group} = $self->{name};
	}
}

package OpenBSD::PackingElement::Mode;
our @ISA=qw(OpenBSD::PackingElement::State);

sub keyword() { 'mode' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;

	if ($self->{name} eq '') {
		undef $state->{mode};
	} else {
		$state->{mode} = $self->{name};
	}
}

package OpenBSD::PackingElement::Sysctl;
our @ISA=qw(OpenBSD::PackingElement::Action);

sub keyword() { 'sysctl' }
__PACKAGE__->register_with_factory;

sub new

{
	my ($class, $args) = @_;
	if ($args =~ m/^\s*(.*)\s*(\=|\>=)\s*(.*)\s*$/o) {
		bless { name => $1, mode => $2, value => $3}, $class;
	} else {
		die "Bad syntax for \@sysctl";
	}
}

sub stringize
{
	my $self = shift;
	return $self->{name}.$self->{mode}.$self->{value};
}

package OpenBSD::PackingElement::ExeclikeAction;
use File::Basename;
use OpenBSD::Error;
our @ISA=qw(OpenBSD::PackingElement::Action);

sub expand
{
	my ($self, $state) = @_;
	local $_ = $self->{name};
	if (m/\%F/o) {
		die "Bad expand" unless defined $state->{lastfile};
		s/\%F/$state->{lastfile}->{name}/g;
	}
	if (m/\%D/o) {
		die "Bad expand" unless defined $state->{cwd};
		s/\%D/$state->cwd/ge;
	}
	if (m/\%B/o) {
		die "Bad expand" unless defined $state->{lastfile};
		s/\%B/dirname($state->{lastfile}->fullname)/ge;
	}
	if (m/\%f/o) {
		die "Bad expand" unless defined $state->{lastfile};
		s/\%f/basename($state->{lastfile}->fullname)/ge;
	}
	return $_;
}

sub destate
{
	my ($self, $state) = @_;
	$self->{expanded} = $self->expand($state);
}

sub run
{
	my ($self, $state) = @_;

	OpenBSD::PackingElement::Lib::ensure_ldconfig($state);
	print $self->keyword, " ", $self->{expanded}, "\n" if $state->{beverbose};
	$state->system(OpenBSD::Paths->sh, '-c', $self->{expanded}) 
	    unless $state->{not};
}

package OpenBSD::PackingElement::Exec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

sub keyword() { "exec" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExecAlways;
our @ISA=qw(OpenBSD::PackingElement::Exec);

sub keyword() { "exec-always" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExecInstall;
our @ISA=qw(OpenBSD::PackingElement::Exec);

sub keyword() { "exec-add" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExecUpdate;
our @ISA=qw(OpenBSD::PackingElement::Exec);

sub keyword() { "exec-update" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::Unexec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

sub keyword() { "unexec" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::UnexecAlways;
our @ISA=qw(OpenBSD::PackingElement::Unexec);

sub keyword() { "unexec-always" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::UnexecUpdate;
our @ISA=qw(OpenBSD::PackingElement::Unexec);

sub keyword() { "unexec-update" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::UnexecDelete;
our @ISA=qw(OpenBSD::PackingElement::Unexec);

sub keyword() { "unexec-delete" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExtraUnexec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

sub keyword() { "extraunexec" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::DirlikeObject;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

package OpenBSD::PackingElement::DirBase;
our @ISA=qw(OpenBSD::PackingElement::DirlikeObject);

sub destate
{
	my ($self, $state) = @_;
	$state->{lastdir} = $self;
	$self->SUPER::destate($state);
}
	

sub stringize
{
	my $self = shift;
	return $self->{name}."/";
}

sub write
{
	my ($self, $fh) = @_;
	$self->SUPER::write($fh);
	if (defined $self->{noshadow}) {
		print $fh "\@tag no shadow\n";
	}
}

package OpenBSD::PackingElement::Dir;
our @ISA=qw(OpenBSD::PackingElement::DirBase);

sub keyword() { "dir" }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$self->SUPER::destate($state);
	$self->compute_modes($state);
}

sub needs_keyword
{
	my $self = shift;
	return $self->stringize =~ m/\^@/o;
}

package OpenBSD::PackingElement::Infodir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
sub keyword() { "info" }
sub needs_keyword() { 1 }

package OpenBSD::PackingElement::Fontdir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
sub keyword() { "fontdir" }
__PACKAGE__->register_with_factory;
sub needs_keyword() { 1 }
sub dirclass() { "OpenBSD::PackingElement::Fontdir" }

our %fonts_todo = ();

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	$fonts_todo{$state->{destdir}.$self->fullname} = 1;
}

sub reload
{
	my ($self, $state) = @_;
	$fonts_todo{$state->{destdir}.$self->fullname} = 1;
}

sub update_fontalias
{
	my $dirname = shift;
	my @aliases;

	for my $alias (glob "$dirname/fonts.alias-*") {
		open my $f ,'<', $alias or next;
		push(@aliases, <$f>);
		close $f;
	}
	open my $f, '>', "$dirname/fonts.alias";
	print $f @aliases;
	close $f;
}

sub restore_fontdir
{
	my $dirname = shift;
	if (-f "$dirname/fonts.dir.dist") {
		require OpenBSD::Error;

		unlink("$dirname/fonts.dir");
		OpenBSD::Error::Copy("$dirname/fonts.dir.dist", "$dirname/fonts.dir");
	}
}

sub run_if_exists
{
	my ($state, $cmd, @l) = @_;

	require OpenBSD::Error;

	if (-x $cmd) {
		OpenBSD::Error::VSystem($state->{very_verbose}, $cmd, @l);
	} else {
		OpenBSD::Error::Warn("$cmd not found\n");
	}
}

sub finish_fontdirs
{
	my $state = shift;
	my @l = keys %fonts_todo;
	if (@l != 0) {
		require OpenBSD::Error;

		map { update_fontalias($_) } @l unless $state->{not};
		print "You may wish to update your font path for ", join(' ', @l), "\n";
		return if $state->{not};
		run_if_exists($state, OpenBSD::Paths->mkfontscale, @l);
		run_if_exists($state, OpenBSD::Paths->mkfontdir, @l);

		map { restore_fontdir($_) } @l;

		run_if_exists($state, OpenBSD::Paths->fc_cache, @l);
	}
}


package OpenBSD::PackingElement::Mandir;
our @ISA=qw(OpenBSD::PackingElement::Dir);

sub keyword() { "mandir" }
__PACKAGE__->register_with_factory;
sub needs_keyword() { 1 }
sub dirclass() { "OpenBSD::PackingElement::Mandir" }

package OpenBSD::PackingElement::Extra;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

sub keyword() { 'extra' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state, 1);
}

sub dirclass() { "OpenBSD::PackingElement::Extradir" }

package OpenBSD::PackingElement::Extradir;
our @ISA=qw(OpenBSD::PackingElement::DirBase OpenBSD::PackingElement::Extra);

sub destate
{
	&OpenBSD::PackingElement::Extra::destate;
}

package OpenBSD::PackingElement::SpecialFile;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub exec_on_add { 0 }
sub exec_on_delete { 0 }

sub add_md5
{
	&OpenBSD::PackingElement::FileBase::add_md5;
}

sub add_size
{
	&OpenBSD::PackingElement::FileBase::add_size;
}

sub compute_md5
{
	&OpenBSD::PackingElement::FileObject::compute_md5;
}

sub write
{
	&OpenBSD::PackingElement::FileBase::write;
}

sub needs_keyword { 0 }

sub add_object
{
	my ($self, $plist) = @_;
	$self->{infodir} = $plist->{infodir};
	$self->SUPER::add_object($plist);
}

sub infodir
{
	my $self = shift;
	return ${$self->{infodir}};
}

sub stringize
{
	my $self = shift;
	return $self->category;
}

sub add
{
	my ($class, $plist, @args) = @_;

	$class->SUPER::add($plist, $class->category);
}

sub fullname
{
	my $self = shift;
	my $d = $self->infodir;
	if (defined $d) {
		return $d.$self->category;
	} else {
		return undef;
	}
}

package OpenBSD::PackingElement::FCONTENTS;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::CONTENTS }
# XXX we don't write `self'
sub write
{}

package OpenBSD::PackingElement::ScriptFile;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
use OpenBSD::Error;

sub exec_on_add { 1 }
sub exec_on_delete { 1 }

sub run
{
	my ($self, $state, @args) = @_;

	my $not = $state->{not};
	my $pkgname = $state->{pkgname};
	my $name = $self->fullname;

	return if $state->{dont_run_scripts};

	OpenBSD::PackingElement::Lib::ensure_ldconfig($state);
	print $self->beautify, " script: $name $pkgname ", join(' ', @args), "\n" if $state->{beverbose};
	return if $not;
	chmod 0755, $name;
	return if $state->system($name, $pkgname, @args) == 0;
	if ($state->{defines}->{scripts}) {
		$state->warn($self->beautify, " script failed\n");
	} else {
		$state->fatal($self->beautify." script failed");
	}
}

package OpenBSD::PackingElement::FCOMMENT;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::COMMENT }

package OpenBSD::PackingElement::FDESC;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::DESC }

package OpenBSD::PackingElement::FINSTALL;
our @ISA=qw(OpenBSD::PackingElement::ScriptFile);
sub exec_on_delete { 0 }
sub category() { OpenBSD::PackageInfo::INSTALL }
sub beautify() { "Install" }

package OpenBSD::PackingElement::FDEINSTALL;
our @ISA=qw(OpenBSD::PackingElement::ScriptFile);
sub exec_on_add { 0 }
sub category() { OpenBSD::PackageInfo::DEINSTALL }
sub beautify() { "Deinstall" }

package OpenBSD::PackingElement::FREQUIRE;
our @ISA=qw(OpenBSD::PackingElement::ScriptFile);
sub category() { OpenBSD::PackageInfo::REQUIRE }
sub beautify() { "Require" }

package OpenBSD::PackingElement::DisplayFile;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
use OpenBSD::Error;

sub prepare
{
	my ($self, $state) = @_;
	my $fname = $self->fullname;
	if (open(my $src, '<', $fname)) {
		while (<$src>) {
			next if m/^\+\-+\s*$/o;
			s/^[+-] //o;
			$state->print($_);
		} 
	} else {
		Warn "Can't open $fname: $!\n";
    	}
}

package OpenBSD::PackingElement::FDISPLAY;
our @ISA=qw(OpenBSD::PackingElement::DisplayFile);
sub category() { OpenBSD::PackageInfo::DISPLAY }

package OpenBSD::PackingElement::FUNDISPLAY;
our @ISA=qw(OpenBSD::PackingElement::DisplayFile);
sub category() { OpenBSD::PackageInfo::UNDISPLAY }

package OpenBSD::PackingElement::FMTREE_DIRS;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub category() { OpenBSD::PackageInfo::MTREE_DIRS }

package OpenBSD::PackingElement::Arch;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub category() { 'arch' }
sub keyword() { 'arch' }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my @arches= split(/\,/o, $args);
	bless { arches => \@arches }, $class;
}

sub stringize($)
{
	my $self = $_[0];
	return join(',', @{$self->{arches}});
}

sub check
{
	my ($self, $forced_arch) = @_;

	my ($machine_arch, $arch);
	for my $ok (@{$self->{arches}}) {
		return 1 if $ok eq '*';
		if (defined $forced_arch) {
			if ($ok eq $forced_arch) {
				return 1;
			} else {
				next;
			}
		}
		if (!defined $machine_arch) {
			my $cmd = OpenBSD::Paths->arch." -s";
			chomp($machine_arch = `$cmd`);
		}
		return 1 if $ok eq $machine_arch;
		if (!defined $arch) {
			my $cmd = OpenBSD::Paths->uname." -m";
			chomp($arch = `$cmd`);
		}
		return 1 if $ok eq $arch;
	}
	return;
}

package OpenBSD::PackingElement::DigitalSignature;
our @ISA=qw(OpenBSD::PackingElement::Unique);
sub keyword() { 'digital-signature' }
__PACKAGE__->register_with_factory;

# parse to and from a subset of iso8601
#
# allows us to represent timestamps in a human readable format without
# any ambiguity
sub time_to_iso8601
{
	my $time = shift;
	my ($sec, $min, $hour, $day, $month, $year, @rest) = gmtime($time);
	return sprintf("%04d-%02d-%02dT%02d:%02d:%02dZ", 
	    $year+1900, $month+1, $day, $hour, $min, $sec);
}

sub iso8601_to_time
{
	if ($_[0] =~ m/^(\d{4})\-(\d{2})\-(\d{2})T(\d{2})\:(\d{2})\:(\d{2})Z$/) {
		my ($year, $month, $day, $hour, $min, $sec) =
			($1 - 1900, $2-1, $3, $4, $5, $6);
		require POSIX;
		my $oldtz = $ENV{TZ};
		$ENV{TZ} = 'UTC';
		my $t = POSIX::mktime($sec, $min, $hour, $day, $month, $year);
		if (defined $oldtz) {
			$ENV{TZ} = $oldtz;
		} else {
			delete $ENV{TZ};
		}
		return $t;
	} else {
		die "Incorrect ISO8601 timestamp: $_[0]";
	}
}

sub new
{
	my ($class, $args) = @_;
	my ($key, $tsbase, $tsmin, $tssec, $signature) = split(/\:/, $args);
	my $timestamp = iso8601_to_time("$tsbase:$tsmin:$tssec");
	bless { key => $key, timestamp => $timestamp, b64sig => $signature },
		$class;
}


sub stringize
{
	my $self = shift;
	return join(':', $self->{key}, time_to_iso8601($self->{timestamp}), 
	    $self->{b64sig});
}

sub write_no_sig
{
	my ($self, $fh) = @_;
	print $fh "\@", $self->keyword, " ", $self->{key}, ":", 
	    $self->{timestamp}, "\n";
}

package OpenBSD::PackingElement::Old;
our @ISA=qw(OpenBSD::PackingElement);

my $warned;

sub new
{
	my ($class, $k, $args) = @_;
	bless { keyword => $k, name => $args }, $class;
}

sub add
{
	my ($o, $plist, $args) = @_;
	my $keyword = $$o;
	if (!$warned->{$keyword}) {
		print STDERR "Warning: obsolete construct: \@$keyword $args\n";
		$warned->{$keyword} = 1;
	}
	my $o2 = OpenBSD::PackingElement::Old->new($keyword, $args);
	$o2->add_object($plist);
	$plist->{deprecated} = 1;
}

sub keyword
{
	my $self = shift;
	return $self->{keyword};
}

sub register_old_keyword
{
	my ($class, $k) = @_;
	$class->register_with_factory($k, bless \$k, $class);
}

for my $k (qw(src display mtree ignore_inst dirrm pkgcfl pkgdep newdepend 
    libdepend ignore)) {
	__PACKAGE__->register_old_keyword($k);
}

1;
