/*
    isaset.c - isaset, a user-space program to write ISA registers
    Copyright (C) 2000  Frodo Looijaard <frodol@dds.nl>, and
                        Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (C) 2004  The lm_sensors group

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

/*
	Typical usage:
	isaset 0x295 0x296 0x10 0x12	Write 0x12 to address 0x10 using address/data registers
	isaset -f 0x5010 0x12		Write 0x12 to location 0x5010
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


/* To keep glibc2 happy */
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 0
#include <sys/io.h>
#else
#include <asm/io.h>
#endif

#ifdef __powerpc__
unsigned long isa_io_base = 0; /* XXX for now */
#endif /* __powerpc__ */

void help(void)
{
	fprintf(stderr,
	        "Syntax for I2C-like access:\n"
	        "  isaset [-y] ADDRREG DATAREG ADDRESS VALUE [MASK]\n"
	        "Syntax for flat address space:\n"
	        "  isaset [-y] -f ADDRESS VALUE [MASK]\n");
}

int main(int argc, char *argv[])
{
	int addrreg, datareg = 0, value, addr = 0, vmask = 0;
	unsigned char res;
	int flags = 0;
	int flat = 0, yes = 0;
	char *end;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
		case 'f': flat = 1; break;
		case 'y': yes = 1; break;
		default:
			fprintf(stderr, "Warning: Unsupported flag "
				"\"-%c\"!\n", argv[1+flags][1]);
			help();
			exit(1);
		}
		flags++;
	}

	/* verify that the argument count is correct */
	if ((!flat && (argc < 1+flags+4 || argc > 1+flags+5))
	 || (flat && (argc < 1+flags+2 || argc > 1+flags+3))) {
		help();
		exit(1);
	}

	addrreg = strtol(argv[1+flags], &end, 0);
	if (*end) {
		fprintf(stderr, "Error: Invalid address!\n");
		help();
		exit(1);
	}
	if (addrreg < 0 || addrreg > (flat?0xffff:0x3fff)) {
		fprintf(stderr,
		        "Error: Address out of range (0x0000-0x%04x)!\n",
			flat?0xffff:0x3fff);
		help();
		exit(1);
	}

	if (!flat) {
		datareg = strtol(argv[1+flags+1], &end, 0);
		if (*end) {
			fprintf(stderr, "Error: Invalid data register!\n");
			help();
			exit(1);
		}
		if (datareg < 0 || datareg > 0x3fff) {
			fprintf(stderr, "Error: Data register out of range "
			        "(0x0000-0x3fff)!\n");
			help();
			exit(1);
		}

		addr = strtol(argv[1+flags+2], &end, 0);
		if (*end) {
			fprintf(stderr, "Error: Invalid address!\n");
			help();
			exit(1);
		}
		if (addr < 0 || addr > 0xff) {
			fprintf(stderr, "Error: Address out of range "
			        "(0x00-0xff)!\n");
			help();
			exit(1);
		}
	}

	/* rest is the same for both modes so we cheat on flags */
	if (!flat)
		flags += 2;

	value = strtol(argv[flags+2], &end, 0);
	if (*end) {
		fprintf(stderr, "Error: Invalid value!\n");
		help();
		exit(1);
	}
	if (value < 0 || value > 0xff) {
		fprintf(stderr, "Error: Value out of range "
			"(0x00-0xff)!\n");
		help();
		exit(1);
	}

	if (flags+3 < argc) {
		vmask = strtol(argv[flags+3], &end, 0);
		if (*end) {
			fprintf(stderr, "Error: Invalid mask!\n");
			help();
			exit(1);
		}
		if (vmask < 0 || vmask > 0xff) {
			fprintf(stderr, "Error: Mask out of range "
				"(0x00-0xff)!\n");
			help();
			exit(1);
		}
	}

	if (getuid()) {
		fprintf(stderr, "Error: Can only be run as root "
		        "(or make it suid root)\n");
		exit(1);
	}

	if (!yes) {
		char s[2];

		fprintf(stderr, "WARNING! Running this program can cause "
		        "system crashes, data loss and worse!\n");

		if (flat)
			fprintf(stderr, "I will write value 0x%02x%s to address "
		                "0x%x.\n", value, vmask ? " (masked)" : "",
			        addrreg);
		else
			fprintf(stderr, "I will write value 0x%02x%s to address "
		                "0x%02x of chip with address register 0x%x\n"
		                "and data register 0x%x.\n",
		                value, vmask ? " (masked)" : "", addr,
			        addrreg, datareg);

		fprintf(stderr, "Continue? [Y/n] ");
		fflush(stderr);
		fgets(s, 2, stdin);
		if (s[0] != '\n' && s[0] != 'y' && s[0] != 'Y') {
			fprintf(stderr, "Aborting on user request.\n");
			exit(0);
		}
	}

#ifndef __powerpc__
	if (!flat && datareg < 0x400 && addrreg < 0x400) {
		if (ioperm(datareg, 1, 1)) {
			fprintf(stderr, "Error: Could not ioperm() data "
			        "register!\n");
			exit(1);
		}
		if (ioperm(addrreg, 1, 1)) {
			fprintf(stderr, "Error: Could not ioperm() address "
		        	"register!\n");
			exit(1);
		}
	} else {
		if (iopl(3)) {
			fprintf(stderr, "Error: Could not do iopl(3)!\n");
			exit(1);
		}
	}
#endif

	if (vmask) {
		int oldvalue;

		if (flat) {
			oldvalue = inb(addrreg);
		} else {	
			outb(addr, addrreg);
			oldvalue = inb(datareg);
		}

		if (oldvalue < 0) {
			fprintf(stderr, "Error: Failed to read old value\n");
			exit(1);
		}

		value = (value & vmask) | (oldvalue & ~vmask);

		if (!yes) {
			char s[2];
			
			fprintf(stderr, "Old value 0x%02x, write mask "
				"0x%02x: Will write 0x%02x to %s "
				"0x%02x\n", oldvalue, vmask, value,
				flat ? "address" : "register",
				flat ? addrreg : addr);

			fprintf(stderr, "Continue? [Y/n] ");
			fflush(stderr);
			fgets(s, 2, stdin);
			if (s[0] != '\n' && s[0] != 'y' && s[0] != 'Y') {
				fprintf(stderr, "Aborting on user request.\n");
				exit(0);
			}
		}
	}

	/* do the real thing */
	if (flat) {
		/* write */
		outb(value, addrreg);
		/* readback */
		res = inb(addrreg);
	} else {	
		/* write */
		outb(addr, addrreg);
		outb(value, datareg);
		/* readback */
		res = inb(datareg);
	}

	if (res != value) {
		fprintf(stderr, "Data mismatch, wrote 0x%02x, "
		        "read 0x%02x back.\n", value, res);
	}

	exit(0);
}
