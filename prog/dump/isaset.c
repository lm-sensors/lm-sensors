/*
    isaset.c - isaset, a user-space program to write ISA registers
    Copyright (c) 2000 - 2004  Frodo Looijaard <frodol@dds.nl>, and
                        Mark D. Studebaker <mdsxyz123@yahoo.com>

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
	isaset -f 0x5000 0x12		Write 0x12 to location 0x5010
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


/* To keep glibc2 happy */
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 0
#include <sys/io.h>
#else
#include <asm/io.h>
#endif

#ifdef __powerpc__
unsigned long isa_io_base = 0; /* XXX for now */
#endif /* __powerpc__ */

char hexchar(int i)
{
  if ((i >= 0) && (i <= 9))
    return '0' + i;
  else if (i <= 15)
    return 'a' - 10 + i;
  else
    return 'X';
}

void help(void)
{
  fprintf(stderr,"Syntax: isaset ADDRREG DATAREG ADDR DATA (for chips with addr/data registers)\n");
  fprintf(stderr,"        isaset -f ADDR DATA (for chips with a flat address space)\n");
}

int main(int argc, char *argv[])
{
  int addrreg, datareg = 0;
  unsigned char value, res, addr = 0;
  int flat = 0;
  char *end;

  if (argc < 4) {
    help();
    exit(1);
  }

  if(strcmp(argv[1], "-f")) {
    addrreg = strtol(argv[1],&end,0);
  } else {
    if(argc != 4) {
      help();
      exit(1);
    }
    flat = 1;
    addrreg = strtol(argv[2],&end,0);
  }
  if (*end) {
    fprintf(stderr,"Error: Invalid address!\n");
    help();
    exit(1);
  }
  if ((addrreg < 0) || (addrreg > 0xffff)) {
    fprintf(stderr,"Error: Address out of range!\n");
    help();
    exit(1);
  }

  if(flat) {
	    value = strtol(argv[3],&end,0);
	    if (*end) {
	      fprintf(stderr,"Error: Invalid data!\n");
	      help();
	      exit(1);
	    }
  } else {

	  datareg = strtol(argv[2],&end,0);
	  if (*end) {
	    fprintf(stderr,"Error: Invalid data register!\n");
	    help();
	    exit(1);
  }
  if ((datareg < 0) || (datareg > 0xffff)) {
    fprintf(stderr,"Error: Data register out of range!\n");
    help();
    exit(1);
  }

  if(flat) {
    value = strtol(argv[3],&end,0);
    if (*end) {
      fprintf(stderr,"Error: Invalid addr!\n");
      help();
      exit(1);
    }
  } else {
    addr = strtol(argv[3],&end,0);
    if (*end) {
      fprintf(stderr,"Error: Invalid addr!\n");
      help();
      exit(1);
    }
    value = strtol(argv[4],&end,0);
    if (*end) {
      fprintf(stderr,"Error: Invalid data!\n");
      help();
      exit(1);
    }
  }
}

  if (getuid()) {
    fprintf(stderr,"Error: Can only be run as root (or make it suid root)\n");
    exit(1);
  }

  fprintf(stderr,"  WARNING! Running this program can cause system crashes, "
          "data loss and worse!\n");
  if(flat)
	fprintf(stderr,"  I will write address 0x%04x with data 0x%02x\n",
                 addrreg, value);
  else
	fprintf(stderr,"  I will write chip address 0x%04x with data 0x%02x\n"
	               "  using address register 0x%04x and "
                       "data register 0x%04x.\n",
                       addr, value, addrreg, datareg);
  fprintf(stderr,"  You have five seconds to reconsider and press CTRL-C!\n\n");
  sleep(5);

#ifndef __powerpc__
  if ((datareg < 0x400) && (addrreg < 0x400) && !flat) {
    if(ioperm(datareg,1,1)) {
      fprintf(stderr,"Error: Could not ioperm() data register!\n");
      exit(1);
    }
    if(ioperm(addrreg,1,1)) {
      fprintf(stderr,"Error: Could not ioperm() address register!\n");
      exit(1);
    }
  } else {
    if(iopl(3)) {
      fprintf(stderr,"Error: Could not do iopl(3)!\n");
      exit(1);
    }
  }
#endif

/* write */
	if(flat) {
	      outb(value, addrreg);
	} else {	
	      outb(addr, addrreg);
	      outb(value,datareg);
	}

/* readback */
	if(flat) {
	      res = inb(addrreg);
	} else {	
	      outb(addr, addrreg);
	      res = inb(datareg);
	}

	if(res != value) {
		fprintf(stderr, "Warning - data mismatch - wrote 0x%.2x, read back 0x%.2x\n", value, res);
	} else {
		fprintf(stderr, "Value 0x%x written, readback matched\n", value);
	}
  exit(0);
}
