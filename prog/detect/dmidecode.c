/*
 * DMI decode rev 1.8
 *
 *	(C) 2000-2002 Alan Cox <alan@redhat.com>
 *
 *      2-July-2001 Matt Domsch <Matt_Domsch@dell.com>
 *      Additional structures displayed per SMBIOS 2.3.1 spec
 *
 *      13-December-2001 Arjan van de Ven <arjanv@redhat.com>
 *      Fix memory bank type (DMI case 6)
 *
 *      3-August-2002 Mark D. Studebaker <mds@paradyne.com>
 *      Better indent in dump_raw_data
 *      Fix return value in dmi_bus_name
 *      Additional sensor fields decoded
 *      Fix compilation warnings
 *
 *      6-August-2002 Jean Delvare <khali@linux-fr.org>
 *      Reposition file pointer after DMI table display
 *      Disable first RSD PTR checksum (was not correct anyway)
 *      Show actual DMI struct count and occupied size
 *      Check for NULL after malloc
 *      Use SEEK_* constants instead of numeric values
 *      Code optimization (and warning fix) in DMI cases 10 and 14
 *      Add else's to avoid unneeded cascaded if's in main loop
 *      Code optimization in DMI information display
 *      Fix all compilation warnings
 *
 *      9-August-2002 Jean Delvare <khali@linux-fr.org>
 *      Better DMI struct count/size error display
 *      More careful memory access in dmi_table
 *      DMI case 13 (Language) decoded
 *      C++ style comments removed
 *      Commented out code removed
 *      DMI 0.0 case handled
 *      Fix return value of dmi_port_type and dmi_port_connector_type
 *
 *      23-August-2002 Alan Cox <alan@redhat.com>
 *      Make the code pass -Wall -pedantic by fixing a few harmless sign of
 *        pointer mismatches
 *      Correct main prototype
 *      Check for compilers with wrong type sizes
 *
 *      17-Sep-2002 Larry Lile <llile@dreamworks.com>
 *      Type 16 & 17 structures displayed per SMBIOS 2.3.1 spec
 *
 *      20-September-2002 Dave Johnson <ddj@cascv.brown.edu>
 *      Fix comparisons in dmi_bus_name
 *      Fix comparison in dmi_processor_type
 *      Fix bitmasking in dmi_onboard_type
 *      Fix return value of dmi_temp_loc
 *
 *      28-September-2002 Jean Delvare <khali@linux-fr.org>
 *      Fix missing coma in dmi_bus_name
 *      Remove unwanted bitmaskings in dmi_mgmt_dev_type, dmi_mgmt_addr_type,
 *        dmi_fan_type, dmi_volt_loc, dmi_temp_loc and dmi_status
 *      Fix DMI table read bug ("dmi: read: Success")
 *      Make the code pass -W again
 *      Fix return value of dmi_card_size
 *
 *      05-October-2002 Jean Delvare <khali@linux-fr.org>
 *      More ACPI decoded
 *      More PNP decoded
 *      More SYSID decoded
 *      PCI Interrupt Routing decoded
 *      BIOS32 Service Directory decoded
 *      Sony system detection (unconfirmed)
 *      Checksums verified whenever possible
 *      Better checks on file read and close
 *      Define VERSION and display version at beginning
 *      More secure decoding (won't run off the table in any case)
 *      Do not try to decode more structures than announced
 *      Fix an off-by-one error that caused the last address being
 *        scanned to be 0x100000, not 0xFFFF0 as it should
 *
 *      10-October-2002 Jean Delvare <khali@linux-fr.org>
 *      Remove extra semicolon at the end of dmi_memory_array_use
 *      Fix compilation warnings
 *      Add missing backslash in DMI case 37
 *      Fix BIOS ROM size (DMI case 0)
 *
 *      12-October-2002 Jean Delvare <khali@linux-fr.org>
 *      Fix maximum cache size and installed size being inverted
 *      Fix typos in port types
 *
 *      14-October-2002 Jean Delvare <khali@linux-fr.org>
 *      Fix typo in dmi_memory_array_location
 *      Replace Kbyte by kB in DMI case 16
 *      Add DDR entry in dmi_memory_device_type
 *      Fix extra s in SYSIS
 *
 *      15-October-2002 Jean Delvare <khali@linux-fr.org>
 *      Fix bad index in DMI case 27 (cooling device)
 *
 * Licensed under the GNU Public license. If you want to use it in with
 * another license just ask.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * For the avoidance of doubt the "preferred form" of this code is one which
 * is in an open unpatent encumbered format. Where cryptographic key signing
 * forms part of the process of creating an executable the information 
 * including keys needed to generate an equivalently functional executable
 * are deemed to be part of the source code.
 *		[In the light of TCPA and Palladium the author urges
 *		 other open source authors to add such a clarification]
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define VERSION "1.8"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static void
dump_raw_data(void *data, unsigned int length)
{
	char buffer1[80], buffer2[80], *b1, *b2, c;
	unsigned char *p = data;
	unsigned long column=0;
	unsigned int length_printed = 0;
	const unsigned char maxcolumn = 16;
	while (length_printed < length) {
		printf("\t");
		b1 = buffer1;
		b2 = buffer2;
		for (column = 0;
		     column < maxcolumn && length_printed < length; 
		     column ++) {
			b1 += sprintf(b1, "%02x ",(unsigned int) *p);
			if (*p < 32 || *p > 126) c = '.';
			else c = *p;
			b2 += sprintf(b2, "%c", c);
			p++;
			length_printed++;
		}
		/* pad out the line */
		for (; column < maxcolumn; column++)
		{
			b1 += sprintf(b1, "   ");
			b2 += sprintf(b2, " ");
		}
		
		printf("%s\t%s\n", buffer1, buffer2);
	}
}



struct dmi_header
{
	u8	type;
	u8	length;
	u16	handle;
};

static const char *dmi_string(struct dmi_header *dm, u8 s)
{
	char *bp=(char *)dm;
	if(s==0)
		return "";
	
	bp+=dm->length;
	while(s>1 && *bp)
	{
		bp+=strlen(bp);
		bp++;
		s--;
	}
	if(!*bp)
		return "<bad index>";
	return bp;
}

