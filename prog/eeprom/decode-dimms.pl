#!/usr/bin/perl
#
# Copyright 1998, 1999 Philip Edelbrock <phil@netroedge.com>
#
# Version 0.4
#
# EEPROM data decoding for SDRAM DIMM modules. 
#
# Two assumptions: lm_sensors-2.0.2 installed,
# and Perl is at /usr/bin/perl
#
#
# References: 
# PC SDRAM Serial Presence 
# Detect (SPD) Specification, Intel, 
# Dec '97, Rev 1.2A
#
#
# Jedec Standards 4.1.x & 4.5.x
# http://www.jedec.org
#

print "PC-100 DIMM Serial Presence Detect Tester/Decoder\n";
print "Written by Philip Edelbrock.  Copyright 1998, 1999.\n";
print "Version 0.2\n\n";

$dimm_count=0;
$_=`ls /proc/sys/dev/sensors/`;
@dimm_list=split();

for $i ( 0 .. $#dimm_list ) {
	$_=$dimm_list[$i];
	if (/^eeprom-/) {
		$dimm_checksum=0;
		$dimm_count=$dimm_count + 1;
		
		print "\nDecoding EEPROM: /proc/sys/dev/sensors/$dimm_list[$i]\n";
		if (/^[^-]+-[^-]+-[^-]+-([^-]+)$/) {
			$dimm_num=$1 - 49;
			print "Guessing DIMM is in bank $dimm_num\n";
		}
# Decode first 16 bytes
		print "\t\t----=== The Following is Required Data and is Applicable to all DIMM Types ===----\n";

		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data0-15`;
		@bytes=split(" ");
		for $j ( 0 .. 15 ) { $dimm_checksum = $dimm_checksum + $bytes[$j];  }
		
		print "\t# of bytes written to SDRAM EEPROM:\t\t$bytes[0]\n";

		print "\tTotal number of bytes in EEPROM:\t\t";
		if ($bytes[1] < 13) {
			print 2**$bytes[1];
			print "\n";
		} elsif ($bytes[1] == 0) {
			print "RFU\n"; 
		} else { print "ERROR!\n"; }

		print "\tFundemental Memory type:\t\t\t";
		if ($bytes[2] == 2) { print "EDO\n"; } elsif ($bytes[2] == 4) { print "SDRAM\n";
			} else { print "???\n"; }

		print "\tNumber of Row Address Bits (SDRAM only):\t";
		if ($bytes[3] == 0) { print "Undefined!\n" } 		
		elsif ($bytes[3] == 1) { print "1/16\n" } 		
		elsif ($bytes[3] == 2) { print "2/17\n" } 		
		elsif ($bytes[3] == 3) { print "3/18\n" }
		else { print $bytes[3]; print "\n"; }

		print "\tNumber of Col Address Bits (SDRAM only):\t";
		if ($bytes[4] == 0) { print "Undefined!\n" } 		
		elsif ($bytes[4] == 1) { print "1/16\n" } 		
		elsif ($bytes[4] == 2) { print "2/17\n" } 		
		elsif ($bytes[4] == 3) { print "3/18\n" }
		else { print $bytes[4]; print "\n"; }

		print "\tNumber of Module Rows:\t\t\t\t";
		if ($bytes[5] == 0 ) { print "Undefined!\n"; } else { print $bytes[5]; print "\n"; }

		print "\tData Width (SDRAM only):\t\t\t";
		if ($bytes[7] > 1) { print "Undefined!\n" } else {
			$temp=($bytes[7]*256) + $bytes[6];
			print $temp; print "\n"; }

		print "\tModule Interface Signal Levels:\t\t\t";
		if ($bytes[8] == 0) { print "5.0 Volt/TTL\n";} elsif ($bytes[8] == 1) { 
			print "LVTTL\n";} elsif ($bytes[8] == 2) { print "HSTL 1.5\n";} elsif ($bytes[8] == 3) { 
			print "SSTL 3.3\n";} elsif ($bytes[8] == 4) { print "SSTL 2.5\n";} else { print "Undefined!\n";}
		
		print "\tCycle Time (SDRAM):\t\t\t\t";
		$temp=($bytes[9] >> 4) + (($bytes[9] - (($bytes[9] >> 4)<< 4)) * 0.1);
		print $temp; print "ns\n";
		
		print "\tAccess Time (SDRAM):\t\t\t\t";
		$temp=($bytes[10] >> 4) + (($bytes[10] - (($bytes[10] >> 4)<< 4)) * 0.1);
		print $temp; print "ns\n";
		
		print "\tModule Configuration Type:\t\t\t";
		if ($bytes[11] == 0) { print "No Parity\n"; } elsif ($bytes[11] == 1) { print "Parity\n"; 
			} elsif ($bytes[11] == 2) { print "ECC\n"; } else { print "Undefined!"; }
			
		print "\tRefresh Type:\t\t\t\t\t";
		if ($bytes[12] > 126) { print "Self Refreshing\n"; $temp=$bytes[12] - 127;
			} else { print "Not Self Refreshing\n"; $temp=$bytes[12];}
		
		print "\tRefresh Rate:\t\t\t\t\t";
		if ($temp == 0) { print "Normal (15.625uS)\n"; } elsif ($temp == 1) { print "Reduced (3.9uS)\n"; 
		} elsif ($temp == 2) { print "Reduced (7.8uS)\n"; } elsif ($temp == 3) { print "Extended (31.3uS)\n"; 
		} elsif ($temp == 4) { print "Extended (62.5uS)\n"; } elsif ($temp == 5) { print "Extended (125uS)\n";
		} else { print "Undefined!";}
		
		print "\tPrimary SDRAM Component Bank Config:\t\t";
		if ($bytes[13]>126) {$temp=$bytes[13]-127; print "Bank2 = 2 x Bank1\n";} else {
			$temp=$bytes[13]; print "No Bank2 OR Bank2 = Bank1 width\n";}
		
		print "\tPrimary SDRAM Component Widths:\t\t\t";
		if ($temp == 0) { print "Undefined!\n"; } else { print "$temp\n"; }
		
		print "\tError Checking SDRAM Component Bank Config:\t";
		if ($bytes[14]>126) {$temp=$bytes[14]-127; print "Bank2 = 2 x Bank1\n";} else {
			$temp=$bytes[14]; print "No Bank2 OR Bank2 = Bank1 width\n";}
		
		print "\tError Checking SDRAM Component Widths:\t\t";
		if ($temp == 0) { print "Undefined!\n"; } else { print "$temp\n"; }
		
		print "\tMin Clock Delay for Back to Back Random Access:\t";
		if ($bytes[15] == 0) { print "Undefined!\n"; } else { print "$bytes[15]\n"; }
		
		print "\t\t----=== The Following Apply to SDRAM DIMMs ONLY ===----\n";
		
# Decode next 16 bytes
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data16-31`;
		@bytes=split(" ");
		for $j ( 0 .. 15 ) { $dimm_checksum = $dimm_checksum + $bytes[$j];  }
		
		print "\tBurst lengths supported:\t\t\t";
		$temp="";
		if (($bytes[0] & 1) > 0) { print "${temp}Burst Length = 1\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[0] & 2) > 0) { print "${temp}Burst Length = 2\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[0] & 4) > 0) { print "${temp}Burst Length = 4\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[0] & 8) > 0) { print "${temp}Burst Length = 8\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[0] & 16) > 0) { print "${temp}Undefined! (bit 4)\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[0] & 32) > 0) { print "${temp}Undefined! (bit 5)\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[0] & 64) > 0) { print "${temp}Undefined! (bit 6)\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[0] & 128) > 0) { print "${temp}Burst Length = Page\n"; $temp="\t\t\t\t\t\t\t"; }
		if ($bytes[0] == 0) { print "(None Supported)\n";}
		
		print "\tNumber of Device Banks:\t\t\t\t";
		if ($bytes[1] == 0) { print "Undefined/Reserved!\n"; } else { print "$bytes[1]\n"; }
		
		print "\tSupported CAS Latencies:\t\t\t";
		$temp="";
		if (($bytes[2] & 1) > 0) { print "${temp}CAS Latency = 1\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[2] & 2) > 0) { print "${temp}CAS Latency = 2\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[2] & 4) > 0) { print "${temp}CAS Latency = 3\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[2] & 8) > 0) { print "${temp}CAS Latency = 4\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[2] & 16) > 0) { print "${temp}CAS Latency = 5\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[2] & 32) > 0) { print "${temp}CAS Latency = 6\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[2] & 64) > 0) { print "${temp}CAS Latency = 7\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[2] & 128) > 0) { print "${temp}Undefined (bit 7)\n"; $temp="\t\t\t\t\t\t\t"; }
		if ($bytes[2] == 0) { print "(None Supported)\n";}
		
		print "\tSupported CS Latencies:\t\t\t\t";
		$temp="";
		if (($bytes[3] & 1) > 0) { print "${temp}CS Latency = 0\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[3] & 2) > 0) { print "${temp}CS Latency = 1\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[3] & 4) > 0) { print "${temp}CS Latency = 2\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[3] & 8) > 0) { print "${temp}CS Latency = 3\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[3] & 16) > 0) { print "${temp}CS Latency = 4\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[3] & 32) > 0) { print "${temp}CS Latency = 5\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[3] & 64) > 0) { print "${temp}CS Latency = 6\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[3] & 128) > 0) { print "${temp}Undefined (bit 7)\n"; $temp="\t\t\t\t\t\t\t"; }
		if ($bytes[3] == 0) { print "(None Supported)\n";}
		
		print "\tSupported WE Latencies:\t\t\t\t";
		$temp="";
		if (($bytes[4] & 1) > 0) { print "${temp}WE Latency = 0\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[4] & 2) > 0) { print "${temp}WE Latency = 1\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[4] & 4) > 0) { print "${temp}WE Latency = 2\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[4] & 8) > 0) { print "${temp}WE Latency = 3\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[4] & 16) > 0) { print "${temp}WE Latency = 4\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[4] & 32) > 0) { print "${temp}WE Latency = 5\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[4] & 64) > 0) { print "${temp}WE Latency = 6\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[4] & 128) > 0) { print "${temp}Undefined (bit 7)\n"; $temp="\t\t\t\t\t\t\t"; }
		if ($bytes[4] == 0) { print "(None Supported)\n";}
		
		print "\tSDRAM Module Attributes:\t\t\t";
		$temp="";
		if (($bytes[5] & 1) > 0) { print "${temp}Buffered Address/Control Inputs\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[5] & 2) > 0) { print "${temp}Registered Address/Control Inputs\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[5] & 4) > 0) { print "${temp}On card PLL (clock)\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[5] & 8) > 0) { print "${temp}Buffered DQMB Inputs\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[5] & 16) > 0) { print "${temp}Registered DQMB Inputs\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[5] & 32) > 0) { print "${temp}Differential Clock Input\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[5] & 64) > 0) { print "${temp}Redundant Row Address\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[5] & 128) > 0) { print "${temp}Undefined (bit 7)\n"; $temp="\t\t\t\t\t\t\t"; }
		if ($bytes[5] == 0) { print "(None Reported)\n";}
		
		print "\tSDRAM Device Attributes (General):\t\t";
		$temp="";
		if (($bytes[6] & 1) > 0) { print "${temp}Supports Early RAS# Recharge\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[6] & 2) > 0) { print "${temp}Supports Auto-Precharge\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 4) > 0) { print "${temp}Supports Precharge All\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 8) > 0) { print "${temp}Supports Write1/Read Burst\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 16) > 0) { print "${temp}Lower VCC Tolerance:5%\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 16) == 0) { print "${temp}Lower VCC Tolerance:10%\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 32) > 0) { print "${temp}Upper VCC Tolerance:5%\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 32) == 0) { print "${temp}Upper VCC Tolerance:10%\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 64) > 0) { print "${temp}Undefined (bit 6)\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[6] & 128) > 0) { print "${temp}Undefined (bit 7)\n"; $temp="\t\t\t\t\t\t\t"; }
		
		print "\tSDRAM Cycle Time (2ns highest CAS):\t\t";
		$temp=$bytes[7] >> 4;
		if ($temp == 0) { print "Undefined!\n"; } else {
			if ($temp < 4 ) {$temp=$temp + 15;}
			print $temp + (($bytes[7] & 15) / 10);
			print "nS\n";
		}
		
		print "\tSDRAM Access from Clock Time (2nd highest CAS):\t";
		$temp=$bytes[8] >> 4;
		if ($temp == 0) { print "Undefined!\n"; } else {
			if ($temp < 4 ) {$temp=$temp + 15;}
			print $temp + (($bytes[8] & 15) / 10.0);
			print "nS\n";
		}
		
		print "\t\t----=== The Following are Optional (may be Bogus) ===----\n";
		
		print "\tSDRAM Cycle Time (3rd highest CAS):\t\t";
		$temp=$bytes[9] >> 2;
		if ($temp == 0) { print "Undefined!\n"; } else {
			print $temp + (($bytes[9] & 3) / 4.0);
			print "nS\n";
		}
		
		print "\tSDRAM Access from Clock Time (3rd highest CAS):\t";
		$temp=$bytes[10] >> 2;
		if ($temp == 0) { print "Undefined!\n"; } else {
			print $temp + (($bytes[10] & 3) / 4.0);
			print "nS\n";
		}
		
		print "\t\t----=== The Following are Required (for SDRAMs) ===----\n";
		
		print "\tMinumum Row Precharge Time:\t\t\t";
		if ($bytes[11] == 0) { print "Undefined!\n"; } else { print "$bytes[11]nS\n"; }
		
		print "\tRow Active to Row Active Min:\t\t\t";
		if ($bytes[12] == 0) { print "Undefined!\n"; } else { print "$bytes[12]nS\n"; }
		
		print "\tRAS to CAS Delay:\t\t\t\t";
		if ($bytes[13] == 0) { print "Undefined!\n"; } else { print "$bytes[13]nS\n"; }
		
		print "\tMin RAS Pulse Width:\t\t\t\t";
		if ($bytes[14] == 0) { print "Undefined!\n"; } else { print "$bytes[14]nS\n"; }
		
		
		print "\t\t----=== The Following are Required and Apply to ALL DIMMs ===----\n";
		
		print "\tRow Densities:\t\t\t\t\t";
		$temp="";
		if (($bytes[15] & 1) > 0) { print "${temp}4 MByte\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 2) > 0) { print "${temp}8 MByte\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[15] & 4) > 0) { print "${temp}16 MByte\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[15] & 8) > 0) { print "${temp}32 MByte\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[15] & 16) > 0) { print "${temp}64 MByte\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[15] & 32) > 0) { print "${temp}128 MByte\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[15] & 64) > 0) { print "${temp}256 MByte\n"; $temp="\t\t\t\t\t\t\t"; }
		if (($bytes[15] & 128) > 0) { print "${temp}512 MByte\n"; $temp="\t\t\t\t\t\t\t"; }
		if ($bytes[15] == 0) { print "(Undefined! -- None Reported!)\n";}
		
		
