/*
    i2cdump.c - a user-space program to dump I2C registers
    Copyright (C) 2002-2003  Frodo Looijaard <frodol@dds.nl>, and
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
#include "util.h"
#include "i2cbusses.h"
#include "i2c-dev.h"
#include "version.h"

void help(void)
{
	fprintf(stderr, "Syntax: i2cdump [-f] [-y] I2CBUS ADDRESS [MODE] "
	        "[BANK [BANKREG]]\n"
	        "        i2cdump -V\n"
	        "  MODE is one of:\n"
		"    b (byte, default)\n"
		"    w (word)\n"
		"    W (word on even register addresses)\n"
		"    s (SMBus block)\n"
		"    i (I2C block)\n"
	        "    c (consecutive byte)\n"
	        "    Append 'p' to 'b', 'w', 's' or 'c' for PEC checking\n"
	        "  I2CBUS is an integer\n"
	        "  ADDRESS is an integer 0x00 - 0x7f\n"
	        "  BANK and BANKREG are for byte and word accesses (default "
	        "bank 0, reg 0x4e)\n"
	        "  BANK is the command for smbusblock accesses (default 0)\n");
	print_i2c_busses(0);
}

int main(int argc, char *argv[])
{
	char *end;
	int i, j, res, i2cbus, address, size, file;
	int bank = 0, bankreg = 0x4E, old_bank = 0;
	char filename[20];
	unsigned long funcs;
	int block[256], s_length = 0;
	int pec = 0, even = 0;
	int flags = 0;
	int force = 0, yes = 0, version = 0;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
		case 'V': version = 1; break;
		case 'f': force = 1; break;
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
		fprintf(stderr, "i2cdump version %s\n", LM_VERSION);
		exit(0);
	}

	if (argc < flags + 2) {
		fprintf(stderr, "Error: No i2c-bus specified!\n");
		help();
		exit(1);
	}
	i2cbus = strtol(argv[flags+1], &end, 0);
	if (*end) {
		fprintf(stderr, "Error: First argument not a number!\n");
		help();
		exit(1);
	}
	if (i2cbus < 0 || i2cbus > 0xff) {
		fprintf(stderr, "Error: I2CBUS argument out of range!\n");
		help();
		exit(1);
	}

	if (argc < flags + 3) {
		fprintf(stderr, "Error: No address specified!\n");
		help();
		exit(1);
	}
	address = strtol(argv[flags+2], &end, 0);
	if (*end) {
		fprintf(stderr, "Error: Second argument not a number!\n");
		help();
		exit(1);
	}
	if (address < 0 || address > 0x7f) {
		fprintf(stderr, "Error: Address out of range!\n");
		help();
		exit(1);
	}

	if (argc < flags + 4) {
		fprintf(stderr, "No size specified (using byte-data access)\n");
		size = I2C_SMBUS_BYTE_DATA;
	} else if (!strncmp(argv[flags+3], "b", 1)) {
		size = I2C_SMBUS_BYTE_DATA;
		pec = argv[flags+3][1] == 'p';
	} else if (!strncmp(argv[flags+3], "w", 1)) {
		size = I2C_SMBUS_WORD_DATA;
		pec = argv[flags+3][1] == 'p';
	} else if (!strncmp(argv[flags+3], "W", 1)) {
		size = I2C_SMBUS_WORD_DATA;
		even = 1;
	} else if (!strncmp(argv[flags+3], "s", 1)) {
		size = I2C_SMBUS_BLOCK_DATA;
		pec = argv[flags+3][1] == 'p';
	} else if (!strncmp(argv[flags+3], "c", 1)) {
		size = I2C_SMBUS_BYTE;
		pec = argv[flags+3][1] == 'p';
	} else if (!strcmp(argv[flags+3], "i"))
		size = I2C_SMBUS_I2C_BLOCK_DATA;
	else {
		fprintf(stderr, "Error: Invalid mode!\n");
		help();
		exit(1);
	}

	if (argc > flags + 4) {
		bank = strtol(argv[flags+4], &end, 0);
		if (*end || size == I2C_SMBUS_I2C_BLOCK_DATA) {
			fprintf(stderr, "Error: Invalid bank number!\n");
			help();
			exit(1);
		}
		if ((size == I2C_SMBUS_BYTE_DATA || size == I2C_SMBUS_WORD_DATA)
		 && (bank < 0 || bank > 15)) {
			fprintf(stderr, "Error: bank out of range!\n");
			help();
			exit(1);
		}
		if (size == I2C_SMBUS_BLOCK_DATA
		 && (bank < 0 || bank > 0xff)) {
			fprintf(stderr, "Error: block command out of range!\n");
			help();
			exit(1);
		}

		if (argc > flags + 5) {
			bankreg = strtol(argv[flags+5], &end, 0);
			if (*end || size == I2C_SMBUS_BLOCK_DATA) {
				fprintf(stderr, "Error: Invalid bank register "
				        "number!\n");
				help();
				exit(1);
			}
			if (bankreg < 0 || bankreg > 0xff) {
				fprintf(stderr, "Error: bank out of range "
				        "(0-0xff)!\n");
				help();
				exit(1);
			}
		}
	}

	file = open_i2c_dev(i2cbus, filename, 0);
	if (file < 0) {
		exit(1);
	}

	/* check adapter functionality */
	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
		fprintf(stderr, "Error: Could not get the adapter "
		        "functionality matrix: %s\n", strerror(errno));
		exit(1);
	}

	switch(size) {
	case I2C_SMBUS_BYTE:
		if (!((funcs & I2C_FUNC_SMBUS_BYTE) == I2C_FUNC_SMBUS_BYTE)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
				"not have byte capability\n", i2cbus);
			exit(1);
		}
		break;

	case I2C_SMBUS_BYTE_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
				"not have byte read capability\n", i2cbus);
			exit(1);
		}
		break;

	case I2C_SMBUS_WORD_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_WORD_DATA)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
				"not have word read capability\n", i2cbus);
			exit(1);
		}
		break;

	case I2C_SMBUS_BLOCK_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
				"not have smbus block read capability\n",
				i2cbus);
			exit(1);
		}
		break;

	case I2C_SMBUS_I2C_BLOCK_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
			fprintf(stderr, "Error: Adapter for i2c bus %d does "
			        "not have i2c block read capability\n",
			        i2cbus);
			exit(1);
		}
		break;
	}

	if (set_slave_addr(file, address, force) < 0)
		exit(1);

	if (pec) {
		if (ioctl(file, I2C_PEC, 1) < 0) {
			fprintf(stderr, "Error: Could not set PEC: %s\n",
			        strerror(errno));
			exit(1);
		}
		if (!(funcs & (I2C_FUNC_SMBUS_HWPEC_CALC | I2C_FUNC_I2C))) {
			fprintf(stderr, "Warning: Adapter for i2c bus %d does "
			        "not seem to actually support PEC\n", i2cbus);
		}
	}

	if (!yes) {
		fprintf(stderr, "WARNING! This program can confuse your I2C "
		        "bus, cause data loss and worse!\n");

		fprintf(stderr, "I will probe file %s, address 0x%x, mode "
		        "%s\n", filename, address,
		        size == I2C_SMBUS_BLOCK_DATA ? "smbus block" :
		        size == I2C_SMBUS_I2C_BLOCK_DATA ? "i2c block" :
		        size == I2C_SMBUS_BYTE ? "byte consecutive read" :
		        size == I2C_SMBUS_BYTE_DATA ? "byte" : "word");
		if (pec)
			fprintf(stderr, "PEC checking enabled.\n");
		if (even)
			fprintf(stderr, "Only probing even register "
			        "addresses.\n");
		if (bank) {
			if (size == I2C_SMBUS_BLOCK_DATA)
				fprintf(stderr, "Using command 0x%02x.\n",
				        bank);
			else
				fprintf(stderr, "Probing bank %d using bank "
				        "register 0x%02x.\n", bank, bankreg);
		}

		fprintf(stderr, "Continue? [Y/n] ");
		fflush(stderr);
		if (!user_ack(1)) {
			fprintf(stderr, "Aborting on user request.\n");
			exit(0);
		}
	}

	/* See Winbond w83781d data sheet for bank details */
	if (bank && size != I2C_SMBUS_BLOCK_DATA) {
		res = i2c_smbus_read_byte_data(file, bankreg);
		if (res >= 0) {
			old_bank = res;
			res = i2c_smbus_write_byte_data(file, bankreg,
				bank | (old_bank & 0xf0));
		}
		if (res < 0) {
			fprintf(stderr, "Error: Bank switching failed\n");
			exit(1);
		}
	}

	/* handle all but word data */
	if (size != I2C_SMBUS_WORD_DATA || even) {
		/* do the block transaction */
		if (size == I2C_SMBUS_BLOCK_DATA
		 || size == I2C_SMBUS_I2C_BLOCK_DATA) {
			unsigned char cblock[288];

			if (size == I2C_SMBUS_BLOCK_DATA) {
				res = i2c_smbus_read_block_data(file, bank,
				      cblock);
				/* Remember returned block length for a nicer
				   display later */
				s_length = res;
			} else {
				for (res = 0; res < 256; res += i) {
					i = i2c_smbus_read_i2c_block_data(file,
						res, 32, cblock + res);
					if (i <= 0)
						break;
				}
			}
			if (res <= 0) {
				fprintf(stderr, "Error: Block read failed, "
				        "return code %d\n", res);
				exit(1);
			}
			if (res >= 256)
				res = 256;
			for (i = 0; i < res; i++)
				block[i] = cblock[i];
			if (size != I2C_SMBUS_BLOCK_DATA)
				for (i = res; i < 256; i++)
					block[i] = -1;
		}

		if (size == I2C_SMBUS_BYTE) {
			res = i2c_smbus_write_byte(file, 0);
			if(res != 0) {
				fprintf(stderr, "Error: Write start address "
				        "failed, return code %d\n", res);
				exit(1);
			}
		}

		printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f"
		       "    0123456789abcdef\n");
		for (i = 0; i < 256; i+=16) {
			if (size == I2C_SMBUS_BLOCK_DATA && i >= s_length)
				break;
			printf("%02x: ", i);
			for (j = 0; j < 16; j++) {
				fflush(stdout);
				if (size == I2C_SMBUS_BYTE_DATA) {
					block[i+j] = res =
					  i2c_smbus_read_byte_data(file, i+j);
				} else if (size == I2C_SMBUS_WORD_DATA) {
					res = i2c_smbus_read_word_data(file,
					                               i+j);
					if (res < 0) {
						block[i+j] = res;
						block[i+j+1] = res;
					} else {
						block[i+j] = res & 0xff;
						block[i+j+1] = res >> 8;
					}
				} else if (size == I2C_SMBUS_BYTE) {
					block[i+j] = res =
					  i2c_smbus_read_byte(file);
				} else
					res = block[i+j];

				if (size == I2C_SMBUS_BLOCK_DATA
				 && i+j >= s_length) {
					printf("   ");
				} else if (res < 0) {
					printf("XX ");
					if (size == I2C_SMBUS_WORD_DATA)
						printf("XX ");
				} else {
					printf("%02x ", block[i+j]);
					if (size == I2C_SMBUS_WORD_DATA)
						printf("%02x ", block[i+j+1]);
				}
				if (size == I2C_SMBUS_WORD_DATA)
					j++;
			}
			printf("   ");

			for (j = 0; j < 16; j++) {
				if (size == I2C_SMBUS_BLOCK_DATA
				 && i+j >= s_length)
					break;

				res = block[i+j];
				if (res < 0)
					printf("X");
				else
				if ((res & 0xff) == 0x00
				 || (res & 0xff) == 0xff)
					printf(".");
				else
				if ((res & 0xff) < 32
				 || (res & 0xff) >= 127)
					printf("?");
				else
					printf("%c", res & 0xff);
			}
			printf("\n");
		}
	} else {
		printf("     0,8  1,9  2,a  3,b  4,c  5,d  6,e  7,f\n");
		for (i = 0; i < 256; i+=8) {
			printf("%02x: ", i);
			for (j = 0; j < 8; j++) {
				res = i2c_smbus_read_word_data(file, i+j);
				if (res < 0)
					printf("XXXX ");
				else
					printf("%04x ", res & 0xffff);
			}
			printf("\n");
		}
	}
	if (bank && size != I2C_SMBUS_BLOCK_DATA) {
		i2c_smbus_write_byte_data(file, bankreg, old_bank);
	}
	exit(0);
}
