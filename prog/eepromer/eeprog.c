/***************************************************************************
    copyright            : (C) by 2002-2003 Stefano Barbato
    email                : stefano@codesink.org

    $Id$
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "24cXX.h"

#define VERSION 	"0.7.4"

#define ENV_DEV		"EEPROG_DEV"
#define ENV_I2C_ADDR	"EEPROG_I2C_ADDR"

#define usage_if(a) do { do_usage_if( a , __LINE__); } while(0);
void do_usage_if(int b, int line)
{
const static char *eeprog_usage = 
"Usage: eeprog [ -r addr[:count] | -w addr ]  /dev/i2c-N  i2c-address\n" 
"	-8		Use 8bit address mode for 24c0x...24C16\n"
"	-r addr[:count]	Read [count] (1 if omitted) bytes from [addr]\n" 
"			and print them to the standard output\n" 
"	-w addr		Write input (stdin) at mem address [addr]\n"
"	-x		Set the hex output mode\n" 
"	-d		Dummy mode, display what *would* have been done\n" 
"	-f		Disable warnings and don't ask confirmation\n"
"\n"
"The following environment variables could be set instead of the command\n"
"line arguments:\n"
"	EEPROG_DEV		device name(/dev/i2c-N)\n"
"	EEPROG_I2C_ADDR		i2c-address\n"
"\n"
"	Examples\n"
"	1- read 64 bytes from the EEPROM at address 0x54 on bus 0 starting\n"
"	   at address 123 (decimal)\n"
"		eeprog /dev/i2c-0 0x54 -r 123:64\n"
"	2- prints the hex codes of the first 32 bytes read from bus 1 \n"
"	   at address 0x22\n"
"		eeprog /dev/i2c-1 0x51 -x -r 0x22:0x20\n"
"	3- write the current timestamp at address 0x200 of the EEPROM on \n"
"	   bus 0 at address 0x33 \n"
"		date | eeprog /dev/i2c-0 0x33 -w 0x200\n";

	if(!b)
		return;
	fprintf(stderr, "%s\n[line %d]\n", eeprog_usage, line);
	exit(1);
}


#define die_if(a, msg) do { do_die_if( a , msg, __LINE__); } while(0);
void do_die_if(int b, char* msg, int line)
{
	if(!b)
		return;
	fprintf(stderr, "Error at line %d: %s\n", line, msg);
	//fprintf(stderr, "	sysmsg: %s\n", strerror(errno));
	exit(1);
}


void parse_arg(char *arg, int* paddr, int *psize)
{
	char *end;
	*paddr = strtoul(arg, &end, 0);
	if(*end == ':')
		*psize = strtoul(++end, 0, 0);
}

int confirm_action()
{
	fprintf(stderr, 
	"\n"
	"__________________________________WARNING_______________________________\n"
	"Erroneously writing to a system EEPROM (like DIMM SPD modules) can break your\n"
	"system.  It will NOT boot any more so you'll not be able to fix it.\n"
	"\n"
	"Reading from 8bit EEPROMs (like that in your DIMM) without using the -8 switch\n"
	"can also UNEXPECTEDLY write to them, so be sure to use the -8 command param when\n"
	"required.\n"
	"\n"
	"Use -f to disable this warning message\n"
	"\n"
	"Press ENTER to continue or hit CTRL-C to exit\n"
	"\n"
	);
	getchar();
	return 1; 
}

int read_from_eeprom(struct eeprom *e, int addr, int size, int hex)
{
	int c=1;

	if(!hex)
	{
		putchar(eeprom_read_byte(e, addr));
		while(--size)
		{
			putchar(eeprom_read_current_byte(e));
			fflush(stdout);
		}
		return 0;
	}
	// hex print out
	printf("\n %.4x|  %.2x ", addr, eeprom_read_byte(e, addr));
	while(--size)
	{
		addr++;
		if( (c % 16) == 0 ) 
			printf("\n %.4x|  ", addr);
		else if( (c % 8) == 0 ) 
			printf("  ");
		c++;
		printf("%.2x ", eeprom_read_current_byte(e));
	}
	printf("\n\n");
	return 0;
}

int write_to_eeprom(struct eeprom *e, int addr)
{
	int c;
	while((c = getchar()) != EOF)
	{
		fprintf(stderr, ".");
		fflush(stdout);
		eeprom_write_byte(e, addr++, c);
	}
	printf("\n\n");
	return 0;
}

int main(int argc, char** argv)
{
	struct eeprom e;
	int ret, op, i2c_addr, memaddr, size, want_hex, dummy, force;
	char *device, *arg = 0, *i2c_addr_s;
	struct stat st;
	int eeprom_type = EEPROM_TYPE_16BIT_ADDR;

	op = want_hex = dummy = force = 0;

	fprintf(stderr, "eeprog %s, a 24Cxx EEPROM reader/writer\n", VERSION);
	fprintf(stderr, 
		"Copyright (c) 2003 by Stefano Barbato - All rights reserved.\n");
	while((ret = getopt(argc, argv, "8fr:w:xd")) != -1)
	{
		switch(ret)
		{
		case 'x':
			want_hex++;
			break;
		case 'd':
			dummy++;
			break;
		case '8':
			eeprom_type = EEPROM_TYPE_8BIT_ADDR;
			break;
		case 'f':
			force++;
			break;
		default:
			die_if(op != 0, "Both read and write requested"); 
			arg = optarg;
			op = ret;
		}
	}
	usage_if(op == 0); // no switches 
	// set device and i2c_addr reading from cmdline or env
	device = i2c_addr_s = 0;
	switch(argc - optind)
	{
	case 0:
		device = getenv(ENV_DEV);
		i2c_addr_s = getenv(ENV_I2C_ADDR);
		break;
	case 1:
		if(stat(argv[optind], &st) != -1)
		{
			device = argv[optind];
			i2c_addr_s = getenv(ENV_I2C_ADDR);
		} else {
			device = getenv(ENV_DEV);
			i2c_addr_s = argv[optind];
		}
		break;
	case 2:
		device = argv[optind++];
		i2c_addr_s = argv[optind];
		break;
	default:
		usage_if(1);
	}
	usage_if(!device || !i2c_addr_s);
	i2c_addr = strtoul(i2c_addr_s, 0, 0);

	fprintf(stderr, "  Bus: %s, Address: 0x%x, Mode: %dbit\n", 
			device, i2c_addr, 
			(eeprom_type == EEPROM_TYPE_8BIT_ADDR ? 8 : 16) );
	if(dummy)
	{
		fprintf(stderr, "Dummy mode selected, nothing done.\n");
		return 0;
	}
	die_if(eeprom_open(device, i2c_addr, eeprom_type, &e) < 0, 
			"unable to open eeprom device file (check that the file exists and that it's readable)");
	switch(op)
	{
	case 'r':
		if(force == 0)
			confirm_action();
		size = 1; // default
		parse_arg(arg, &memaddr, &size);
		fprintf(stderr, 
			"  Reading %d bytes from 0x%x\n", size, memaddr);
		read_from_eeprom(&e, memaddr, size, want_hex);
		break;
	case 'w':
		if(force == 0)
			confirm_action();
		parse_arg(arg, &memaddr, &size);
		fprintf(stderr, 
			"  Writing stdin starting at address 0x%x\n", memaddr);
		write_to_eeprom(&e, memaddr);
		break;
	default:
		usage_if(1);
		exit(1);
	}
	eeprom_close(&e);

	return 0;
}

