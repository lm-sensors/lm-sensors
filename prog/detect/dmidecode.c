/*
 *	DMI decode rev 1.2
 *
 *	(C) 2000,2001 Alan Cox <alan@redhat.com>
 *
 *      2-July-2001 Matt Domsch <Matt_Domsch@dell.com>
 *      Additional structures displayed per SMBIOS 2.3.1 spec
 *
 *	Licensed under the GNU Public license. If you want to use it in with
 *	another license just ask.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static void
dump_raw_data(void *data, unsigned int length)
{
	unsigned char buffer1[80], buffer2[80], *b1, *b2, c;
	unsigned char *p = data;
	unsigned long column=0;
	unsigned int length_printed = 0;
	const unsigned char maxcolumn = 16;
	while (length_printed < length) {
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

static char *dmi_string(struct dmi_header *dm, u8 s)
{
	u8 *bp=(u8 *)dm;
	if (!s) return NULL;
	
	bp+=dm->length;
	while(s>1)
	{
		bp+=strlen(bp);
		bp++;
		s--;
	}
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

static char *dmi_bus_name(u8 num)
{
	static char *bus[]={
		"",
		"",
		"",
		"ISA ",
		"MCA ",
		"EISA ",
		"PCI ",
		"PCMCIA "
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
	static char *jpbus[]={
		"PC98/C20",
		"PC98/C24",
		"PC98/E",
		"PC98/LocalBus",
		"PC98/Card"
	};
	
	if(num<=0x12)
		return bus[num];
	if(num>=0xA0 && num<0xA5)
		return jpbus[num];
	return "";
}

static char *dmi_bus_width(u8 code)
{
	static char *width[]={
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

static char *dmi_card_size(u8 v)
{
	if(v==2)
		return("Short ");
	if(v==3)
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
		
static char *dmi_chassis_type(u8 code)
{
	static char *chassis_type[]={
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

static char *dmi_port_connector_type(u8 code)
{
	static char *connector_type[]={
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
	
	if (code > 0xA4)
		return "";
	return connector_type[code];
	
}

static char *dmi_port_type(u8 code)
{
	static char *port_type[]={
		"None",
		"Parallel Port XT/AT Compatible",
		"Parallel Port PS/2",
		"Parallel Port ECP",
		"Parallel Port EPP",
		"Parallel Port ECP/EPP",
		"Serial Port XT/AT Compatible",
		"Serial Port 16450 Compatible",
		"Serial Port 16650 Compatible",
		"Serial Port 16650A Compatible",
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
	
	if (code > 0xA1)
		return "";
	return port_type[code];
	
}

static char *dmi_processor_type(u8 code)
{
	static char *processor_type[]={
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
	
	if (code > 0xA1)
		return "";
	return processor_type[code];
}

static char *dmi_processor_family(u8 code)
{
	static char *processor_family[]={
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

static char *dmi_onboard_type(u8 code)
{
	static char *onboard_type[]={
		"",
		"Other",
		"Unknown",
		"Video",
		"SCSI Controller",
		"Ethernet",
		"Token Ring",
		"Sound",
	};
	code &= 0x80;
	if (code > 7)
		return "";
	return onboard_type[code];
}

		
static void dmi_table(int fd, u32 base, int len, int num)
{
	char *buf=malloc(len);
	struct dmi_header *dm;
	u8 *data;
	int i=0;
		
	if(lseek(fd, (long)base, 0)==-1)
	{
		perror("dmi: lseek");
		return;
	}
	if(read(fd, buf, len)!=len)
	{
		perror("dmi: read");
		return;
	}
	data = buf;
	while(i<num)
	{
		u32 u;
		u32 u2;
		dm=(struct dmi_header *)data;
		printf("Handle 0x%04X\n\tDMI type %d, %d bytes.\n",
			dm->handle,
			dm->type, dm->length);
		
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
					64*data[9]);
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
				dmi_decode_ram(data[7]);
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
				static char *types[4]={
					"Internal ", "External ",
					"", ""};
				static char *modes[4]={
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
				dmi_cache_size(data[7]|data[8]<<8);
				printf("\t\tL%d Cache Maximum: ", 1+(u&7));
				dmi_cache_size(data[9]|data[10]<<8);
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
			for (u=0; u<((dm->length - 4)/2); u++) {
				printf("\t\tDescription: %s : %s\n",
				       dmi_string(dm, data[5+(2*u)]),
				       (data[4+(2*u)]) & 0x80 ?
				       "Enabled" : "Disabled");
				printf("\t\tType: %s\n",
				       dmi_onboard_type(data[4+(2*u)]));

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
			break;
			
		case 14:
			printf("\tGroup Associations\n");
			for (u=0; u<(dm->length - 5)/3 ; u++) {
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
			break;
		case 17:
			printf("\tMemory Device\n");
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
		case 24:
			printf("\tHardware Security\n");
			break;
		case 25:
			printf("\tSystem Power Controls\n");
			break;
		case 32:
			printf("\tSystem Boot Information\n");
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
		data+=dm->length;
		while(*data || data[1])
			data++;
		data+=2;
		i++;
	}
	free(buf);
}


char key[8]={'R','S','D',' ','P','T','R',' '};

char zot[16];

int main(int argc, char *argv[])
{
	unsigned char buf[20];
	int fd=open("/dev/mem", O_RDONLY);
	long fp=0xE0000L;
	if(fd==-1)
	{
		perror("/dev/mem");
		exit(1);
	}
	if(lseek(fd,fp,0)==-1)
	{
		perror("seek");
		exit(1);
	}
		

	fp -= 16;
	
	while( fp < 0xFFFFF)
	{
		fp+=16;
		if(read(fd, buf, 16)!=16)
			perror("read");
//		if(memcmp(buf, zot, 16)==0)
//			printf("*");
		if(memcmp(buf, "_SM_", 4)==0) {
			printf("SMBIOS %d.%d present.\n", buf[6], buf[7]);
		}
		
		if(memcmp(buf, "_SYSID_", 7)==0)
			printf("SYSID present.\n");
		if(memcmp(buf, "_DMI_", 5)==0)
		{
			u16 num=buf[13]<<8|buf[12];
			u16 len=buf[7]<<8|buf[6];
			u32 base=buf[11]<<24|buf[10]<<16|buf[9]<<8|buf[8];

			printf("DMI %d.%d present.\n",
				buf[14]>>4, buf[14]&0x0F);
			printf("%d structures occupying %d bytes.\n",
				buf[13]<<8|buf[12],
				buf[7]<<8|buf[6]);
			printf("DMI table at 0x%08X.\n",
				buf[11]<<24|buf[10]<<16|buf[9]<<8|buf[8]);
			dmi_table(fd, base,len, num);
		}
		if(memcmp(buf, "$PnP", 4)==0)
			printf("PNP BIOS present.\n");
		if(memcmp(buf, key, 8)==0)
		{
			int a;
			unsigned char sum=0;
			unsigned int i=0, checksum=0;
			printf("RSD PTR found at 0x%lX\n", fp);
			for (i=0; i<20; i++) checksum += buf[i];
			if (checksum != 0) {
				printf("checksum failed.\n");
			}

			if(buf[15]!=0)
			{
				printf("Reserved check failed.\n");
			}
			printf("OEM ");
			fwrite(buf+9, 6, 1, stdout);
			printf("\n");
			read(fd,buf+16,4);
			lseek(fd, -4, 1);
			for(a=0;a<20;a++)
				sum+=buf[a];
			if(sum!=0)
				printf("Bad checksum.\n");
		}
	}
	close(fd);
	return 0;
}
