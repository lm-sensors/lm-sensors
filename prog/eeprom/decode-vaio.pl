#!/usr/bin/perl -w
#
# Copyright (C) 2002-2005 Jean Delvare <khali@linux-fr.org>
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
# Version 0.1  2002-02-06  Jean Delvare <khali@linux-fr.org>
# Version 0.2  2002-02-16  Jean Delvare <khali@linux-fr.org>
#  Fixed to work with the new, simplified /proc interface names of the eeprom
#    driver (lm_sensors 2.6.3 and greater).
#  Shifted data display by 4 columns left.
# Version 0.3  2002-02-17  Jean Delvare <khali@linux-fr.org>
#  Added UUID field at 0x10 (added decode_uuid).
#  Merged decode_string and decode_string32.
#  Added unknown field at 0x20.
#  Moved header and footer to BEGIN and END, respectively.
#  Reformated history to match those of the other decode scripts.
#  Deleted decode_char (made useless by decode_string).
#  Reordered field display, changed some labels.
#  Added old /proc interface check.
# Version 1.0  2002-11-15  Jean Delvare <khali@linux-fr.org>
#  Gave the label "OEM Data" to the field at 0x20.
#  Gave the label "Timestamp" to the field at 0xE0.
#  Renamed "Model Number" to "Model Name".
#  Added some documentation.
# Version 1.1  2004-01-17  Jean Delvare <khali@linux-fr.org>
#  Added support for Linux 2.5/2.6 (i.e. sysfs).
# Version 1.2  2004-11-28  Jean Delvare <khali@linux-fr.org>
#  Support bus number 0 to 4 instead of only 0.
# Version 1.3  2005-01-18  Jean Delvare <khali@linux-fr.org>
#  Revision might be a Service Tag.
#
# EEPROM data decoding for Sony Vaio laptops. 
#
# Two assumptions: lm_sensors-2.6.3 or greater installed,
# and Perl is at /usr/bin/perl
#
# Please note that this is a guess-only work.  Sony support refused to help
# me, so if someone can provide information, please contact me.
# My knowledge is summarized on this page:
# http://khali.linux-fr.org/vaio/eeprom.html
#
# It seems that if present, the EEPROM is always at 0x57.
#
# Models tested so far:
#   PCG-F403     : No EEPROM
#   PCG-F707     : No EEPROM
#   PCG-GR114EK  : OK
#   PCG-GR114SK  : OK
#   PCG-GR214EP  : OK
#   PCG-GRX316G  : OK
#   PCG-GRX570   : OK
#   PCG-GRX600K  : OK
#   PCG-U1       : OK
#   PCG-Z600LEK  : No EEPROM
#   PCG-Z600NE   : No EEPROM
#   VGN-S260     : OK
# Any feedback appreciated anyway.
#
# Thanks to Werner Heuser, Carsten Blume, Christian Gennerat, Joe Wreschnig,
# Xavier Roche, Sebastien Lefevre, Lars Heer, Steve Dobson, Kent Hunt and
# others for their precious help.


use strict;
use Fcntl qw(:DEFAULT :seek);
use vars qw($sysfs $found);

sub print_item
{
	my ($label,$value) = @_;
	
	printf("\%16s : \%s\n",$label,$value);
}

# Abstract reads so that other functions don't have to care wether
# we need to use procfs or sysfs
sub read_eeprom_bytes
{
	my ($bus, $addr, $offset, $length) = @_;
	my $filename;
	
	if ($sysfs)
	{
		$filename = "/sys/bus/i2c/devices/$bus-00$addr/eeprom";
		sysopen(FH, $filename, O_RDONLY)
			or die "Can't open $filename";
		sysseek(FH, $offset, SEEK_SET)
			or die "Can't seek in $filename";

		my ($r, $bytes);
		$bytes = '';
		$offset = 0;
		while($length)
		{
			$r = sysread(FH, $bytes, $length, $offset);
			die "Can't read $filename"
				unless defined($r);
			die "Unexpected EOF in $filename"
				unless $r;
			$offset += $r;
			$length -= $r;
		}
		close(FH);
		
		return $bytes;
	}
	else
	{
		my $base = $offset & 0xf0;
		$offset -= $base;
		my $values = '';
		my $remains = $length + $offset;
		
		# Get all lines in a single string
		while ($remains > 0)
		{
			$filename = "/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/"
			          . sprintf('%02x', $base);
			open(FH, $filename)
				or die "Can't open $filename";
			$values .= <FH>;
			close(FH);
			$remains -= 16;
			$base += 16;
		}
		
		# Store the useful part in an array
		my @bytes = split(/[ \n]/, $values);
		@bytes = @bytes[$offset..$offset+$length-1];

		# Back to a binary string
		return pack('C*', @bytes);
	}
}

sub decode_string
{
	my ($bus, $addr, $offset, $length) = @_;

	my $string = read_eeprom_bytes($bus, $addr, $offset, $length);
	$string =~ s/\x00.*$//;
	
	return($string);
}

sub decode_uuid
{
	my ($bus,$addr,$base) = @_;

	my @bytes = unpack('C16', read_eeprom_bytes($bus, $addr, $base, 16));
	my $string='';

	for(my $i=0;$i<16;$i++)
	{
		$string.=sprintf('%02x',shift(@bytes));
		if(($i==3)||($i==5)||($i==7)||($i==9))
		{
			$string.='-';
		}
	}

	return($string);
}

sub vaio_decode
{
	my ($bus,$addr) = @_;
	
	print_item('Machine Name', decode_string($bus, $addr, 128, 32));
	print_item('Serial Number', decode_string($bus, $addr, 192, 32));
	print_item('UUID', decode_uuid($bus, $addr, 16));
	my $revision = decode_string($bus, $addr, 160, 10);
	print_item(length($revision) > 2 ? 'Service Tag' : 'Revision',
		   $revision);
	print_item('Model Name', 'PCG-'.decode_string($bus, $addr, 170, 4));
	print_item('OEM Data', decode_string($bus, $addr, 32, 16));
	print_item('Timestamp', decode_string($bus, $addr, 224, 32));
}

BEGIN
{
	print("Sony Vaio EEPROM Decoder\n");
	print("Copyright (C) 2002-2005  Jean Delvare\n");
	print("Version 1.3\n\n");
}

END
{
	print("\n");
}

for (my $i = 0, $found=0; $i <= 4 && !$found; $i++)
{
	if (-r "/sys/bus/i2c/devices/$i-0057/eeprom")
	{
		$sysfs = 1;
		vaio_decode($i, '57');
		$found++;
	}
	elsif (-r "/proc/sys/dev/sensors/eeprom-i2c-$i-57")
	{
		if (-r "/proc/sys/dev/sensors/eeprom-i2c-$i-57/data0-15")
		{
			print("Deprecated old interface found.  Please upgrade to lm_sensors 2.6.3 or greater.");
			exit;
		}
		else
		{
			$sysfs = 0;
			vaio_decode($i, '57');
			$found++;
		}
	}
}

if (!$found)
{
	print("Vaio EEPROM not found.  Please make sure that the eeprom module is loaded.\n");
	print("If you believe this is an error, please contact me <khali\@linux-fr.org>\n");
	print("so that we may see how to fix the problem.\n");
}
