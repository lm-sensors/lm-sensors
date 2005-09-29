/*
    i2cget.c - A user-space program to read an I2C register.
    Copyright (C) 2005       Jean Delvare <khali@linux-fr.org>

    Based on i2cset.c:
    Copyright (C) 2001-2003  Frodo Looijaard <frodol@dds.nl>, and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (C) 2004-2005  Jean Delvare <khali@linux-fr.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "i2cbusses.h"
#include "i2c-dev.h"
#include "version.h"

void help(void) __attribute__ ((noreturn));

void help(void)
{
	fprintf(stderr, "Syntax: i2cget [-y] I2CBUS CHIP-ADDRESS [DATA-ADDRESS "
	        "[MODE]]\n"
	        "        i2cget -V\n"
	        "  MODE can be: 'b' (read byte data, default)\n"
	        "               'w' (read word data)\n"
	        "               'c' (write byte/read byte)\n"
		"  If DATA-ADDRESS is omitted, a single read byte command is "
		"issued\n"
	        "  Append 'p' to MODE for PEC checking\n"
	        "  I2CBUS is an integer\n");
	print_i2c_busses(0);
	exit(1);
}

int check_funcs(int file, int i2cbus, int size, int daddress, int pec)
{
	long funcs;

	/* check adapter functionality */
	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
		fprintf(stderr, "Error: Could not get the adapter "
		        "functionality matrix: %s\n", strerror(errno));
		return -1;
	}

	switch (size) {
	case I2C_SMBUS_BYTE:
		if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
			        "not have read byte capability\n", i2cbus);
			return -1;
		}
		if (daddress >= 0
		 && !(funcs & I2C_FUNC_SMBUS_WRITE_BYTE)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
			        "not have write byte capability\n", i2cbus);
			return -1;
		}
		break;

	case I2C_SMBUS_BYTE_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
			        "not have read byte data capability\n", i2cbus);
			return -1;
		}
		break;

	case I2C_SMBUS_WORD_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_WORD_DATA)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
			        "not have read word data capability\n", i2cbus);
			return -1;
		}
		break;
	}

	if (pec
	 && !(funcs & (I2C_FUNC_SMBUS_HWPEC_CALC | I2C_FUNC_I2C))) {
		fprintf(stderr, "Warning: Adapter for i2c bus %d does "
		        "not seem to support PEC\n", i2cbus);
	}

	return 0;
}

int set_slave(int file, int address)
{
	/* use FORCE so that we can read registers even when
	   a driver is also running */
	if (ioctl(file, I2C_SLAVE_FORCE, address) < 0) {
		fprintf(stderr, "Error: Could not set address to %d: %s\n",
		        address, strerror(errno));
		return -1;
	}

	return 0;
}

int confirm(const char *filename, int address, int size, int daddress, int pec)
{
	char s[2];
	int dont = 0;

	fprintf(stderr, "WARNING! This program can confuse your I2C "
		"bus, cause data loss and worse!\n");

	/* Don't let the user break his/her EEPROMs */
	if (address >= 0x50 && address <= 0x57 && pec) {
		fprintf(stderr, "STOP! EEPROMs are I2C devices, not "
			"SMBus devices. Using PEC\non I2C devices may "
			"result in unexpected results, such as\n"
			"trashing the contents of EEPROMs. We can't "
			"let you do that, sorry.\n");
		return 0;
	}

	if (size == I2C_SMBUS_BYTE && daddress >= 0 && pec) {
		fprintf(stderr, "WARNING! All I2C chips and some SMBus chips "
		        "will interpret a write\nbyte command with PEC as a"
		        "write byte data command, effectively writing a\n"
		        "value into a register!\n");
		dont++;
	}

	fprintf(stderr, "I will read from device file %s, chip "
		"address 0x%02x, ", filename, address);
	if (daddress < 0)
		fprintf(stderr, "current data\naddress");
	else
		fprintf(stderr, "data address\n0x%02x", daddress);
	fprintf(stderr, ", using %s.\n",
		size == I2C_SMBUS_BYTE ? (daddress < 0 ?
		"read byte" : "write byte/read byte") :
		size == I2C_SMBUS_BYTE_DATA ? "read byte data" :
		"read word data");
	if (pec)
		fprintf(stderr, "PEC checking enabled.\n");

	fprintf(stderr, "Continue? [%s] ", dont ? "y/N" : "Y/n");
	fflush(stderr);
	fgets(s, 2, stdin);
	if ((s[0] != '\n' || dont) && s[0] != 'y' && s[0] != 'Y') {
		fprintf(stderr, "Aborting on user request.\n");
		return 0;
	}

	return 1;
}

