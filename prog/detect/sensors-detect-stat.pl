#!/usr/bin/perl -wT

# sensors-detect-stat.pl
# Statistical analysis of sensors-detect i2c addresses scanner
# Part of the lm_sensors project
# Copyright (C) 2003-2004  Jean Delvare <khali@linux-fr.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

use strict;
use vars qw(%histo $chips $file);

# Where is sensors-detect?
# Use first argument, else try default locations.
if (defined($ARGV[0]))
{
	$file = $ARGV[0];
}
elsif (-r '/usr/local/sbin/sensors-detect')
{
	$file = '/usr/local/sbin/sensors-detect';
}
elsif (-r '/usr/sbin/sensors-detect')
{
	$file = '/usr/sbin/sensors-detect';
}
else
{
	print "Usage: $0 /path/to/sensors-detect\n";
	exit 1;
}

# Can we read that file?
if (! -r $file)
{
	print "Couldn't open $file for reading.\n";
	exit 2;
}

# Get the data.
open (SD, $file) || die;
while (<SD>)
{
	# The regular expression may seem a little bit complex, but we wouldn't
	# want to exec malicious code.
	next unless m/^\s*i2c_addrs\s*=>\s*(\[( *0x[\dA-Fa-f]{2}( *\.\. *0x[\dA-Fa-f]{2})? *,?)+\])\s*,/;
	my $addresses = eval $1 || die "Failed to eval \"$1\"";
	$chips++;
	foreach my $a (@{$addresses})
	{
		$histo{$a}++;
	}
}
close SD;

# Print the data.
printf "$file knows \%d chips and scans \%d addresses.\n\n",
	$chips, scalar keys %histo;
print "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n";
for (my $i=0; $i<128; $i+=16)
{
	printf '%02x:', $i;
	for (my $j=0; $j<16; $j++)
	{
		my $valid = ($i+$j >= 0x04 && $i+$j <= 0x77);
		if (defined $histo{$i+$j})
		{
			printf '%s%02d', ($valid?' ':'!'), $histo{$i+$j};
		}
		else
		{
			printf ' %s', ($valid?'--':'  ');
		}
	}
	print "\n";
}
