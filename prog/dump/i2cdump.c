/*
    i2cdump.c - Part of i2cdump, a user-space program to dump I2C registers
    Copyright (c) 1999  Frodo Looijaard <frodol@dds.nl>

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
#include <linux/i2c-dev.h>

void help(void)
{
  fprintf(stderr,"Syntax: i2cdump I2CBUS ADDRESS MODE\n");
  fprintf(stderr,"  MODE may be 'b' or 'w'\n");
}

int main(int argc, char *argv[])
{
  char *end;
  int i,j,res,i2cbus,address,size,file;
  char filename[20];
  

  if (argc < 2) {
    fprintf(stderr,"Error: No i2c-bus specified!\n");
    help();
    exit(1);
  }

  i2cbus = strtol(argv[1],&end,0);
  if (*end) {
    fprintf(stderr,"Error: First argument not a number!\n");
    help();
    exit(1);
  }
  if ((i2cbus < 0) || (i2cbus > 0xff)) {
    fprintf(stderr,"Error: I2CBUS argument out of range!\n");
    help();
  }

  if (argc < 3) {
    fprintf(stderr,"Error: No address specified!\n");
    help();
    exit(1);
  }
  address = strtol(argv[2],&end,0);
  if (*end) {
    fprintf(stderr,"Error: Second argument not a number!\n");
    help();
    exit(1);
  }
  if ((address < 0) || (address > 0x7f)) {
    fprintf(stderr,"Error: Address out of range!\n");
    help();
  }

  if (argc < 4) {
    fprintf(stderr,"Warning: no size specified (using byte-data access)\n");
    size = I2C_SMBUS_BYTE_DATA;
  } else if (!strcmp(argv[3],"b"))
    size = I2C_SMBUS_BYTE_DATA;
  else if (!strcmp(argv[3],"w"))
    size = I2C_SMBUS_WORD_DATA;
  else {
    fprintf(stderr,"Error: Third argument not recognized!\n");
    help();
    exit(1);
  }

  sprintf(filename,"/dev/i2c-%d",i2cbus);
  if ((file = open(filename,O_RDWR)) < 0) {
    fprintf(stderr,"Error: Could not open file `%s': %s\n",filename,
            strerror(errno));
    exit(1);
  }
  
  if (ioctl(file,I2C_SLAVE,address) < 0) {
    fprintf(stderr,"Error: Could not set address to %d: %s\n",address,
            strerror(errno));
    exit(1);
  }
 
  fprintf(stderr,"  WARNING! This program can confuse your I2C bus, "
          "cause data loss and worse!\n");
  fprintf(stderr,"  I will probe file %s, address 0x%x, mode %s\n",
          filename,address,size == I2C_SMBUS_BYTE_DATA?"byte":"word");
  fprintf(stderr,"  You have five seconds to reconsider and press CTRL-C!\n\n");
  sleep(5);

  if (size == I2C_SMBUS_BYTE_DATA) {
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    for (i = 0; i < 256; i+=16) {
      printf("%02x: ",i);
      for(j = 0; j < 16; j++) {
        res = i2c_smbus_read_byte_data(file,i+j);
        if (res < 0)
          printf("XX ");
        else
          printf("%02x ",res & 0xff);
      }
      printf("\n");
    }
  } else {
    printf("     0,8  1,9  2,a  3,b  4,c  5,d  6,e  7,f\n");
    for (i = 0; i < 256; i+=8) {
      printf("%02x: ",i);
      for(j = 0; j < 8; j++) {
        res = i2c_smbus_read_word_data(file,i+j);
        if (res < 0)
          printf("XXXX ");
        else
          printf("%04x ",res & 0xffff);
      }
      printf("\n");
    }
  }
  exit(0);
}
