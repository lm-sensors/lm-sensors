#!/usr/bin/perl
#
# Copyright 1998, 1999 Philip Edelbrock <phil@netroedge.com>
# and Mark Studebaker <mdsxyz123@yahoo.com>
#
# Version 0.1
#
#
# ID ROM data decoding for Xeon processors. 
# Each Xeon processor contains two memories:
#	- A scratch EEPROM at an even location 0x50, 52, 54, or 56;
#	- An ID ROM at an odd location 0x51, 53, 55, or 57.
# This program decodes the ID ROM's only.
# The scratch EEPROMs have no prescribed format.
# If the output of this program makes no sense for a particular device,
# it is probably decoding a DIMM Serial Presence Detect (SPD) EEPROM.
# See ../eeprom/decode-dimms.pl to decode those devices.
#
#
# Two assumptions: lm_sensors-2.3.1 or greater installed,
# and Perl is at /usr/bin/perl
#
# To do:
#	Calculate and check checksums for each section
#	Decode flags in byte 0x7B (cartridge feature flags)
#
# References: 
# "Pentium II Xeon Processor at 400 and 450 MHz" Data Sheet
# Intel 
#
#
#

print "Xeon Processor Information ROM Decoder\n";
print "Written by Philip Edelbrock and Mark Studebaker.  Copyright 1998, 1999.\n";
print "Version 2.6.3\n\n";

$dimm_count=0;
$_=`ls /proc/sys/dev/sensors/`;
@dimm_list=split();