static void dmi_decode_ram(u8 data)
{
	if(data&(1<<0))
		printf("OTHER ");
	if(data&(1<<1))
		printf("UNKNOWN ");
	if(data&(1<<2))
		printf("STANDARD ");
	if(data&(1<<3))
		printf("FPM ");
	if(data&(1<<4))
		printf("EDO ");
	if(data&(1<<5))
		printf("PARITY ");
	if(data&(1<<6))
		printf("ECC ");
	if(data&(1<<7))
		printf("SIMM ");
	if(data&(1<<8))
		printf("DIMM ");
	if(data&(1<<9))
		printf("Burst EDO ");
	if(data&(1<<10))
		printf("SDRAM ");
}

static void dmi_cache_size(u16 n)
{
	if(n&(1<<15))
		printf("%dK\n", (n&0x7FFF)*64);
	else
		printf("%dK\n", n&0x7FFF);
}

static void dmi_decode_cache(u16 c)
{
	if(c&(1<<0))
		printf("Other ");
	if(c&(1<<1))
		printf("Unknown ");
	if(c&(1<<2))
		printf("Non-burst ");
	if(c&(1<<3))
		printf("Burst ");
	if(c&(1<<4))
		printf("Pipeline burst ");
	if(c&(1<<5))
		printf("Synchronous ");
	if(c&(1<<6))
		printf("Asynchronous ");
}

const char *dmi_bus_name(u8 num)
{
	static const char *bus[]={
		"",
		"",
		"",
		"ISA ",
		"MCA ",
		"EISA ",
		"PCI ",
		"PCMCIA ",
		"VLB ",
		"Proprietary ",
		"CPU Slot ",
		"Proprietary RAM ",
		"I/O Riser ",
		"NUBUS ",
		"PCI-66 ",
		"AGP ",
		"AGP 2x ",
		"AGP 4x "
	};
	static const char *jpbus[]={
		"PC98/C20",
		"PC98/C24",
		"PC98/E",
		"PC98/LocalBus",
		"PC98/Card"
	};
	
	if(num<=0x11)
		return bus[num];
	if(num>=0xA0 && num<=0xA4)
		return jpbus[num - 0xA0];
	return "";
}

const char *dmi_bus_width(u8 code)
{
	static const char *width[]={
		"",
		"",
		"",
		"8bit ",
		"16bit ",
		"32bit ",
		"64bit ",
		"128bit "
	};
	if(code>7)
		return "";
	return width[code];
}

const char *dmi_card_size(u8 v)
{
	if(v==3)
		return("Short ");
	if(v==4)
		return("Long ");
	return "";
}

static void dmi_card_props(u8 v)
{
	printf("\t\tSlot Features: ");
	if(v&(1<<1))
		printf("5v ");
	if(v&(1<<2))
		printf("3.3v ");
	if(v&(1<<3))
		printf("Shared ");
	if(v&(1<<4))
		printf("PCCard16 ");
	if(v&(1<<5))
		printf("CardBus ");
	if(v&(1<<6))
		printf("Zoom-Video ");
	if(v&(1<<7))
		printf("ModemRingResume ");
	printf("\n");
}		
		
const char *dmi_chassis_type(u8 code)
{
	static const char *chassis_type[]={
		"",
		"Other",
		"Unknown",
		"Desktop",
		"Low Profile Desktop",
		"Pizza Box",
		"Mini Tower",
		"Tower",
		"Portable",
		"Laptop",
		"Notebook",
		"Hand Held",
		"Docking Station",
		"All in One",
		"Sub Notebook",
		"Space-saving",
		"Lunch Box",
		"Main Server Chassis",
		"Expansion Chassis",
		"SubChassis",
		"Bus Expansion Chassis",
		"Peripheral Chassis",
		"RAID Chassis",
		"Rack Mount Chassis",
		"Sealed-case PC",
	};
	code &= ~0x80;
	
	if(code>0x18)
		return "";
	return chassis_type[code];
	
}

const char *dmi_port_connector_type(u8 code)
{
	static const char *connector_type[]={
		"None",
		"Centronics",
		"Mini Centronics",
		"Proprietary",
		"DB-25 pin male",
		"DB-25 pin female",
		"DB-15 pin male",
		"DB-15 pin female",
		"DB-9 pin male",
		"DB-9 pin female",
		"RJ-11", 
		"RJ-45",
		"50 Pin MiniSCSI",
		"Mini-DIN",
		"Micro-DIN",
		"PS/2",
		"Infrared",
		"HP-HIL",
		"Access Bus (USB)",
		"SSA SCSI",
		"Circular DIN-8 male",
		"Circular DIN-8 female",
		"On Board IDE",
		"On Board Floppy",
		"9 Pin Dual Inline (pin 10 cut)",
		"25 Pin Dual Inline (pin 26 cut)",
		"50 Pin Dual Inline",
		"68 Pin Dual Inline",
		"On Board Sound Input from CD-ROM",
		"Mini-Centronics Type-14",
		"Mini-Centronics Type-26",
		"Mini-jack (headphones)",
		"BNC",
		"1394",
		"PC-98",
		"PC-98Hireso",
		"PC-H98",
		"PC-98Note",
		"PC98Full",
	};
	
	if(code == 0xFF)
		return "Other";
	
	if(code <= 0x21)
		return connector_type[code];
	
	if((code >= 0xA0) && (code <= 0xA4))
		return connector_type[code-0xA0+0x22];
	
	return "";
}

static const char *dmi_memory_array_location(u8 num)
{
	static const char *memory_array_location[]={
		"",
		"Other",
		"Unknown",
		"System board or motherboard",
		"ISA add-on card",
		"EISA add-on card",
		"PCI add-on card",
		"MCA add-on card",
		"PCMCIA add-on card",
		"Proprietary add-on card",
		"NuBus",
	};
	static const char *jp_memory_array_location[]={
		"PC-98/C20 add-on card",
		"PC-98/C24 add-on card",
		"PC-98/E add-on card",
		"PC-98/Local bus add-on card",
	};
	if(num<=0x0A)
		return memory_array_location[num];
	if(num>=0xA0 && num<0xA3)
		return jp_memory_array_location[num];
	return "";
}

