#!/usr/bin/perl -w
#
# Copyright 2003 Jean Delvare <khali@linux-fr.org>
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
# Version 0.1  2003-07-17  Jean Delvare <khali@linux-fr.org>
# Version 0.2  2003-07-22  Jean Delvare <khali@linux-fr.org>
#  Use print instead of syswrite.
# Version 0.3  2003-08-24  Jean Delvare <khali@linux-fr.org>
#  Fix data block length (128 bytes instead of 256).
#
# EEPROM data decoding for EDID. EDID (Extended Display Identification
# Data) is a VESA standard which allows storing (on manufacturer's side)
# and retrieving (on user's side) of configuration information about
# displays, such as manufacturer, serial number, physical dimensions and
# allowed horizontal and vertical refresh rates.
#
# Using the LM Sensors modules and tools, you have two possibilities to
# make use of these data:
#  1* Use the ddcmon driver and run sensors.
#  2* Use the eeprom driver and run this script.
# Both solutions will return a different kind of information. The first
# method will report user-interesting information, such as the model number
# or the year of manufacturing. The second method will report video-card-
# interesting information, such as video modes and refresh rates.
#
# Note that this script does almost nothing by itself. It simply converts
# what it finds in /proc to binary data to feed the parse-edid program.
# The parse-edid program was written by John Fremlin and is available at
# the following address:
#   http://john.fremlin.de/programs/linux/read-edid/

use strict;

use vars qw($bus $address);

sub edid_valid
{
	my ($bus, $addr) = @_;

	open EEDATA, "/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/00";
	my $line = <EEDATA>;
	close EEDATA;
	return 1
		if $line =~ m/^0 255 255 255 255 255 255 0 /;
	return 0;
}

sub bus_detect
{
	my $max = shift;

	for (my $i=0; $i<$max; $i++)
	{
		if ( -r "/proc/sys/dev/sensors/eeprom-i2c-$i-50/00" )
		{
			if (edid_valid ($i, '50'))
			{
				print STDERR
					"decode-edid: using bus $i (autodetected)\n";
				return $i;
			}
		}
	}
	
	return; # default
}

sub edid_decode
{
	my ($bus, $addr) = @_;

	# Make sure it is an EDID EEPROM.

	unless (edid_valid ($bus, $addr))
	{
		print STDERR
			"decode-edid: not an EDID EEPROM at $bus-$addr\n";
		return;
	}

	$SIG{__WARN__} = sub { };
	open PIPE, "| parse-edid"
		or die "Can't open parse-edid. Please install read-edid.\n";
	delete $SIG{__WARN__};
	binmode PIPE;
	
	for (my $i=0; $i<=0x70; $i+=0x10)
	{
		my $file = sprintf '%02x', $i;
		my $output = '';
		open EEDATA, "/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/$file"
			or die "Can't read /proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/$file";
		while(<EEDATA>)
		{
			foreach my $item (split)
			{
				$output .= pack "C", $item;
			}
		}
		close EEDATA;
		print PIPE $output;
	}

	close PIPE;
}

# Get the address. Default to 0x50 if not given.
$address = $ARGV[1] || 0x50;
# Convert to decimal, whatever the value.
$address = oct $address if $address =~ m/^0/;
# Convert to an hexadecimal string.
$address = sprintf '%02x', $address;

# Get the bus. Try to autodetect if not given.
$bus = $ARGV[0] if defined $ARGV[0];
$bus = bus_detect (8) unless defined $bus;

if ( defined $bus
  && -r "/proc/sys/dev/sensors/eeprom-i2c-$bus-$address" )
{
	print STDERR
		"decode-edid: decode-edid version 0.3\n";
	edid_decode ($bus, $address);
}
else
{
	print STDERR
		"EDID EEPROM not found.  Please make sure that the eeprom module is loaded.\n";
	print STDERR
		"Maybe your EDID EEPROM is on another bus.  Try \"decode-edid ".($bus+1)."\".\n"
		if defined $bus;
}
