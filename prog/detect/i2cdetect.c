/*
    i2cdetect.c - Part of i2cdetect, a user-space program to scan for I2C 
                  devices.
    Copyright (c) 1999-2004  Frodo Looijaard <frodol@dds.nl> and
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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "i2c-dev.h"
#include "version.h"

#define MODE_AUTO	0
#define MODE_QUICK	1
#define MODE_READ	2

void print_i2c_busses(int);

void help(void)
{
	fprintf(stderr,"Syntax: i2cdetect [-f] [-q|-r] I2CBUS\n");
	fprintf(stderr,"  I2CBUS is an integer\n");
	fprintf(stderr,"  With -f, scans all addresses (NOT RECOMMENDED)\n");
	fprintf(stderr,"  With -q, uses only quick write commands for probing (NOT RECOMMENDED)\n");
	fprintf(stderr,"  With -r, uses only read byte commands for probing (NOT RECOMMENDED)\n");
	fprintf(stderr,"  i2cdetect -l lists installed busses only\n");
	print_i2c_busses(0);
}

int main(int argc, char *argv[])
{
  char *end;
  int i=1,j,res,i2cbus=-1,file;
  int e1, e2, e3;
  char filename1[20];
  char filename2[20];
  char filename3[20];
  char *filename;
  long funcs;
  int force = 0;
  int mode = MODE_AUTO;
  

  while(i2cbus == -1) {
    if (i >= argc) {
      fprintf(stderr,"Error: No i2c-bus specified!\n");
      help();
      exit(1);
    }

    if(!strcasecmp(argv[i], "-v")) {
      fprintf(stderr,"i2cdetect version %s\n", LM_VERSION);
      exit(0);
    }

    if(!strcmp(argv[i], "-l")) {
      print_i2c_busses(1);
      exit(0);
    }

    if(!strcmp(argv[i], "-f")) {
      force = 1;
      i++;
    } else
    if(!strcmp(argv[i], "-q")) {
      if (mode != MODE_AUTO) {
        fprintf(stderr,"Error: Different modes specified!\n");
        exit(1);
      }
      mode = MODE_QUICK;
      i++;
    } else
    if(!strcmp(argv[i], "-r")) {
      if (mode != MODE_AUTO) {
        fprintf(stderr,"Error: Different modes specified!\n");
        exit(1);
      }
      mode = MODE_READ;
      i++;
    } else {
      i2cbus = strtol(argv[i],&end,0);
      if (*end) {
        fprintf(stderr,"Error: I2CBUS argument not a number!\n");
        help();
        exit(1);
      }
      if ((i2cbus < 0) || (i2cbus > 0xff)) {
        fprintf(stderr,"Error: I2CBUS argument out of range!\n");
        help();
        exit(1);
      }
    }
  }
/*
 * Try all three variants and give the correct error message
 * upon failure
 */

  sprintf(filename1,"/dev/i2c-%d",i2cbus);
  sprintf(filename2,"/dev/i2c%d",i2cbus);
  sprintf(filename3,"/dev/i2c/%d",i2cbus);
  if ((file = open(filename1,O_RDWR)) < 0) {
    e1 = errno;
    if ((file = open(filename2,O_RDWR)) < 0) {
      e2 = errno;
      if ((file = open(filename3,O_RDWR)) < 0) {
        e3 = errno;
        if(e1 == ENOENT && e2 == ENOENT && e3 == ENOENT) {
          fprintf(stderr,"Error: Could not open file `%s', `%s', or `%s': %s\n",
                     filename1,filename2,filename3,strerror(ENOENT));
        }
        if (e1 != ENOENT) {
          fprintf(stderr,"Error: Could not open file `%s' : %s\n",
                     filename1,strerror(e1));
          if(e1 == EACCES)
            fprintf(stderr,"Run as root?\n");
        }
        if (e2 != ENOENT) {
          fprintf(stderr,"Error: Could not open file `%s' : %s\n",
                     filename2,strerror(e2));
          if(e2 == EACCES)
            fprintf(stderr,"Run as root?\n");
        }
        if (e3 != ENOENT) {
          fprintf(stderr,"Error: Could not open file `%s' : %s\n",
                     filename3,strerror(e3));
          if(e3 == EACCES)
            fprintf(stderr,"Run as root?\n");
        }
        exit(1);
      } else {
         filename = filename3;
      }
    } else {
       filename = filename2;
    }
  } else {
    filename = filename1;
  }

  if (ioctl(file,I2C_FUNCS,&funcs) < 0) {
    fprintf(stderr,
            "Error: Could not get the adapter functionality matrix: %s\n",
            strerror(errno));
    close(file);
    exit(1);
  }
  if (mode != MODE_READ && !(funcs & I2C_FUNC_SMBUS_QUICK)) {
    fprintf(stderr,
            "Error: Can't use SMBus Quick Write command "
            "on this bus (ISA bus?)\n");
    close(file);
    exit(1);
  }
  if (mode != MODE_QUICK && !(funcs & I2C_FUNC_SMBUS_READ_BYTE)) {
    fprintf(stderr,
            "Error: Can't use SMBus Read Byte command "
            "on this bus (ISA bus?)\n");
    close(file);
    exit(1);
  }
  
  fprintf(stderr,"  WARNING! This program can confuse your I2C bus, "
          "cause data loss and worse!\n");
  fprintf(stderr,"  I will probe file %s%s\n", filename,
          mode==MODE_QUICK?" using quick write commands":
          mode==MODE_READ?" using read byte commands":"");
  fprintf(stderr,"  You have five seconds to reconsider and press CTRL-C!\n\n");
  sleep(5);

  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  for (i = 0; i < 128; i+=16) {
    printf("%02x: ",i);
    for(j = 0; j < 16; j++) {
      if (!force && (i+j<0x03 || i+j>0x77)) {
        printf("   ");
        continue;
      }
      if (ioctl(file,I2C_SLAVE,i+j) < 0) {
        if (errno == EBUSY) {
          printf("UU ");
          continue;
        } else {
          fprintf(stderr,"Error: Could not set address to %02x: %s\n",i+j,
                  strerror(errno));
          close(file);
          exit(1);
        }
      }

      switch(mode) {
      case MODE_QUICK:
        /* This is known to corrupt the Atmel AT24RF08 EEPROM */
        res = i2c_smbus_write_quick(file, I2C_SMBUS_WRITE);
        break;
      case MODE_READ:
        /* This is known to lock SMBus on various write-only chips
           (mainly clock chips) */
        res = i2c_smbus_read_byte(file);
        break;
      default:
        if((i+j>=0x30 && i+j<=0x37)
        || (i+j>=0x50 && i+j<=0x5F)) {
          res = i2c_smbus_write_quick(file, I2C_SMBUS_WRITE);
        } else {
          res = i2c_smbus_read_byte(file);
        }
      }

      if (res < 0)
        printf("XX ");
      else
        printf("%02x ",i+j);
    }
    printf("\n");
  }
  close(file);
  exit(0);
}
