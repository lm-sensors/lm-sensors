/*
    isadump.c - Part of isadump, a user-space program to dump ISA registers
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/io.h>


/* To keep glibc2 happy */
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 0
#include <sys/perm.h>
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
}

int main(int argc, char *argv[])
{
  int addrreg, datareg, bank = 0, bankreg = 0x4E;
  char *end;
  int i,j,res;

  if (argc < 2) {
    fprintf(stderr,"Error: No registers specified!\n");
    help();
    exit(1);
  }

  addrreg = strtol(argv[1],&end,0);
  if (*end) {
    fprintf(stderr,"Error: First argument not a number!\n");
    help();
    exit(1);
  }
  if ((addrreg < 0) || (addrreg > 0xfff)) {
    fprintf(stderr,"Error: Address register out of range!\n");
    help();
    exit(1);
  }

  if (argc < 3) {
    fprintf(stderr,"Error: No data register specified!\n");
    help();
    exit(1);
  }

  datareg = strtol(argv[2],&end,0);
  if (*end) {
    fprintf(stderr,"Error: Second argument not a number!\n");
    help();
    exit(1);
  }
  if ((datareg < 0) || (datareg > 0xfff)) {
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
        fprintf(stderr,"Error: Invalid bank register number!\n");
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

  if (getuid()) {
    fprintf(stderr,"Error: Can only be run as root (or make it suid root)\n");
    exit(1);
  }

  fprintf(stderr,"  WARNING! Running this program can cause system crashes, "
          "data loss and worse!\n");
  fprintf(stderr,"  I will probe address register 0x%04x and "
                 "data register 0x%04x.\n",addrreg,datareg);
  if(bank) 	
    fprintf(stderr,"  Probing bank %d using bank register 0x%02x.\n",
            bank, bankreg);
  fprintf(stderr,"  You have five seconds to reconsider and press CTRL-C!\n\n");
  sleep(5);

#ifndef __powerpc__
  if ((datareg < 0x400) && (addrreg < 0x400)) {
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
      outb(i+j,addrreg);
      res = inb(datareg);
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