static const char *dmi_memory_array_use(u8 num)
{
	static const char *memory_array_use[]={
		"",
		"Other",
		"Unknown",
		"System memory",
		"Video memory",
		"Flash memory",
		"Non-volatile RAM",
		"Cache memory",
	};
	if (num > 0x07)
		return "";
	return memory_array_use[num];
}

static const char *dmi_memory_array_error_correction_type(u8 num)
{
	static const char *memory_array_error_correction_type[]={
		"",
		"Other",
		"Unknown",
		"None",
		"Parity",
		"Single-bit ECC",
		"Multi-bit ECC",
		"CRC",
	};
	if (num > 0x07)
		return "";
	return memory_array_error_correction_type[num];
}

static const char *dmi_memory_device_form_factor(u8 num)
{
	static const char *memory_device_form_factor[]={
		"",
		"Other",
		"Unknown",
		"SIMM",
		"SIP",
		"Chip",
		"DIP",
		"ZIP",
		"Proprietary Card",
		"DIMM",
		"TSOP",
		"Row of chips",
		"RIMM",
		"SODIMM",
		"SRIMM",
	};
	if (num > 0x0E)
		return "";
	return memory_device_form_factor[num];
}

static const char *dmi_memory_device_type(u8 num)
{
	static const char *memory_device_type[]={
		"",
		"Other",
		"Unknown",
		"DRAM",
		"EDRAM",
		"VRAM",
		"SRAM",
		"RAM",
		"ROM",
		"FLASH",
		"EEPROM",
		"FEPROM",
		"EPROM",
		"CDRAM",
		"3DRAM",
		"SDRAM",
		"SGRAM",
		"RDRAM",
		"DDR"
	};
	if (num > 0x12)
		return "";
	return memory_device_type[num];
}

static void dmi_memory_device_detail(u8 v)
{
	printf("\t\tType Detail: ");
	if (v&(1<<1))
		printf("Other ");
	if (v&(1<<2))
		printf("Unknown ");
	if (v&(1<<3))
		printf("Fast-paged ");
	if (v&(1<<4))
		printf("Static column ");
	if (v&(1<<5))
		printf("Pseudo-static ");
	if (v&(1<<6))
		printf("RAMBUS ");
	if (v&(1<<7))
		printf("Synchronous ");
	if (v&(1<<8))
		printf("CMOS ");
	if (v&(1<<9))
		printf("EDO ");
	if (v&(1<<10))
		printf("Window DRAM ");
	if (v&(1<<11))
		printf("Cache DRAM ");
	if (v&(1<<12))
		printf("Non-volatile ");
	printf("\n");
}

const char *dmi_port_type(u8 code)
{
	static const char *port_type[]={
		"None",
		"Parallel Port XT/AT Compatible",
		"Parallel Port PS/2",
		"Parallel Port ECP",
		"Parallel Port EPP",
		"Parallel Port ECP/EPP",
		"Serial Port XT/AT Compatible",
		"Serial Port 16450 Compatible",
		"Serial Port 16550 Compatible",
		"Serial Port 16550A Compatible",
		"SCSI Port",
		"MIDI Port",
		"Joy Stick Port",
		"Keyboard Port",
		"Mouse Port",
		"SSA SCSI",
		"USB",
		"FireWire (IEEE P1394)",
		"PCMCIA Type I",
		"PCMCIA Type II",
		"PCMCIA Type III",
		"Cardbus",
		"Access Bus Port",
		"SCSI II",
		"SCSI Wide",
		"PC-98",
		"PC-98-Hireso",
		"PC-H98",
		"Video Port",
		"Audio Port",
		"Modem Port",
		"Network Port",
		"8251 Compatible",
		"8251 FIFO Compatible",
	};
	
	if(code == 0xFF)
		return "Other";
	
	if (code <= 0x1F)
		return port_type[code];
	
	if ((code >= 0xA0) && (code <= 0xA1))
		return port_type[code-0xA0+0x20];
	
	return "";
}

const char *dmi_processor_type(u8 code)
{
	static const char *processor_type[]={
		"",
		"Other",
		"Unknown",
		"Central Processor",
		"Math Processor",
		"DSP Processor",
		"Video Processor"
	};
	
	if(code == 0xFF)
		return "Other";
	
	if (code > 6)
		return "";
	return processor_type[code];
}

const char *dmi_processor_family(u8 code)
{
	static const char *processor_family[]={
		"",
		"Other",
		"Unknown",
		"8086",
		"80286",
		"Intel386 processor",
		"Intel486 processor",
		"8087",
		"80287",
		"80387",
		"80487",
		"Pentium processor Family",
		"Pentium Pro processor",
		"Pentium II processor",
		"Pentium processor with MMX technology",
		"Celeron processor",
		"Pentium II Xeon processor",
		"Pentium III processor",
		"M1 Family",
		"M1","M1","M1","M1","M1","M1", /* 13h - 18h */
		"K5 Family",
		"K5","K5","K5","K5","K5","K5", /* 1Ah - 1Fh */
		"Power PC Family",
		"Power PC 601",
		"Power PC 603",
		"Power PC 603+",
		"Power PC 604",
	};
	
	if(code == 0xFF)
		return "Other";
	
	if (code > 0x24)
		return "";
	return processor_family[code];
}

const char *dmi_onboard_type(u8 code)
{
	static const char *onboard_type[]={
		"",
		"Other",
		"Unknown",
		"Video",
		"SCSI Controller",
		"Ethernet",
		"Token Ring",
		"Sound",
	};
	code &= ~0x80;
	if (code > 7)
		return "";
	return onboard_type[code];
}

const char *dmi_mgmt_dev_type(u8 code)
{
	static const char *type[]={
		"",
		"Other",
		"Unknown",
		"LM75",
		"LM78",
		"LM79",
		"LM80",
		"LM81",
		"ADM9240",
		"DS1780",
		"MAX1617",
		"GL518SM",
		"W83781D",
		"HT82H791",
	};
	
	if (code > 0x0d)
		return "";
	return type[code];
}

const char *dmi_mgmt_addr_type(u8 code)
{
	static const char *type[]={
		"",
		"Other",
		"Unknown",
		"I/O",
		"Memory",
		"SMBus",
	};
	
	if (code > 5)
		return "";
	return type[code];
}

