# ex:ts=8 sw=4:
# $OpenBSD: Dependencies.pm,v 1.64 2008/06/21 14:01:10 espie Exp $
#
# Copyright (c) 2005-2007 Marc Espie <espie@openbsd.org>
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

# generic dependencies lookup class: walk the dependency tree as far
# as necessary to resolve dependencies
package OpenBSD::lookup;

sub lookup
{
	my ($self, $solver, $state, $obj) = @_;

	my $dependencies = $solver->{to_register};
	my $known = $self->{known};
	if (my $r = $self->find_in_already_done($solver, $state, $obj)) {
		$dependencies->{$r} = 1;
		return 1;
	}
	if ($self->find_in_extra_sources($solver, $state, $obj)) {
		return 1;
	}
	# lookup through the rest of the tree...
	my $done = $self->{done};
	while (my $dep = pop @{$self->{todo}}) {
		require OpenBSD::RequiredBy;

		next if $done->{$dep};
		$done->{$dep} = 1;
		for my $dep2 (OpenBSD::Requiring->new($dep)->list) {
			push(@{$self->{todo}}, $dep2) unless $done->{$dep2};
		}
		$known->{$dep} = 1;
		if ($self->find_in_new_source($solver, $state, $obj, $dep)) {
			$dependencies->{$dep} = 1;
			return 1;
		}
	}
	if (my $r = $self->find_elsewhere($solver, $state, $obj)) {
		$dependencies->{$r} = 1;
		return 1;
	}
	
	return 0;
}

sub new
{
	my ($class, $solver) = @_;

	# prepare for closure
	my @todo = $solver->dependencies;
	bless { todo => \@todo, done => {}, known => {} }, $class;
}

sub dump
{
	my $self = shift;

	return unless %{$self->{done}};
	print "Full dependency tree is ", join(',', keys %{$self->{done}}), 
	    "\n";
}

package OpenBSD::lookup::library;
our @ISA=qw(OpenBSD::lookup);

sub find_in_already_done
{
	my ($self, $solver, $state, $obj) = @_;


	my $r = $solver->check_lib_spec($solver->{plist}->localbase, $obj, 
	    $self->{known});
	if ($r) {
		print "found libspec $obj in package $r\n" if $state->{verbose};
		return $r;
	} else {
		return undef;
	}
}

sub find_in_extra_sources
{
	my ($self, $solver, $state, $obj) = @_;
	return undef if $obj =~ m/\//;

	OpenBSD::SharedLibs::add_libs_from_system($state->{destdir});
	for my $dir (OpenBSD::SharedLibs::system_dirs()) {
		if ($solver->check_lib_spec($dir, $obj, {system => 1})) {
			print "found libspec $obj in $dir/lib\n" if $state->{verbose};
			return 'system';
		}
	}
	return undef;
}

sub find_in_new_source
{
	my ($self, $solver, $state, $obj, $dep) = @_;
	OpenBSD::SharedLibs::add_libs_from_installed_package($dep);
	if ($solver->check_lib_spec($solver->{plist}->localbase, $obj, 
	    {$dep => 1})) {
		print "found libspec $obj in package $dep\n" if $state->{verbose};
		return $dep;
	} 
	return undef;
}

sub find_elsewhere
{
	my ($self, $state, $solver, $obj) = @_;

	for my $dep (@{$solver->{plist}->{depend}}) {
		my $r = $solver->find_old_lib($state, 
		    $solver->{plist}->localbase, $dep->{pattern}, $obj);
		if ($r) {
			print "found libspec $obj in old package $r\n" if $state->{verbose};
			return $r;
		}
    	}
	return undef;
}

package OpenBSD::lookup::tag;
our @ISA=qw(OpenBSD::lookup);
sub find_in_extra_sources
{
}

sub find_elsewhere
{
}

sub find_in_already_done
{
	my ($self, $solver, $state, $obj) = @_;
	my $r = $self->{known_tags}->{$obj};
	if (defined $r) {
		print "Found tag $obj in $r\n" if $state->{verbose};
	}
	return $r;
}

sub find_in_plist
{
	my ($self, $plist, $dep) = @_;
	if ($plist->has('define-tag')) {
		for my $t (@{$plist->{'define-tag'}}) {
			$self->{known_tags}->{$t->{name}} = $dep;
		}
	}
}

sub find_in_new_source
{
	my ($self, $solver, $state, $obj, $dep) = @_;
	my $plist = OpenBSD::PackingList->from_installation($dep,
	    \&OpenBSD::PackingList::DependOnly);
	if (!defined $plist) {
		print STDERR "Can't read plist for $dep\n";
	}
	$self->find_in_plist($plist, $dep);
	return $self->find_in_already_done($solver, $state, $obj);
}

