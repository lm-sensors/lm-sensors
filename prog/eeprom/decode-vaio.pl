#!/usr/bin/perl -w
#
# Copyright 2002 Jean Delvare <khali@linux-fr.org>
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
#  Added some Documentation.
#
# EEPROM data decoding for Sony Vaio laptops. 
#
# Two assumptions: lm_sensors-2.6.3 or greater installed,
# and Perl is at /usr/bin/perl
#
# Please note that this is a guess-only work.  Sony support refused to help
# me, so if someone can provide information, please contact me.
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
#   PCG-Z600LEK  : No EEPROM
#   PCG-Z600NE   : No EEPROM
# Any feedback appreciated anyway.
#
# Thanks to Werner Heuser, Carsten Blume, Christian Gennerat, Joe Wreschnig,
# Xavier Roche, Sebastien Lefevre, Philippe H., Lars Heer and Steve Dobson
# for their precious help.
#

use strict;

sub print_item
{
	my ($label,$value) = @_;
	
	printf("\%16s : \%s\n",$label,$value);
}

sub decode_string
{
	my ($bus,$addr,$base,$offset,$length) = @_;

	my $line='';
	my $remains=$length+$offset;
	while($remains>0)
	{
		my $filename="/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/".sprintf('%02x',$base);
		open(FH,$filename) || die "Can't open $filename";
		$line.=<FH>;
		close(FH);
		$remains-=16;
		$base+=16;
	}

	my @bytes=split(/[ \n]/,$line);
	@bytes=@bytes[$offset..$offset+$length-1];
	my $string='';
	my $item;
	while(defined($item=shift(@bytes)) && ($item!=0))
	{
		$string.=chr($item);
	}
	
	return($string);
}

sub decode_uuid
{
	my ($bus,$addr,$base) = @_;

	my $filename="/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/".sprintf('%02x',$base);
	open(FH,$filename) || die "Can't open $filename";
	my $line=<FH>;
	close(FH);

	my @bytes=split(/[ \n]/,$line);
	my $string='';
	my $item;

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
	
	print_item('Machine Name',decode_string($bus,$addr,128,0,32));
	print_item('Serial Number',decode_string($bus,$addr,192,0,32));
	print_item('UUID',decode_uuid($bus,$addr,16));
	print_item('Revision',decode_string($bus,$addr,160,0,10));
	print_item('Model Name','PCG-'.decode_string($bus,$addr,160,10,4));
	print_item('OEM Data',decode_string($bus,$addr,32,0,16));
	print_item('Timestamp',decode_string($bus,$addr,224,0,32));
}

BEGIN
{
	print("Sony Vaio EEPROM Decoder\n");
	print("Written by Jean Delvare.  Copyright 2002.\n");
	print("Version 1.0\n\n");
}

END
{
	print("\n");
}

if ( -r '/proc/sys/dev/sensors/eeprom-i2c-0-57')
{
	if ( -r '/proc/sys/dev/sensors/eeprom-i2c-0-57/data0-15')
	{
		print("Deprecated old interface found.  Please upgrade to lm_sensors 2.6.3 or greater.");
	}
	else
	{
		vaio_decode('0','57');
	}
}
else
{
	print("Vaio EEPROM not found.  Please make sure that the eeprom module is loaded.\n");
	print("If you believe this is an error, please contact me <khali\@linux-fr.org>\n");
	print("so that we may see how to fix the problem.\n");
}