# Decode next 16 bytes (32-47)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data32-47`;
		@bytes=split(" ");
		for $j ( 0 .. 15 ) { $dimm_checksum = $dimm_checksum + $bytes[$j];  }
		
		print "\t\t----=== The Following are Proposed and Apply to SDRAM DIMMs ===----\n";
		
		print "\tCommand and Address Signal Setup Time:\t\t";
		if ($bytes[0] > 127) { $temp=($bytes[0]-128)>>4; $temp2=-1; } else { $temp=$bytes[0]>>4; $temp2=1;}
		print $temp2 * ($temp + (($bytes[0] & 15) / 10.0));
		print "nS\n";
		
		print "\tCommand and Address Signal Hold Time:\t\t";
		if ($bytes[1] > 127) { $temp=($bytes[1]-128)>>4; $temp2=-1; } else { $temp=$bytes[1]>>4; $temp2=1;}
		print $temp2 * ($temp + (($bytes[1] & 15) / 10.0));
		print "nS\n";
		
		print "\tData Signal Setup Time:\t\t\t\t";
		if ($bytes[2] > 127) { $temp=($bytes[2]-128)>>4; $temp2=-1; } else { $temp=$bytes[2]>>4; $temp2=1;}
		print $temp2 * ($temp + (($bytes[2] & 15) / 10.0));
		print "nS\n";
		
		print "\tData Signal Hold Time:\t\t\t\t";
		if ($bytes[3] > 127) { $temp=($bytes[3]-128)>>4; $temp2=-1; } else { $temp=$bytes[3]>>4; $temp2=1;}
		print $temp2 * ($temp + (($bytes[3] & 15) / 10.0));
		print "nS\n";