const char *dmi_fan_type(u8 code)
{
	static const char *type[]={
		"",
		"Other",
		"Unknown",
		"Fan",
		"Centrifugal Blower",
		"Chip Fan",
		"Cabinet Fan",
		"Power Supply Fan",
		"Heat Pipe",
		"Integrated Refrigeration",
		"",
		"",
		"",
		"",
		"",
		"",
		"Active Cooling",
		"Passive Cooling",
	};
	
	if (code > 0x11)
		return "";
	return type[code];
}

const char *dmi_volt_loc(u8 code)
{
	static const char *type[]={
		"",
		"Other",
		"Unknown",
		"Processor",
		"Disk",
		"Peripheral Bay",
		"System Management Module",
		"Motherboard",
		"Memory Module",
		"Processor Module",
		"Power Unit",
		"Add-in Card",
	};
	
	if (code > 0x0b)
		return "";
	return type[code];
}

const char *dmi_temp_loc(u8 code)
{
	static const char *type[]={
		"Front Panel Board",
		"Back Panel Board",
		"Power System Board",
		"Drive Back Plane",
	};
	
	if (code <= 0x0b)
		return dmi_volt_loc(code);
	if (code <= 0x0f)
		return type[code - 0x0c];
	return "";
}

const char *dmi_status(u8 code)
{
	static const char *type[]={
		"",
		"Other",
		"Unknown",
		"OK",
		"Non-critical",
		"Critical",
		"Non-recoverable",
	};
	
	if (code > 6)
		return "";
	return type[code];
}

/* 3 dec. places */
const char *dmi_millivolt(u8 *data, int indx)
{
	static char buffer[20];
	short int d;

	if (data[indx+1] == 0x80 && data[indx] == 0)
		return "Unknown";
	d = data[indx+1] << 8 | data[indx];
	sprintf(buffer, "%0.3f", d / 1000.0);
	return buffer;
}

/* 2 dec. places */
const char *dmi_accuracy(u8 *data, int indx)
{
	static char buffer[20];
	short int d;

	if (data[indx+1] == 0x80 && data[indx] == 0)
		return "Unknown";
	d = data[indx+1] << 8 | data[indx];
	sprintf(buffer, "%0.2f", d / 100.0);
	return buffer;
}

/* 1 dec. place */
const char *dmi_temp(u8 *data, int indx)
{
	static char buffer[20];
	short int d;

	if (data[indx+1] == 0x80 && data[indx] == 0)
		return "Unknown";
	d = data[indx+1] << 8 | data[indx];
	sprintf(buffer, "%0.1f", d / 10.0);
	return buffer;
}

