/*
    i2cset.c - A user-space program to write an I2C register.
    Copyright (C) 2001-2003  Frodo Looijaard <frodol@dds.nl>, and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (C) 2004       The lm_sensors group

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
#include <fcntl.h>
#include "i2c-dev.h"
#include "version.h"

void print_i2c_busses(int);
void help(void) __attribute__ ((noreturn));

void help(void)
{
	fprintf(stderr, "Syntax: i2cset I2CBUS CHIP-ADDRESS DATA-ADDRESS "
	        "VALUE [MODE]\n");
	fprintf(stderr, "  MODE is 'b[yte]' or 'w[ord]' (default b)\n");
	fprintf(stderr, "  I2CBUS is an integer\n");
	print_i2c_busses(0);
	exit(1);
}

int main(int argc, char *argv[])
{
	char *end;
	int res, i2cbus, address, size, file;
	int value, daddress;
	int e1, e2;
	char filename1[20];
	char filename2[20];
	char *filename;
	long funcs;

	if (argc >= 2 && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "-V"))) {
		fprintf(stderr, "i2cset version %s\n", LM_VERSION);
		exit(1);
	}

	if (argc < 5)
		help();

	i2cbus = strtol(argv[1], &end, 0);
	if (*end || i2cbus < 0 || i2cbus > 0x3f) {
		fprintf(stderr, "Error: I2CBUS argument invalid!\n");
		help();
	}

	address = strtol(argv[2], &end, 0);
	if (*end || address < 0 || address > 0x7f) {
		fprintf(stderr, "Error: Chip address invalid!\n");
		help();
	}

	daddress = strtol(argv[3], &end, 0);
	if (*end || daddress < 0 || daddress > 0xff) {
		fprintf(stderr, "Error: Data address invalid!\n");
		help();
	}

	value = strtol(argv[4], &end, 0);
	if (*end) {
		fprintf(stderr, "Error: Data value invalid!\n");
		help();
	}

	if (argc < 6) {
		fprintf(stderr, "No size specified (using byte-data access)\n");
		size = I2C_SMBUS_BYTE_DATA;
	} else if (!strcmp(argv[5], "b"))
		size = I2C_SMBUS_BYTE_DATA;
	else if (!strcmp(argv[5], "w"))
		size = I2C_SMBUS_WORD_DATA;
	else {
		fprintf(stderr, "Error: Invalid mode!\n");
		help();
	}

	if (value < 0
	 || (size == I2C_SMBUS_BYTE_DATA && value > 0xff)
	 || (size == I2C_SMBUS_WORD_DATA && value > 0xffff)) {
		fprintf(stderr, "Error: Data value out of range!\n");
		help();
	}

	/*
	 * Try both variants and give the correct error message
	 * upon failure
	 */

	sprintf(filename1, "/dev/i2c-%d", i2cbus);
	sprintf(filename2, "/dev/i2c/%d", i2cbus);
	if ((file = open(filename1, O_RDWR)) < 0) {
		e1 = errno;
		if ((file = open(filename2, O_RDWR)) < 0) {
			e2 = errno;
			if (e1 == ENOENT && e2 == ENOENT) {
				fprintf(stderr, "Error: Could not open file "
				        "`%s' or `%s': %s\n", filename1,
				        filename2, strerror(ENOENT));
			}
			if (e1 != ENOENT) {
				fprintf(stderr, "Error: Could not open file "
				        ":`%s': %s\n", filename1,
				        strerror(e1));
				if (e1 == EACCES)
					fprintf(stderr, "Run as root?\n");
			}
			if (e2 != ENOENT) {
				fprintf(stderr, "Error: Could not open file "
				        "`%s' : %s\n", filename2,
				        strerror(e2));
				if (e2 == EACCES)
					fprintf(stderr, "Run as root?\n");
			}
			exit(1);
		} else {
			filename = filename2;
		}
	} else {
		filename = filename1;
	}

	/* check adapter functionality */
	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
		fprintf(stderr, "Error: Could not get the adapter "
		        "functionality matrix: %s\n", strerror(errno));
		exit(1);
	}

	switch (size) {
	case I2C_SMBUS_BYTE_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
			        "not have byte write capability\n", i2cbus);
			exit(1);
		}
		break;

	case I2C_SMBUS_WORD_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_WORD_DATA)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
			        "not have word write capability\n", i2cbus);
			exit(1);
		}
		break;
	}

	/* use FORCE so that we can write registers even when
	   a driver is also running */
	if (ioctl(file, I2C_SLAVE_FORCE, address) < 0) {
		fprintf(stderr, "Error: Could not set address to %d: %s\n",
		        address, strerror(errno));
		exit(1);
	}

	fprintf(stderr, "WARNING! This program can confuse your I2C bus, "
	        "cause data loss and worse!\n");
	if (address >= 0x50 && address <= 0x57) {
		fprintf(stderr, "DANGEROUS!! Writing to a serial EEPROM on "
		        "a memory DIMM\nmay render your memory USELESS and "
		        "make your system UNBOOTABLE!!!\nAre you SURE that "
		        "you want to write to the chip at address 0x%02x? "
		        "(y/N) ", address);
		res = getchar();
		if (res != 'y' && res != 'Y')
			exit(1);
	}

	fprintf(stderr, "I will write to device file %s, chip address 0x%02x, "
	        "data address\n0x%02x, data 0x%02x, mode %s.\n", filename,
	        address, daddress, value, size == I2C_SMBUS_BYTE_DATA ?
	        "byte" : "word");
	fprintf(stderr, "You have five seconds to reconsider and press "
	        "CTRL-C!\n\n");
	sleep(5);

	e1 = 0;
	if (size == I2C_SMBUS_WORD_DATA) {
		res = i2c_smbus_write_word_data(file, daddress, value);
	} else {
		res = i2c_smbus_write_byte_data(file, daddress, value);
	}
	if (res < 0) {
		fprintf(stderr, "Warning - write failed\n");
		e1++;
	}

	if (size == I2C_SMBUS_WORD_DATA) {
		res = i2c_smbus_read_word_data(file, daddress);
	} else {
		res = i2c_smbus_read_byte_data(file, daddress);
	}

	if (res < 0) {
		fprintf(stderr, "Warning - readback failed\n");
		e1++;
	} else
	if (res != value) {
		e1++;
		if (size == I2C_SMBUS_WORD_DATA)
			fprintf(stderr, "Warning - data mismatch - wrote "
			        "0x%04x, read back 0x%04x\n", value, res);
		else
			fprintf(stderr, "Warning - data mismatch - wrote "
			        "0x%02x, read back 0x%02x\n", value, res);
	} else {
		fprintf(stderr, "Value 0x%x written, readback matched\n",
		        value);
	}

	exit(e1);
}