package OpenBSD::Dependencies::Solver;

use OpenBSD::PackageInfo;

sub find_candidate
{
	    my $spec = shift;
	    my @candidates = $spec->filter(@_);
	    if (@candidates >= 1) {
		    return $candidates[0];
	    } else {
		    return undef;
	    }
}

sub new
{
	my ($class, $set) = @_;
	bless {set => $set, plist => $set->handle->{plist}, 
	    to_install => {}, deplist => [], to_register => {} }, $class;
}

sub dependencies
{
	my $self = shift;
	if (wantarray) {
		return keys %{$self->{to_register}};
	} else {
		return scalar(%{$self->{to_register}});
	}
}

sub has_dep
{
	my ($self, $dep) = @_;
	return $self->{to_register}->{$dep};
}

sub pkgname
{
	my $self = shift;
	return $self->{plist}->pkgname;
}

sub add_todo
{
	my ($self, @extra) = @_;

	require OpenBSD::PackageName;

	for my $set (@extra) {
		my $fullname = $set->handle->{pkgname};
		$self->{to_install}->
		    {OpenBSD::PackageName::url2pkgname($fullname)} = $set;
	}
}

sub find_dep_in_repositories
{
	my ($self, $state, $dep) = @_;
	require OpenBSD::PackageLocator;

	my @candidates = OpenBSD::PackageLocator->match_locations($dep->spec);
	# XXX not really efficient, but hey
	my %c = map {($_->{name}, $_)} @candidates;
	my @pkgs = keys %c;
	if (!$state->{defines}->{allversions}) {
		@pkgs = OpenBSD::PackageName::keep_most_recent(@pkgs);
	}
	if (@pkgs == 1) {
		return $c{$pkgs[0]};
	} elsif (@pkgs > 1) {
		require OpenBSD::Interactive;

		# put default first if available
		@pkgs = ((grep {$_ eq $dep->{def}} @pkgs),
		    (sort (grep {$_ ne $dep->{def}} @pkgs)));
		my $good =  OpenBSD::Interactive::ask_list(
		    'Ambiguous: choose dependency for '.$self->pkgname.': ',
		    $state->{interactive}, @pkgs);
		return $c{$good};
	} else {
		return;
	}
}

sub find_dep_in_stuff_to_install
{
	my ($self, $state, $dep) = @_;

	return find_candidate($dep->spec, keys %{$self->{to_install}});
}

sub solve_dependency
{
	my ($self, $state, $dep) = @_;

	my $v;

	if ($state->{allow_replacing}) {
		$v = $self->find_dep_in_stuff_to_install($state, $dep);
		if ($v) {
			push(@{$self->{deplist}}, $self->{to_install}->{$v});
			return $v;
		}
	}

	$v = find_candidate($dep->spec, installed_packages());
	if ($v) {
		return $v;
	}
	if (!$state->{allow_replacing}) {
		$v = $self->find_dep_in_stuff_to_install($state, $dep);
		if ($v) {
			push(@{$self->{deplist}}, $self->{to_install}->{$v});
			return $v;
		}
	}

	$v = $self->find_dep_in_repositories($state, $dep);
	if ($v) {
		push(@{$self->{deplist}}, 
		    OpenBSD::UpdateSet->from_location($v->openPackage));
		return $v->{name};
	}

	# resort to default if nothing else
	$v = $dep->{def};
	push(@{$self->{deplist}}, OpenBSD::UpdateSet->create_new($v));
	return $v;
}

sub solve_depends
{
	my ($self, $state, @extra) = @_;

	$self->add_todo(@extra);

	for my $dep (@{$self->{plist}->{depend}}) {
		my $v = $self->solve_dependency($state, $dep);
		$self->{to_register}->{$v} = $dep;
	}

	return @{$self->{deplist}};
}

sub check_depends
{
	my $self = shift;
	my @bad = ();

	for my $dep ($self->dependencies) {
		push(@bad, $dep) unless is_installed($dep);
	}
	return @bad;
}

sub dump
{
	my $self = shift;
	if ($self->dependencies) {
	    print "Dependencies for ", $self->pkgname, " resolve to: ", 
	    	join(', ',  $self->dependencies);
	    print " (todo: ", 
	    	join(',', (map {$_->handle->{pkgname}} @{$self->{deplist}})), 
		")" 
	    	if @{$self->{deplist}} > 0;
	    print "\n";
	}
}