/* 0 dec. place */
const char *dmi_speed(u8 *data, int indx)
{
	static char buffer[20];
	short int d;

	if (data[indx+1] == 0x80 && data[indx] == 0)
		return "Unknown";
	d = data[indx+1] << 8 | data[indx];
	sprintf(buffer, "%d", d);
	return buffer;
}

		
static void dmi_table(int fd, u32 base, int len, int num)
{
	unsigned char *buf=malloc(len);
	struct dmi_header *dm;
	u8 *data, *next;
	int i=0;
	int r=0, r2=0;
		
	if(len==0)
	{
		fputs("dmi: no data\n", stderr);
		return;
	}
	
	if(buf==NULL)
	{
		perror("dmi: malloc");
		return;
	}
	if(lseek(fd, (long)base, SEEK_SET)==-1)
	{
		perror("dmi: lseek");
		return;
	}
	while(r2!=len && (r=read(fd, buf+r2, len-r2))!=0 && r!=-1)
		r2+=r;
	if(r==-1)
	{
		perror("dmi: read");
		return;
	}
	if(r==0)
	{
		fputs("dmi: read: Unexpected end of file\n", stderr);
		return;
	}
	data = buf;
	while(i<num && data+sizeof(struct dmi_header)<=(u8*)buf+len)
	{
		u32 u, u2;
		dm=(struct dmi_header *)data;
		printf("Handle 0x%04X\n\tDMI type %d, %d bytes.\n",
			dm->handle,
			dm->type, dm->length);
		
		/* we won't read beyond allocated memory */
		next=data+dm->length;
		while(next-buf+1<len && (next[0]!=0 || next[1]!=0))
			next++;
		next+=2;
		if(next-buf>len)
		{
			printf("\tIncomplete structure, abort decoding.\n");
			break;
		}
		
		switch(dm->type)
		{
			case  0:
				printf("\tBIOS Information Block\n");
			printf("\t\tVendor: %s\n", 
					dmi_string(dm, data[4]));
			printf("\t\tVersion: %s\n", 
					dmi_string(dm, data[5]));
			printf("\t\tRelease: %s\n",
					dmi_string(dm, data[8]));
			printf("\t\tBIOS base: 0x%04X0\n",
					data[7]<<8|data[6]);
			printf("\t\tROM size: %dK\n",
					64*(data[9]+1));
				printf("\t\tCapabilities:\n");
				u=data[13]<<24|data[12]<<16|data[11]<<8|data[10];		
				u2=data[17]<<24|data[16]<<16|data[15]<<8|data[14];
				printf("\t\t\tFlags: 0x%08X%08X\n",
					u2,u);
				break;
				
			case 1:
				printf("\tSystem Information Block\n");
			printf("\t\tVendor: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tProduct: %s\n",
					dmi_string(dm, data[5]));
			printf("\t\tVersion: %s\n",
					dmi_string(dm, data[6]));
			printf("\t\tSerial Number: %s\n",
					dmi_string(dm, data[7]));
				break;

			case 2:
				printf("\tBoard Information Block\n");
			printf("\t\tVendor: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tProduct: %s\n",
					dmi_string(dm, data[5]));
			printf("\t\tVersion: %s\n",
					dmi_string(dm, data[6]));
			printf("\t\tSerial Number: %s\n",
					dmi_string(dm, data[7]));
				break;

			case 3:
				printf("\tChassis Information Block\n");
			printf("\t\tVendor: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tChassis Type: %s\n",
			       dmi_chassis_type(data[5]));
			if (data[5] & 0x80)
				printf("\t\t\tLock present\n");
			printf("\t\tVersion: %s\n",
					dmi_string(dm, data[6]));
			printf("\t\tSerial Number: %s\n",
					dmi_string(dm, data[7]));
			printf("\t\tAsset Tag: %s\n",
					dmi_string(dm, data[8]));
				break;
			
		case 4:
			printf("\tProcessor\n");
			printf("\t\tSocket Designation: %s\n",
			       dmi_string(dm, data[4]));
			printf("\t\tProcessor Type: %s\n",
			       dmi_processor_type(data[5]));
			printf("\t\tProcessor Family: %s\n",
			       dmi_processor_family(data[6]));
			printf("\t\tProcessor Manufacturer: %s\n",
			       dmi_string(dm, data[7]));
			printf("\t\tProcessor Version: %s\n",
			       dmi_string(dm, data[0x10]));
			if (dm->length <= 0x20) break;
			printf("\t\tSerial Number: %s\n",
			       dmi_string(dm, data[0x20]));
			printf("\t\tAsset Tag: %s\n",
			       dmi_string(dm, data[0x21]));
			printf("\t\tVendor Part Number: %s\n",
			       dmi_string(dm, data[0x22]));
			break;
			
		case 5:
			printf("\tMemory Controller\n");
			break;
			
			case 6:
				printf("\tMemory Bank\n");
				printf("\t\tSocket: %s\n", dmi_string(dm, data[4]));
				if(data[5]!=0xFF)
				{
					printf("\t\tBanks: ");
					if((data[5]&0xF0)!=0xF0)
						printf("%d ",
							data[5]>>4);
					if((data[5]&0x0F)!=0x0F)
						printf("%d",
							data[5]&0x0F);
					printf("\n");
				}
				if(data[6])
					printf("\t\tSpeed: %dnS\n", data[6]);
				printf("\t\tType: ");
				dmi_decode_ram(data[8]<<8|data[7]);
				printf("\n");
				printf("\t\tInstalled Size: ");
				switch(data[9]&0x7F)
				{
					case 0x7D:
						printf("Unknown");break;
					case 0x7E:
						printf("Disabled");break;
					case 0x7F:
						printf("Not Installed");break;
					default:
						printf("%dMbyte",
							(1<<(data[9]&0x7F)));
				}
				if(data[9]&0x80)
					printf(" (Double sided)");
				printf("\n");
				printf("\t\tEnabled Size: ");
				switch(data[10]&0x7F)
				{
					case 0x7D:
						printf("Unknown");break;
					case 0x7E:
						printf("Disabled");break;
					case 0x7F:
						printf("Not Installed");break;
					default:
						printf("%dMbyte",
							(1<<(data[10]&0x7F)));
				}
				if(data[10]&0x80)
					printf(" (Double sided)");
				printf("\n");
				if((data[11]&4)==0)
				{
					if(data[11]&(1<<0))
						printf("\t\t*** BANK HAS UNCORRECTABLE ERRORS (BIOS DISABLED)\n");
					if(data[11]&(1<<1))
						printf("\t\t*** BANK LOGGED CORRECTABLE ERRORS AT BOOT\n");
				}
				break;
			case 7:
			{
				static const char *types[4]={
					"Internal ", "External ",
					"", ""};
				static const char *modes[4]={
					"write-through",
					"write-back",
					"",""};
					
				printf("\tCache\n");
				printf("\t\tSocket: %s\n",
					dmi_string(dm, data[4]));
				u=data[6]<<8|data[5];
				printf("\t\tL%d %s%sCache: ",
					1+(u&7), (u&(1<<3))?"socketed ":"",
					types[(u>>5)&3]);
				if(u&(1<<7))
					printf("%s\n",
						modes[(u>>8)&3]);
				else
					printf("disabled\n");
				printf("\t\tL%d Cache Size: ", 1+(u&7));
				dmi_cache_size(data[9]|data[10]<<8);
				printf("\t\tL%d Cache Maximum: ", 1+(u&7));
				dmi_cache_size(data[7]|data[8]<<8);
				printf("\t\tL%d Cache Type: ", 1+(u&7));
				dmi_decode_cache(data[13]);
				printf("\n");
			}
			break;

		case 8:
			printf("\tPort Connector\n");
			printf("\t\tInternal Designator: %s\n",
			       dmi_string(dm, data[4]));
			printf("\t\tInternal Connector Type: %s\n",
			       dmi_port_connector_type(data[5]));
			printf("\t\tExternal Designator: %s\n",
			       dmi_string(dm, data[6]));
			printf("\t\tExternal Connector Type: %s\n",
			       dmi_port_connector_type(data[7]));
			printf("\t\tPort Type: %s\n",
			       dmi_port_type(data[8]));
			break;
			
			
			
		case 9:
			printf("\tCard Slot\n");
			printf("\t\tSlot: %s\n", 
				dmi_string(dm, data[4]));
			printf("\t\tType: %s%s%s\n",
				dmi_bus_width(data[6]),
				dmi_card_size(data[8]),
				dmi_bus_name(data[5]));
			if(data[7]==3)
				printf("\t\tStatus: Available.\n");
			if(data[7]==4)
				printf("\t\tStatus: In use.\n");
			if(data[11]&0xFE)
				dmi_card_props(data[11]);
			break;
							
		case 10:
			printf("\tOn Board Devices Information\n");
			for (u=2; u*2+1<dm->length; u++) {
				printf("\t\tDescription: %s : %s\n",
				       dmi_string(dm, data[1+2*u]),
				       (data[2*u]) & 0x80 ?
				       "Enabled" : "Disabled");
				printf("\t\tType: %s\n",
				       dmi_onboard_type(data[2*u]));

			}

			break;
			
			
			case 11:
				printf("\tOEM Data\n");
			for(u=1;u<=data[4];u++)
					printf("\t\t%s\n", dmi_string(dm,u));
				break;
			case 12:
				printf("\tConfiguration Information\n");
			for(u=1;u<=data[4];u++)
					printf("\t\t%s\n", dmi_string(dm,u));
				break;
				
		case 13:
			printf("\tBIOS Language Information\n");
			printf("\t\tInstallable Languages: %u\n", data[4]);
			for (u=1; u<=data[4]; u++) {
				printf("\t\t\t%s\n", dmi_string(dm,u));
			}
			printf("\t\tCurrently Installed Language: %s\n", dmi_string(dm, data[21]));
			break;
			
		case 14:
			printf("\tGroup Associations\n");
			for (u=0; 3*u+7<dm->length; u++) {
				printf("\t\tGroup Name: %s\n",
				       dmi_string(dm,data[4]));
				printf("\t\t\tType: 0x%02x\n", *(data+5+(u*3)));
				printf("\t\t\tHandle: 0x%04x\n",
				       *(u16*)(data+6+(u*3)));
			}
			break;
			
			
			case 15:
				printf("\tEvent Log\n");
				printf("\t\tLog Area: %d bytes.\n",
					data[5]<<8|data[4]);
				printf("\t\tLog Header At: %d.\n",
					data[7]<<8|data[6]);
				printf("\t\tLog Data At: %d.\n",
					data[9]<<8|data[8]);
				printf("\t\tLog Type: %d.\n",
					data[10]);
				if(data[11]&(1<<0))
					printf("\t\tLog Valid: Yes.\n");
				if(data[11]&(1<<1))
					printf("\t\t**Log Is Full**.\n");
				break;
			
		case 16:
			printf("\tPhysical Memory Array\n");
			printf("\t\tLocation: %s\n",
				dmi_memory_array_location(data[4]));
			printf("\t\tUse: %s\n",
				dmi_memory_array_use(data[5]));
			printf("\t\tError Correction Type: %s\n",
				dmi_memory_array_error_correction_type(data[6]));
			u2 = data[10]<<24|data[9]<<16|data[8]<<8|data[7];
			printf("\t\tMaximum Capacity: ");
			if (u2 == 0x80000000)
				printf("Unknown\n");
			else
				printf("%u kB\n", u2);
			printf("\t\tError Information Handle: ");
			u = data[12]<<8|data[11];
			if (u == 0xffff) {
				printf("None\n");
			} else if (u == 0xfffe) {
				printf("Not Provided\n");
			} else {
				printf("0x%04X\n", u);
			}
			printf("\t\tNumber of Devices: %u\n", data[14]<<8|data[13]);
			break;
		case 17:
			printf("\tMemory Device\n");
			printf("\t\tArray Handle: 0x%04X\n", data[5]<<8|data[4]);
			printf("\t\tError Information Handle: ");
			u = data[7]<<8|data[6];
			if (u == 0xffff) {
				printf("None\n");
			} else if (u == 0xfffe) {
				printf("Not Provided\n");
			} else {
				printf("0x%04X\n", u);
			}
			u = data[9]<<8|data[8];
			printf("\t\tTotal Width: ");
			if (u == 0xffff)
				printf("Unknown\n");
			else
				printf("%u bits\n", u);
			u = data[11]<<8|data[10];
			printf("\t\tData Width: ");
			if (u == 0xffff)
				printf("Unknown\n");
			else
				printf("%u bits\n", u);
			u = data[13]<<8|data[12];
			printf("\t\tSize: ");
			if (u == 0xffff)
				printf("Unknown\n");
			else
				printf("%u %sbyte\n", (u&0x7fff), (u&0x8000) ? "K" : "M");
			printf("\t\tForm Factor: %s\n",
				dmi_memory_device_form_factor(data[14]));
			if (data[15] != 0) {
				printf("\t\tSet: ");
				if (data[15] == 0xff)
					printf("Unknown\n");
				else
					printf("0x%02X\n", data[15]);
			}
			printf("\t\tLocator: %s\n",
				dmi_string(dm, data[16]));
			printf("\t\tBank Locator: %s\n",
				dmi_string(dm, data[17]));
			printf("\t\tType: %s\n",
				dmi_memory_device_type(data[18]));
			u = data[20]<<8|data[19];
			if (u&0x1ffe)
				dmi_memory_device_detail(u);
			if (dm->length > 21) {
				u = data[22]<<8|data[21];
				printf("\t\tSpeed: ");
				if (u == 0)
					printf("Unknown\n");
				else
					printf("%u MHz (%.1f ns)\n", u, (1000.0/u));
			}
			if (dm->length > 23)
				printf("\t\tManufacturer: %s\n",
					dmi_string(dm, data[23]));
			if (dm->length > 24)
				printf("\t\tSerial Number: %s\n",
					dmi_string(dm, data[24]));
			if (dm->length > 25)
				printf("\t\tAsset Tag: %s\n",
					dmi_string(dm, data[25]));
			if (dm->length > 26)
				printf("\t\tPart Number: %s\n",
					dmi_string(dm, data[26]));
			break;
		case 18:
			printf("\t32-bit Memory Error Information\n");
			break;
		case 19:
			printf("\tMemory Array Mapped Address\n");
			break;
		case 20:
			printf("\tMemory Device Mapped Address\n");
			break;
		case 21:
			printf("\tBuilt-In Pointing Device\n");
			break;
		case 22:
			printf("\tPortable Battery\n");
			printf("\t\tLocation: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tManufacturer: %s\n",
					dmi_string(dm, data[5]));
			printf("\t\tManufacture Date: %s\n",
					dmi_string(dm, data[6]));
			printf("\t\tSerial Number: %s\n",
					dmi_string(dm, data[7]));
			printf("\t\tName: %s\n",
					dmi_string(dm, data[8]));
				break;

		case 23:
			printf("\tSystem Reset\n");
			break;
		case 24:
			printf("\tHardware Security\n");
			break;
		case 25:
			printf("\tSystem Power Controls\n");
			break;
		case 26:
			printf("\tVoltage Sensor\n");
			printf("\t\tDescription: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tDevice Location: %s\n",
			       dmi_volt_loc(data[5] & 0x1f));
			printf("\t\tDevice Status: %s\n",
			       dmi_status(data[5] >> 5));
			printf("\t\tMaximum Value: %s\n",
			        dmi_millivolt(data, 6));
			printf("\t\tMinimum Value: %s\n",
			        dmi_millivolt(data, 8));
			printf("\t\tResolution:    %s\n",
			        dmi_millivolt(data, 10));
			printf("\t\tTolerance:     %s\n",
			        dmi_millivolt(data, 12));
			printf("\t\tAccuracy:      %s\n",
			        dmi_accuracy(data, 14));
			if(dm->length > 0x14)
				printf("\t\tNominal Value: %s\n",
				        dmi_millivolt(data, 0x14));
			break;
		case 27:
			printf("\tCooling Device\n");
			printf("\t\tDevice Type: %s\n",
			       dmi_fan_type(data[6] & 0x1f));
			printf("\t\tDevice Status: %s\n",
			       dmi_status(data[6] >> 5));
			if(dm->length > 0x0c)
				printf("\t\tNominal Speed: %s\n",
				        dmi_speed(data, 0x0c));
			break;
		case 28:
			printf("\tTemperature Sensor\n");
			printf("\t\tDescription: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tDevice Location: %s\n",
			       dmi_temp_loc(data[5] & 0x1f));
			printf("\t\tDevice Status: %s\n",
			       dmi_status(data[5] >> 5));
			printf("\t\tMaximum Value: %s\n",
			        dmi_temp(data, 6));
			printf("\t\tMinimum Value: %s\n",
			        dmi_temp(data, 8));
			printf("\t\tResolution:    %s\n",
			        dmi_temp(data, 10));
			printf("\t\tTolerance:     %s\n",
			        dmi_temp(data, 12));
			printf("\t\tAccuracy:      %s\n",
			        dmi_accuracy(data, 14));
			if(dm->length > 0x14)
				printf("\t\tNominal Value: %s\n",
				        dmi_temp(data, 0x14));
			break;
		case 29:
			printf("\tCurrent Sensor\n");
			printf("\t\tDescription: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tDevice Location: %s\n",
			       dmi_volt_loc(data[5] & 0x1f));
			printf("\t\tDevice Status: %s\n",
			       dmi_status(data[5] >> 5));
			printf("\t\tMaximum Value: %s\n",
			        dmi_millivolt(data, 6));
			printf("\t\tMinimum Value: %s\n",
			        dmi_millivolt(data, 8));
			printf("\t\tResolution:    %s\n",
			        dmi_millivolt(data, 10));
			printf("\t\tTolerance:     %s\n",
			        dmi_millivolt(data, 12));
			printf("\t\tAccuracy:      %s\n",
			        dmi_accuracy(data, 14));
			if(dm->length > 0x14)
				printf("\t\tNominal Value: %s\n",
				        dmi_millivolt(data, 0x14));
			break;
		case 30:
			printf("\tOut-of-Band Remote Access\n");
			break;
		case 31:
			printf("\tBoot Integrity Services Entry Point\n");
			break;
		case 32:
			printf("\tSystem Boot Information\n");
			break;
		case 33:
			printf("\t64-bit Memory Error Information\n");
			break;
		case 34:
			printf("\tManagement Device\n");
			printf("\t\tDescription: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tDevice Type: %s\n",
			       dmi_mgmt_dev_type(data[5]));
			printf("\t\tAddress Type: %s\n",
			       dmi_mgmt_addr_type(data[6]));
			break;
		case 35:
			printf("\tManagement Device Component\n");
			printf("\t\tDescription: %s\n",
					dmi_string(dm, data[4]));
			printf("\t\tDevice Handle   : 0x%02x%02x\n",
			       data[6], data[5]);
			printf("\t\tComponent Handle: 0x%02x%02x\n",
			       data[8], data[7]);
			printf("\t\tThreshold Handle: 0x%02x%02x\n",
			       data[10], data[9]);
			break;
		case 36:
			printf("\tManagement Device Threshold Data\n");
			if (dm->length > 4) 
				dump_raw_data(data+4, dm->length-4);
			break;
		case 37:
			printf("\tMemory Channel\n");
			break;
		case 38:
			printf("\tIPMI Device\n");
			if (dm->length > 4) 
				dump_raw_data(data+4, dm->length-4);
			break;
		case 39:
			printf("\tPower Supply\n");
			if (dm->length > 4) 
				dump_raw_data(data+4, dm->length-4);
			break;
		case 126:
			printf("\tInactive\n");
			break;
			
		case 127:
			printf("\tEnd-of-Table\n");
			break;
			
		default:
			if (dm->length > 4) 
				dump_raw_data(data+4, dm->length-4);
			break;
			
			
			
		}
		data=next;
		i++;
	}
	if(i!=num)
	{
		printf("Wrong DMI structures count: %d announced, only %d decoded.\n",
			num, i);
	}
	if(data-(u8*)buf!=len)
	{
		printf("Wrong DMI structures length: %d bytes announced, %d bytes decoded.\n",
			len, data-(u8*)buf);
	}
	free(buf);
}