int main(int argc, char *argv[])
{
	char *end;
	int res, i2cbus, address, file;
	int size = I2C_SMBUS_BYTE_DATA;
	int daddress;
	char filename[20];
	int pec = 0;
	int flags = 0;
	int yes = 0, version = 0;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
		case 'V': version = 1; break;
		case 'y': yes = 1; break;
		default:
			fprintf(stderr, "Warning: Unsupported flag "
				"\"-%c\"!\n", argv[1+flags][1]);
			help();
			exit(1);
		}
		flags++;
	}

	if (version) {
		fprintf(stderr, "i2cget version %s\n", LM_VERSION);
		exit(0);
	}

	if (argc - flags < 3)
		help();

	i2cbus = strtol(argv[flags+1], &end, 0);
	if (*end || i2cbus < 0 || i2cbus > 0x3f) {
		fprintf(stderr, "Error: I2CBUS argument invalid!\n");
		help();
	}

	address = strtol(argv[flags+2], &end, 0);
	if (*end || address < 3 || address > 0x77) {
		fprintf(stderr, "Error: Chip address invalid!\n");
		help();
	}

	if (!(flags+3 < argc)) {
		size = I2C_SMBUS_BYTE;
		daddress = -1;
	} else {
		daddress = strtol(argv[flags+3], &end, 0);
		if (*end || daddress < 0 || daddress > 0xff) {
			fprintf(stderr, "Error: Data address invalid!\n");
			help();
		}
	}

	if (flags+4 < argc) {
		switch (argv[flags+4][0]) {
		case 'b': size = I2C_SMBUS_BYTE_DATA; break;
		case 'w': size = I2C_SMBUS_WORD_DATA; break;
		case 'c': size = I2C_SMBUS_BYTE; break;
		default:
			fprintf(stderr, "Error: Invalid mode!\n");
			help();
		}
		pec = argv[flags+4][1] == 'p';
	}

	file = open_i2c_dev(i2cbus, filename, 0);
	if (file < 0
	 || check_funcs(file, i2cbus, size, daddress, pec)
	 || set_slave(file, address))
		exit(1);

	if (!yes && !confirm(filename, address, size, daddress, pec))
	 	exit(0);

	if (pec && ioctl(file, I2C_PEC, 1) < 0) {
		fprintf(stderr, "Error: Could not set PEC: %s\n",
		        strerror(errno));
		exit(1);
	}

	switch (size) {
	case I2C_SMBUS_BYTE:
		if (daddress >= 0) {
			res = i2c_smbus_write_byte(file, daddress);
			if (res < 0)
				fprintf(stderr, "Warning - write failed\n");
		}
		res = i2c_smbus_read_byte(file);
		break;
	case I2C_SMBUS_WORD_DATA:
		res = i2c_smbus_read_word_data(file, daddress);
		break;
	default: /* I2C_SMBUS_BYTE_DATA */
		res = i2c_smbus_read_byte_data(file, daddress);
	}
	close(file);

	if (res < 0) {
		fprintf(stderr, "Error: Read failed\n");
		exit(2);
	}

	printf("0x%0*x\n", size == I2C_SMBUS_WORD_DATA ? 4 : 2, res);

	exit(0);
}