sub register_dependencies
{
	my ($self, $state) = @_;

	require OpenBSD::RequiredBy;
	my $pkgname = $self->pkgname;
	my @l = $self->dependencies;

	OpenBSD::Requiring->new($pkgname)->add(@l);
	for my $dep (@l) {
		OpenBSD::RequiredBy->new($dep)->add($pkgname);
	}
	delete $self->{toregister};
	delete $self->{deplist};
}

sub record_old_dependencies
{
	my ($self, $state) = @_;
	for my $o ($self->{set}->older_to_do) {
		require OpenBSD::RequiredBy;
		my @wantlist = OpenBSD::RequiredBy->new($o->{pkgname})->list;
		$o->{wantlist} = \@wantlist;
	}
}

sub adjust_old_dependencies
{
	my ($self, $state) = @_;
	my $pkgname = $self->{set}->handle->{pkgname};
	for my $o ($self->{set}->older) {
		next unless defined $o->{wantlist};
		require OpenBSD::Replace;
		require OpenBSD::RequiredBy;

		my $oldname = $o->{pkgname};

		print "Adjusting dependencies for $pkgname/$oldname\n" 
		    if $state->{beverbose};
		my $d = OpenBSD::RequiredBy->new($pkgname);
		for my $dep (@{$o->{wantlist}}) {
			if (defined $self->{set}->{skipupdatedeps}->{$dep}) {
				print "\tskipping $dep\n" if $state->{beverbose};
				next;
			}
			print "\t$dep\n" if $state->{beverbose};
			$d->add($dep);
			OpenBSD::Replace::adjust_dependency($dep, $oldname, $pkgname);
		}
	}
}

sub repair_dependencies
{
	my ($self, $state) = @_;
	my $pkgname = $self->{set}->handle->{pkgname};
	for my $pkg (installed_packages(1)) {
		my $plist = OpenBSD::PackingList->from_installation($pkg, 
		    \&OpenBSD::PackingList::DependOnly);
	    	$plist->repair_dependency($pkg, $pkgname);
	}
}

use OpenBSD::SharedLibs;

sub check_lib_spec
{
	my ($self, $base, $spec, $dependencies) = @_;
	my @r = OpenBSD::SharedLibs::lookup_libspec($base, $spec);
	for my $candidate (@r) {
		if ($dependencies->{$candidate}) {
			return $candidate;
		}
	}
	return;
}

sub find_old_lib
{
	my ($self, $state, $base, $pattern, $lib) = @_;

	require OpenBSD::Search;
	require OpenBSD::PackageRepository::Installed;

	for my $try (OpenBSD::PackageRepository::Installed->new->match(OpenBSD::Search::PkgSpec->new(".libs-".$pattern))) {
		OpenBSD::SharedLibs::add_libs_from_installed_package($try);
		if ($self->check_lib_spec($base, $lib, {$try => 1})) {
			return $try;
		}
	}
	return undef;
}

sub solve_wantlibs
{
	my ($solver, $state) = @_;
	my $okay = 1;

	my $lib_finder = OpenBSD::lookup::library->new($solver);
	for my $h ($solver->{set}->newer) {
		for my $lib (@{$h->{plist}->{wantlib}}) {
			next if $lib_finder->lookup($solver, $state, 
			    $lib->{name});
			OpenBSD::Error::Warn "Can't install ", 
			    $h->{pkgname}, ": lib not found ", 
			    $lib->{name}, "\n";
			if ($okay) {
				$solver->dump;
				$lib_finder->dump;
				$okay = 0;
			}
			OpenBSD::SharedLibs::report_problem(
			    $state->{localbase}, $lib->{name});
		}
	}
	return $okay;
}

sub solve_tags
{
	my ($solver, $state) = @_;
	my $okay = 1;

	my $tag_finder = OpenBSD::lookup::tag->new($solver);
	for my $h ($solver->{set}->newer) {
		for my $tag (keys %{$h->{plist}->{tags}}) {
			next if $tag_finder->lookup($solver, $state, $tag);
			OpenBSD::Error::Warn "Can't install ", 
			    $h->{pkgname}, ": tag definition not found ", 
			    $tag, "\n";
			if ($okay) {
				$solver->dump;
				$tag_finder->dump;
				$okay = 0;
			}
	    	}
	}
	return $okay;
}

package OpenBSD::PackingElement;
sub repair_dependency
{
}

package OpenBSD::PackingElement::Dependency;
sub repair_dependency
{
	my ($self, $requiring, $required) = @_;
	if ($self->spec->filter($required) == 1) {
		require OpenBSD::RequiredBy;
		OpenBSD::RequiredBy->new($required)->add($requiring);
		OpenBSD::Requiring->new($requiring)->add($required);
	}
}

1;
