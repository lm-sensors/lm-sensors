/*
    isadump.c - isadump, a user-space program to dump ISA registers
    Copyright (c) 2000  Frodo Looijaard <frodol@dds.nl>, and
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
	isadump 0x295 0x296		Basic winbond dump using address/data registers
	isadump 0x295 0x296 2		Winbond dump, bank 2
	isadump -f 0x5000		Flat address space dump like for Via 686a
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
  fprintf(stderr,"Syntax: isadump ADDRREG DATAREG [BANK [BANKREG]]\n");
  fprintf(stderr,"        isadump -f ADDRESS (for flat address space)\n");
}

int main(int argc, char *argv[])
{
  int addrreg, datareg = 0, bank = 0, bankreg = 0x4E;
  int i,j,res;
  int flat = 0;
  char *end;

  if (argc < 3) {
    help();
    exit(1);
  }

  if(strcmp(argv[1], "-f")) {
    addrreg = strtol(argv[1],&end,0);
  } else {
    if(argc != 3) {
      help();
      exit(1);
    }
    flat = 1;
    addrreg = strtol(argv[2],&end,0) & 0xff00;
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

  if(flat)
    goto START;

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

  if(argc > 3) {
    bank = strtol(argv[3],&end,0);
    if (*end) {
      fprintf(stderr,"Error: Invalid bank number!\n");
      help();
      exit(1);
    }
    if ((bank < 0) || (bank > 15)) {
      fprintf(stderr,"Error: bank out of range (0-15)!\n");
      help();
      exit(1);
    }

    if(argc > 4) {
      bankreg = strtol(argv[4],&end,0);
      if (*end) {
        fprintf(stderr,"Error: Invalid bank register!\n");
        help();
        exit(1);
      }
      if ((bankreg < 0) || (bankreg > 0xff)) {
        fprintf(stderr,"Error: bank out of range (0-0xff)!\n");
        help();
        exit(1);
      }
    }
  }

START:

  if (getuid()) {
    fprintf(stderr,"Error: Can only be run as root (or make it suid root)\n");
    exit(1);
  }

  fprintf(stderr,"  WARNING! Running this program can cause system crashes, "
          "data loss and worse!\n");
  if(flat)
	fprintf(stderr,"  I will probe address range 0x%04x to "
                 "0x%04x.\n",addrreg, addrreg + 0xff);
  else
	fprintf(stderr,"  I will probe address register 0x%04x and "
                 "data register 0x%04x.\n",addrreg,datareg);
  if(bank) 	
    fprintf(stderr,"  Probing bank %d using bank register 0x%02x.\n",
            bank, bankreg);
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

  /* See Winbond w83781d data sheet for bank details */
  if(bank) {
    outb(bankreg,addrreg);
    outb(bank | 0x80,datareg); /* OR in high byte flag */
  }

  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  for (i = 0; i < 256; i+=16) {
    printf("%c0: ",hexchar(i/16));
    for(j = 0; j < 16; j++) {
	if(flat) {
	      res = inb(addrreg + i + j);
	} else {	
	      outb(i+j,addrreg);
	      res = inb(datareg);
	}
	printf("%c%c ",hexchar(res/16),hexchar(res%16));
    }
    printf("\n");
  }
  if(bank) {
    outb(bankreg,addrreg);
    outb(0x80,datareg); /* put back in bank 0 high byte */
  }
  exit(0);
}