# That's it for the lower part of an SDRAM EEPROM's memory!
# Decode next 16 bytes (48-63)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data48-63`;
		@bytes=split(" ");
		for $j ( 0 .. 14 ) { $dimm_checksum = $dimm_checksum + $bytes[$j];  }

		print "\tSPD Revision code:\t\t\t\t$bytes[14]\n";
		print "\tEEPROM Checksum of bytes 0-62:\t\t\t";
		printf("0x%.2X (verses calculated: 0x%.2X)\n",$bytes[15],$dimm_checksum & 255);
		
# Decode next 16 bytes (64-79)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data64-79`;
		@bytes=split(" ");
		
		print "\tManufacturer's JEDEC ID Code:\t\t\t";
		printf("0x%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X\n",$bytes[0],$bytes[1],$bytes[2],$bytes[3],$bytes[4],$bytes[5],$bytes[6],$bytes[7]);
		print "\t\t\t\t\t\t\t(\"";
		print pack("cccccccc",
			$bytes[0],$bytes[1],$bytes[2],$bytes[3],$bytes[4],$bytes[5],$bytes[6],$bytes[7]);
		print "\")\n";
		
		print "\tManufacturing Location Code:\t\t\t";
		printf("0x%.2X\n",$bytes[8]);
		
		print "\tManufacurer's Part Number:\t\t\t\"";
