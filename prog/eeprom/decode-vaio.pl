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
#
# EEPROM data decoding for Sony Vaio laptops. 
#
# Two assumptions: lm_sensors-2.x installed,
# and Perl is at /usr/bin/perl
#
# Please note that this is a guess-only work.  I don't expect much help from
# Sony, so if someone can provide information, please contact me.  I used my
# PCG-GR214EP as a base, but I can't promise that this script will work with
# other models.  Any feedback appreciated anyway.
#

use strict;

sub print_item
{
	my ($label,$value) = @_;
	
	printf("\%20s : \%s\n",$label,$value);
}

sub decode_char
{
	my ($bus,$addr,$base,$offset,$length) = @_;
	
	my $filename="/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/data$base-".($base+15);
	open(FH,$filename) || die "Can't open $filename";
	my $line=<FH>;
	close(FH);
	
	my @bytes=split(/[ \n]/,$line);
	@bytes=@bytes[$offset..$offset+$length-1];
	my $string='';
	my $item;
	while(defined($item=shift(@bytes)))
	{
		$string.=chr($item);
	}
	
	return($string);
}

sub decode_string
{
	my ($bus,$addr,$base,$offset,$length) = @_;

	my $filename="/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/data$base-".($base+15);
	open(FH,$filename) || die "Can't open $filename";
	my $line=<FH>;
	close(FH);

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

sub decode_string32
{
	my ($bus,$addr,$base) = @_;

	my $filename="/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/data$base-".($base+15);
	open(FH,$filename) || die "Can't open $filename";
	my $line=<FH>;
	close(FH);
	$filename="/proc/sys/dev/sensors/eeprom-i2c-$bus-$addr/data".($base+16).'-'.($base+31);
	open(FH,$filename) || die "Can't open $filename";
	$line.=<FH>;
	close(FH);

	my @bytes=split(/[ \n]/,$line);
	my $string='';
	my $item;
	while(defined($item=shift(@bytes)) && ($item!=0))
	{
		$string.=chr($item);
	}
	
	return($string);
}

sub vaio_decode
{
	my ($bus,$addr) = @_;
	
	print_item('Model name',decode_string32($bus,$addr,128).' ['.decode_char($bus,$addr,160,10,4).']');
	print_item('Revision',decode_string($bus,$addr,160,0,10));
	print_item('Serial number',decode_string32($bus,$addr,192));
	print_item('Timestamp',decode_string32($bus,$addr,224));
}

print("Sony Vaio EEPROM Decoder\n");
print("Written by Jean Delvare.  Copyright 2002.\n");
print("Version 0.1\n\n");

if ( -r '/proc/sys/dev/sensors/eeprom-i2c-0-57')
{
	vaio_decode('0','57');
}
else
{
	print("Vaio eeprom not found.  Please make sure that the eeprom module is loaded.\n");
	print("If you believe this is an error, please contact me <khali\@linux-fr.org>\n");
	print("so that we may see how to fix the problem.\n");
}

print("\n");