for $i ( 0 .. $#dimm_list ) {
	$_=$dimm_list[$i];
	if ((/^eeprom-/) && (/-51$/ || /-53$/ || /-55$/ || /-57$/)) {
		$dimm_count=$dimm_count + 1;
		
		print "\nDecoding Xeon ROM: /proc/sys/dev/sensors/$dimm_list[$i]\n";
		if (/^[^-]+-[^-]+-[^-]+-([^-]+)$/) {
			$dimm_num=($1 - 49) / 2;
			print "Guessing Xeon is number $dimm_num\n";
		}
# Decode first 16 bytes
		print "\t\t----=== Xeon ROM Header Data ===----\n";

		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/00`;
		@bytes=split(" ");
		
		printf("\tData Format Revision: \t\t\t\t0x%.4X\n", $bytes[0]);

		print "\tTotal number of bytes in EEPROM:\t\t";
		print ($bytes[1] << 4) + $bytes[2];
		print "\n";

		printf("\tProcessor Data Address:\t\t\t\t0x%.2X\n", $bytes[3]);
		printf("\tProcessor Core Data Address:\t\t\t0x%.2X\n", $bytes[4]);
		printf("\tL2 Cache Data Address:\t\t\t\t0x%.2X\n", $bytes[5]);
		printf("\tSEC Cartridge Data Address:\t\t\t0x%.2X\n", $bytes[6]);
		printf("\tPart Number Data Address:\t\t\t0x%.2X\n", $bytes[7]);
		printf("\tThermal Reference Data Address:\t\t\t0x%.2X\n", $bytes[8]);
		printf("\tFeature Data Address:\t\t\t\t0x%.2X\n", $bytes[9]);
		printf("\tOther Data Address:\t\t\t\t0x%.2X\n", $bytes[10]);
		
		print "\t\t----=== Xeon ROM Processor Data ===----\n";
		
# Decode next 16 bytes
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/10`;
		@bbytes=split(" ");
	      	print "\tS-spec/QDF Number:\t\t\t\t\"";
		print pack("cccccc",$bytes[14],$bytes[15],$bbytes[0],
    				    $bbytes[1],$bbytes[2],$bbytes[3]);
		print "\"\n";
		$tmp =  $bbytes[4] & 0xC0 >> 6;
		printf("\tSample / Production:\t\t\t\t0x%.2X", $tmp);
		if($tmp) {
			print " (Production)\n";
		} else {
			print " (Sample)\n";
		}

		print "\t\t----=== Xeon ROM Core Data ===----\n";
		
		printf("\tProcessor Core Type:\t\t\t\t0x%.2X\n",
			($bbytes[6] & 0xC0) >> 6);
		printf("\tProcessor Core Family:\t\t\t\t0x%.2X\n",
			($bbytes[6] & 0x3C) >> 2);
		printf("\tProcessor Core Model:\t\t\t\t0x%.2X\n",
			(($bbytes[6] & 0x03) << 2) + (($bbytes[7] & 0xC0) >> 6));
		printf("\tProcessor Core Stepping:\t\t\t0x%.2X\n",
			($bbytes[7] & 0x30) >> 4);
		print "\tMaximum Core Frequency (Mhz):\t\t\t";
		print ($bbytes[13] << 4) + $bbytes[14];
		print "\n";
		
# Decode next 16 bytes (32-47)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/20`;
		@bytes=split(" ");
		print "\tCore Voltage ID (mV):\t\t\t\t";
		print ($bbytes[15] << 4) + $bytes[0];
		print "\n";
		print "\tCore Voltage Tolerance, High (mV):\t\t";
		print $bytes[1];
		print "\n";
		print "\tCore Voltage Tolerance, Low (mV):\t\t";
		print $bytes[2];
		print "\n";

		print "\t\t----=== Xeon ROM L2 Cache Data ===----\n";
		
		print "\tL2 Cache Size (KB):\t\t\t\t";
		print ($bytes[9] << 4) + $bytes[10];
		print "\n";
		printf("\tNumber of SRAM Components:\t\t\t%d\n",
			($bytes[11] & 0xF0) >> 4);
		print "\tL2 Cache Voltage ID (mV):\t\t\t";
		print ($bytes[12] << 4) + $bytes[13];
		print "\n";
		print "\tL2 Cache Voltage Tolerance, High (mV):\t\t";
		print $bytes[14];
		print "\n";
		print "\tL2 Cache Voltage Tolerance, Low (mV):\t\t";
		print $bytes[15];
		print "\n";

# Decode next 16 bytes (48-63)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/30`;
		@bytes=split(" ");

		printf("\tCache/Tag Stepping ID:\t\t\t\t0x%.2X\n",
			($bytes[0] & 0xF0) >> 4);

		print "\t\t----=== Xeon ROM Cartridge Data ===----\n";

	      	print "\tCartridge Revision:\t\t\t\t\"";
		print pack("cccc",$bytes[2],$bytes[3],$bytes[4],$bytes[5]);
		print "\"\n";
		printf("\tSubstrate Rev. Software ID:\t\t\t0x%.2X\n",
			($bbytes[6] & 0xC0) >> 6);

		print "\t\t----=== Xeon ROM Part Number Data ===----\n";

	      	print "\tProcessor Part Number:\t\t\t\t\"";
		print pack("ccccccc",$bytes[8],$bytes[9],$bytes[10],
  			    $bytes[11],$bytes[12],$bytes[13],$bytes[14]);
		print "\"\n";
		$byte15=$byte[15];

# Decode next 16 bytes (64-79)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/40`;
		@bytes=split(" ");

	      	print "\tProcessor BOM ID:\t\t\t\t\"";
		print pack("cccccccccccccc",$byte15,$bytes[0],$bytes[1],
			$bytes[2],$bytes[3],$bytes[4],$bytes[5],$bytes[6],
			$bytes[7],$bytes[8],$bytes[9],$bytes[10],$bytes[11],
			$bytes[12]);
		print "\"\n";

# Decode next 16 bytes (80-95)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/50`;
		@bbytes=split(" ");
		printf("\tProcessor Electronic Signature: \t\t0x%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X\n",
			$bytes[13],$bytes[14],$bytes[15],$bbytes[0],
			$bbytes[1],$bbytes[2],$bbytes[3],$bbytes[4]);

# Decode next 16 bytes (96-111)
# Not used...   

# Decode next 16 bytes (112-127)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/70`;
		@bytes=split(" ");
		
		print "\t\t----=== Xeon Thermal Reference Data ===----\n";

		printf("\tThermal Reference Byte: \t\t\t0x%.2X\n", $bytes[0]);

		print "\t\t----=== Xeon ROM Feature Data ===----\n";

		printf("\tProcessor Core Feature Flags: \t\t\t0x%.2X%.2X%.2X%.2X\n",
			$bytes[4],$bytes[5],$bytes[6],$bytes[7]);
		printf("\tCartridge Feature Flags: \t\t\t0x%.2X%.2X%.2X%.2X\n",
			$bytes[8],$bytes[9],$bytes[10],$bytes[11]);
		printf("\tNumber of Devices in TAP Chain:\t\t\t%d\n",
			($bytes[12] & 0xF0) >> 4);
		
	}
}
print "\n\nNumber of Xeon ROMs detected and decoded: $dimm_count\n";
