/*
    i2cdetect.c - Part of i2cdetect, a user-space program to scan for I2C 
                  devices.
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
  fprintf(stderr,"Syntax: i2cdetect I2CBUS\n");
}

int main(int argc, char *argv[])
{
  char *end;
  int i,j,res,i2cbus,file;
  char filename[20];
  long funcs;
  

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

  sprintf(filename,"/dev/i2c-%d",i2cbus);
  if ((file = open(filename,O_RDWR)) < 0) {
    fprintf(stderr,"Error: Could not open file `%s': %s\n",filename,
            strerror(errno));
    exit(1);
  }

  if (ioctl(file,I2C_FUNCS,&funcs) < 0) {
    fprintf(stderr,
            "Error: Could not get the adapter functionality maxtrix: %s\n",
            strerror(errno));
    exit(1);
  }
  if (! (funcs & I2C_FUNC_SMBUS_QUICK)) {
    fprintf(stderr,
            "Error: Can't use SMBus Quick Write command "
            "on this bus (ISA bus?)\n");
    exit(1);
  }
  
  fprintf(stderr,"  WARNING! This program can confuse your I2C bus, "
          "cause data loss and worse!\n");
  fprintf(stderr,"  I will probe file %s\n", filename);
  fprintf(stderr,"  You have five seconds to reconsider and press CTRL-C!\n\n");
  sleep(5);

  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  for (i = 0; i < 128; i+=16) {
    printf("%02x: ",i);
    for(j = 0; j < 16; j++) {
      if (ioctl(file,I2C_SLAVE,i+j) < 0) {
        fprintf(stderr,"Error: Could not set address to %d: %s\n",i+j,
                strerror(errno));
        exit(1);
      }

      res = i2c_smbus_write_quick(file, I2C_SMBUS_WRITE);
      if (res < 0)
        printf("XX ");
      else
        printf("%02x ",i+j);
    }
    printf("\n");
  }
  exit(0);
}
