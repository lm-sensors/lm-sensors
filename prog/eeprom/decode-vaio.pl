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
#  Fixed to work with the new, simplified /proc interface names of the eeprom driver
#  (lm_sensors 2.6.3 and greater.)
#  Shifted data display by 4 columns left.
# Version 0.3  2002-02-17  Jean Delvare <khali@linux-fr.org>
#  Added UUID field at 0x10 (added decode_uuid.)
#  Merged decode_string and decode_string32.
#  Added unknown field at 0x20.
#  Moved header and footer to BEGIN and END, respectivly.
#  Reformated history to match those of the other decode scripts.
#  Deleted decode_char (made useless by decode_string.)
#  Reordered field display, changed some labels.
#  Added old /proc interface check.
#
# EEPROM data decoding for Sony Vaio laptops. 
#
# Two assumptions: lm_sensors-2.6.3 or greater installed,
# and Perl is at /usr/bin/perl
#
# Please note that this is a guess-only work.  Sony support refused to help
# me, so if someone can provide information, please contact me.  I used my
# PCG-GR214EP as a base, but I can't promise that this script will work with
# other models.  Any feedback appreciated anyway.
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
	print_item('Model Number','PCG-'.decode_string($bus,$addr,160,10,4));
	print_item('?',decode_string($bus,$addr,32,0,16));
	print_item('?',decode_string($bus,$addr,224,0,32));
}

BEGIN
{
	print("Sony Vaio EEPROM Decoder\n");
	print("Written by Jean Delvare.  Copyright 2002.\n");
	print("Version 0.3\n\n");
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
