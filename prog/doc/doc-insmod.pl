#!/usr/bin/perl -w

#
#    doc-insmod.pl - Generate module parameters documentation
#    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
#    Copyright (c) 2002  Jean Delvare <khali@linux-fr.org>
#
#    Known to work with Linux modutils 2.3.10-pre1, 2.4.6, 2.4.15, 2.4.16
#    and 2.4.19.
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#


use strict;
use Text::Wrap;
use vars qw($modinfo);

$modinfo = 'modinfo';
$ENV{PATH} = '/sbin';

sub get_modinfo_version
{
	my $line = `$modinfo -V`;
	
	return undef unless $line =~ m/\b(\d+)\.(\d+)\.(\d+)/;
	return ($1<<16)+($2<<8)+$3;
}

# @_[0]: name or full path of the kernel module
sub print_modinfo
{
	my $modname = shift;
	my $version = get_modinfo_version();
	
	unless ($modname =~ m/^([\w\d.\/-]+)$/)
	{
		print STDERR "Invalid module name.\n";
		return;
	}
	$modname = $1;
	
	my ($author, $license);
	$author = `$modinfo -a $modname`;
	$author =~ s/\n\s+/ /mg; # handle multiline authors
	$author =~ s/^"//;
	$author =~ s/"$//;
	if (defined $version and $version >= (2<<16)+(4<<8)+15)
	{
		$license = `$modinfo -l $modname`;
		$license =~ s/^"//;
		$license =~ s/"$//;
	}
	
	print wrap('Author: ', '        ', $author), "\n";
	print wrap('License: ', '         ', $license), "\n"
		if defined $license;
	print "\n", "Module Parameters\n", "-----------------\n", "\n";

	my $lines = 0;
	open OPTIONS, "$modinfo -p $modname |";
	while (<OPTIONS>)
	{
		next unless m/^(parm:\s*)?(\S+) (.+), description "(.*)"$/;
		print "* $2: $3\n",
			wrap('  ', '  ', $4), "\n";
		$lines++;
	}
	close OPTIONS;
	print "(none)\n"
		unless $lines;
}

if (@ARGV != 1 or $ARGV[0] =~ /^-/)
{
	print "Usage: $0 MODULE\n",
		"  MODULE is a module name or the full path to a module file.\n";
	exit 0;
}

print_modinfo($ARGV[0]);