# Decode next 16 bytes (80-95)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data80-95`;
		@bytes2=split(" ");
		print pack("cccccccccccccccccc",$bytes[9],$bytes[10],$bytes[11],$bytes[12],$bytes[13],$bytes[14],$bytes[15],
			$bytes2[0],$bytes2[1],$bytes2[2],$bytes2[3],$bytes2[4],$bytes2[5],$bytes2[6],$bytes2[7],$bytes2[8],$bytes2[9]);
		print "\"\n";
		
		print "\tRevision Code:\t\t\t\t\t";
		printf("0x%.2X%.2X\n",$bytes2[10],$bytes2[11]);
		
		print "\tManufacturing Date:\t\t\t\t";
		printf("0x%.2X%.2X\n",$bytes2[12],$bytes2[13]);
		
		print "\tAssembly Serial Number:\t\t\t\t";
# Decode next 16 bytes (96-111)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data96-111`;
		@bytes=split(" ");
		
		printf("0x%.2X%.2X%.2X%.2X\n",$bytes2[15],$bytes[0],$bytes[1],$bytes[2]);
# Decode next 16 bytes (112-127)
		$_=`cat /proc/sys/dev/sensors/$dimm_list[$i]/data112-127`;
		@bytes=split(" ");
		
		print "\tIntel Specification for Frequency:\t\t";
		if ($bytes[14] == 102) { print "66MHz\n"; } elsif ($bytes[14] == 100) { print "100MHz\n"; } else { print "Undefined!\n"; }
		
		print "\tIntel Spec Details for 100MHz Support:\t\t";
		$temp="";
		if (($bytes[15] & 1) > 0) { print "${temp}Intel Concurrent AutoPrecharge\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 2) > 0) { print "${temp}CAS Latency = 2\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 4) > 0) { print "${temp}CAS Latency = 3\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 8) > 0) { print "${temp}Junction Temp A (90 degrees C)\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 8) == 0) { print "${temp}Junction Temp B (100 degrees C)\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 16) > 0) { print "${temp}CLK 3 Connected\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 32) > 0) { print "${temp}CLK 2 Connected\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 64) > 0) { print "${temp}CLK 1 Connected\n"; $temp="\t\t\t\t\t\t\t";}
		if (($bytes[15] & 128) > 0) { print "${temp}CLK 0 Connected\n"; $temp="\t\t\t\t\t\t\t";}
		if ($bytes[15] > 175) { print "${temp}Double Sided DIMM\n"; $temp="\t\t\t\t\t\t\t";
			} else {print "${temp}Single Sided DIMM\n"; $temp="\t\t\t\t\t\t\t";}
		
		
	}
}
print "\n\nNumber of SDRAM DIMMs detected and decoded: $dimm_count\n";