static const char *acpi_version(u8 code)
{
	static const char *version[]={
		" 1.0",
		"",
		" 2.0"
	};
	
	if(code<=2)
		return version[code];
	return "";
}


static const char *pnp_event_notification(u8 code)
{
	static const char *notification[]={
		"Not Supported",
		"Polling",
		"Asynchronous"
	};
	
	if(code<=2)
		return notification[code];
	return "";
}


static void pir_exclusive_irqs(u16 code)
{
	if(code==0)
		printf(" None");
	else
	{
		u8 a;
		for(a=0; a<16; a++)
			if(code&(1<<a))
				printf(" %u", a);
	}
}


static __inline__ int checksum(u8 *buf, int len)
{
	u8 sum=0;
	int a;
	
	for(a=0; a<len; a++)
		sum+=buf[a];
	return (sum==0);
}

int main(__attribute__ ((unused)) int argc, const char *argv[])
{
	u8 buf[48];
	int fd=open("/dev/mem", O_RDONLY);
	long fp=0xE0000L;
	u8 smmajver=0, smminver=0;
	
	if(sizeof(u8)!=1 || sizeof(u16)!=2 || sizeof(u32)!=4 || '\0'!=0)
	{
		fprintf(stderr,"%s: compiler incompatibility.\n", argv[0]);
		exit(255);
	}
	printf("# dmidecode %s\n", VERSION);
	if(fd==-1)
	{
		perror("/dev/mem");
		exit(1);
	}
	if(lseek(fd, fp, SEEK_SET)==-1)
	{
		perror("lseek");
		exit(1);
	}
		
	while( fp < 0xFFFFF)
	{
		ssize_t r=0, r2=0;
		
		while(r2!=16 && (r=read(fd, buf+r2, 16-r2))!=0 && r!=-1)
			r2+=r;
		if(r==-1)
		{
			perror("read");
			exit(1);
		}
		if(r==0)
		{
			fputs("read: Unexpected end of file\n", stderr);
			exit(1);
		}
		
		if(memcmp(buf, "_SM_", 4)==0)
			printf("SMBIOS %d.%d present.\n", smmajver=buf[6], smminver=buf[7]);
		else if(memcmp(buf, "_SYSID_", 7)==0)
		{
			/* complete buffer, compute checksum */
			read(fd, buf+16, 16); /* arbitrary, at least 1 */
			if(*(u16 *)(buf+8)<=32 && checksum(buf, *(u16 *)(buf+8)))
			{
				fp+=16;
				printf("SYSID present.\n");
				printf("\tRevision: %u\n",
					buf[16]);
				printf("%d structure%s.\n",
					*(u16 *)(buf+14), *(u16 *)(buf+14)>1?"s":"");
				printf("SYSID table at 0x%08X.\n",
					*(u32 *)(buf+10));
			}
			else
				lseek(fd, -16, SEEK_CUR);
		}
		else if(memcmp(buf, "_DMI_", 5)==0 && checksum(buf, 15))
		{
			u16 num=buf[13]<<8|buf[12];
			u16 len=buf[7]<<8|buf[6];
			u32 base=buf[11]<<24|buf[10]<<16|buf[9]<<8|buf[8];

			/* if version is 0.0, the real version is taken from _SM_ above */
			printf("DMI %d.%d present.\n",
				buf[14]?buf[14]>>4:smmajver, buf[14]?buf[14]&0x0F:smminver);
			printf("%d structures occupying %d bytes.\n",
				num, len);
			printf("DMI table at 0x%08X.\n",
				base);
			dmi_table(fd, base, len, num);

			/* dmi_table moved us far away */
			lseek(fd, fp+16, SEEK_SET);
		}
		else if(memcmp(buf, "$PnP", 4)==0)
		{
			/* complete buffer, compute checksum */
			read(fd, buf+16, 32); /* arbitrary, at least 1 */
			if(buf[5]<=48 && checksum(buf, buf[5]))
			{
				fp+=32;
				printf("PNP %u.%u present.\n",
					buf[4]>>4, buf[4]&0x0F);
				printf("\tEvent Notification: %s\n",
					pnp_event_notification((*(u16 *)(buf+6))&0x03));
				if(((*(u16 *)(buf+6))&0x03)==1)
					printf("\tEvent Notification Flag Address: 0x%08X\n",
						*(u32 *)(buf+9));
				printf("\tReal Mode Code Address: %04X:%04X\n",
					*(u16 *)(buf+0x0F), *(u16 *)(buf+0x0D));
				printf("\tReal Mode Data Address: %04X:0000\n",
					*(u16 *)(buf+0x1B));
				printf("\tProtected Mode Code Address: 0x%08X\n",
					*(u32 *)(buf+0x13)+*(u16 *)(buf+0x11));
				printf("\tProtected Mode Data Address: 0x%08X\n",
					*(u32 *)(buf+0x1D));
				if(*(u32 *)(buf+0x17)!=0)
					printf("\tOEM Device Identifier: 0x%08X\n",
						*(u32 *)(buf+0x17));
			}
			else
				lseek(fd, -32, SEEK_CUR);
		}
		else if(memcmp(buf, "RSD PTR ", 8)==0)
		{
			/* complete buffer, compute checksum */
			read(fd, buf+16, 4);
			if(checksum(buf, 20))
			{
				lseek(fd, 12, SEEK_CUR);
				fp+=16;
				printf("ACPI%s present.\n",
					acpi_version(buf[15]));
				printf("\tOEM ID: ");
				fwrite(buf+9, 6, 1, stdout);
				printf("\n");
				printf("RSD table at 0x%08X.\n",
					*(u32 *)(buf+16));
			}
			else
				lseek(fd, -4, SEEK_CUR);
		}
		else if(memcmp(buf, "$SNY", 4)==0)
		{
			/* complete buffer, compute checksum */
			read(fd, buf+16, 16);
			if(buf[5]<=32 && checksum(buf, buf[5]))
			{
				fp+=16;
				printf("Sony system detected (?).\n");
			}
			else
				lseek(fd, -16, SEEK_CUR);
		}
		else if(memcmp(buf, "_32_", 4)==0)
		{
			/* complete buffer, reposition file, compute checksum */
			read(fd, buf+16, 32);
			lseek(fd, -32, SEEK_CUR);
			if(buf[9]>=1 && buf[9]<=3 && checksum(buf, buf[9]<<4))
			{
				printf("BIOS32 Service Directory present.\n");
				printf("\tCalling Interface Address: 0x%08X\n",
					*(u32 *)(buf+4));
				lseek(fd, (buf[9]-1)<<4, SEEK_CUR);
				fp+=(buf[9]-1)<<4;
			}
		}
		else if(memcmp(buf, "$PIR", 4)==0)
		{
			/* complete buffer, reposition file */
			read(fd, buf+16, 16);
			lseek(fd, -16, SEEK_CUR);
			if(buf[5]!=0 && buf[30]==0)
			{
				printf("PCI Interrupt Routing %u.%u present.\n",
					buf[5], buf[4]);
				printf("\tTable Size: %u bytes\n",
					*(u16 *)(buf+6));
				printf("\tRouter ID: %02x:%02x.%1x\n",
					buf[8], buf[9]>>3, buf[9]&0x07);
				printf("\tExclusive IRQs:");
				pir_exclusive_irqs(*(u16 *)(buf+10));
				printf("\n");
				if(*(u32 *)(buf+12)!=0)
					printf("\tCompatible Router: %04x:%04x\n",
						*(u16 *)(buf+12), *(u16 *)(buf+14));
				if(*(u32 *)(buf+16)!=0)
					printf("\tMiniport Data: 0x%08X\n",
						*(u32 *)(buf+16));
			}
		}
		fp+=16;
	}

	if(close(fd)==-1)
	{
		perror("/dev/mem");
		exit(1);
	}

	return 0;
}
