/*
    i2cdump.c - Part of i2cdump, a user-space program to dump I2C registers
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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>

/*
   We don't use this #define but it was put into i2c.h at the same time as
   i2c_smbus_read_i2c_block_data() was implemented (i2c 2.6.3),
   so we use it as a version check.
*/
#ifdef I2C_FUNC_SMBUS_READ_I2C_BLOCK_2 
#define USE_I2C_BLOCK 1
#else
#define USE_I2C_BLOCK 0
#endif


void help(void)
{
  FILE *fptr;
  char s[100];

  fprintf(stderr,"Syntax: i2cdump I2CBUS ADDRESS [MODE] [BANK [BANKREG]]\n");
  fprintf(stderr,"  MODE is 'b[yte]', 'w[ord]', 's[mbusblock], or 'i[2cblock]' (default b)\n");
  fprintf(stderr,"  I2CBUS is an integer\n");
  if((fptr = fopen("/proc/bus/i2c", "r"))) {
    fprintf(stderr,"  Installed I2C busses:\n");
    while(fgets(s, 100, fptr))
      fprintf(stderr, "    %s", s);	
    fclose(fptr);
  }
}

int main(int argc, char *argv[])
{
  char *end;
  int i,j,res,res2,i2cbus,address,size,file;
  int e1, e2, e3;
  int bank = 0, bankreg = 0x4E;
  char filename1[20];
  char filename2[20];
  char filename3[20];
  char *filename;
  long funcs;
  unsigned char cblock[256];  
  int block[256];  

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
  else if (!strcmp(argv[3],"s"))
    size = I2C_SMBUS_BLOCK_DATA;
  else if (!strcmp(argv[3],"i"))
    size = I2C_SMBUS_I2C_BLOCK_DATA;
  else {
    fprintf(stderr,"Error: Invalid mode!\n");
    help();
    exit(1);
  }

  if(argc > 4) {
    bank = strtol(argv[4],&end,0);
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

    if(argc > 5) {
      bankreg = strtol(argv[5],&end,0);
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
  
  /* check adapter functionality */
  if (ioctl(file,I2C_FUNCS,&funcs) < 0) {
    fprintf(stderr,
            "Error: Could not get the adapter functionality matrix: %s\n",
            strerror(errno));
    exit(1);
  }

  switch(size) {
     case I2C_SMBUS_BYTE_DATA:
	if (! (funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
	   fprintf(stderr, "Error: Adapter for i2c bus %d", i2cbus);
	   fprintf(stderr, " does not have byte read capability\n");
	   exit(1);
	}       
	break;

     case I2C_SMBUS_WORD_DATA:
	if (! (funcs & I2C_FUNC_SMBUS_READ_WORD_DATA)) {
	   fprintf(stderr, "Error: Adapter for i2c bus %d", i2cbus);
	   fprintf(stderr, " does not have word read capability\n");
	   exit(1);
	}       
	break;

     case I2C_SMBUS_BLOCK_DATA:
	if (! (funcs & I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
	   fprintf(stderr, "Error: Adapter for i2c bus %d", i2cbus);
	   fprintf(stderr, " does not have smbus block read capability\n");
	   exit(1);
	}       
	break;

     case I2C_SMBUS_I2C_BLOCK_DATA:
	if (! (funcs & I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
	   fprintf(stderr, "Error: Adapter for i2c bus %d", i2cbus);
	   fprintf(stderr, " does not have i2c block read capability\n");
	   exit(1);
	}       
	break;

  }
  /* use FORCE so that we can look at registers even when
     a driver is also running */
  if (ioctl(file,I2C_SLAVE_FORCE,address) < 0) {
    fprintf(stderr,"Error: Could not set address to %d: %s\n",address,
            strerror(errno));
    exit(1);
  }
 
  fprintf(stderr,"  WARNING! This program can confuse your I2C bus, "
          "cause data loss and worse!\n");
  fprintf(stderr,"  I will probe file %s, address 0x%x, mode %s\n",
          filename,address,size == I2C_SMBUS_BLOCK_DATA ? "smbus block" :
                           size == I2C_SMBUS_I2C_BLOCK_DATA ? "i2c block" :
                           size == I2C_SMBUS_BYTE_DATA ? "byte" : "word");
  if(bank) 	
    fprintf(stderr,"  Probing bank %d using bank register 0x%02x.\n",
            bank, bankreg);
  fprintf(stderr,"  You have five seconds to reconsider and press CTRL-C!\n\n");
  sleep(5);

  /* See Winbond w83781d data sheet for bank details */
  if(bank) {
    i2c_smbus_write_byte_data(file,bankreg,bank | 0x80);
  }

  /* handle all but word data */
  if (size != I2C_SMBUS_WORD_DATA) {

    /* do the block transaction */
    if(size == I2C_SMBUS_BLOCK_DATA || size == I2C_SMBUS_I2C_BLOCK_DATA) {
      if(size == I2C_SMBUS_BLOCK_DATA) {
        res = i2c_smbus_read_block_data(file,0,cblock);
      } else if(size == I2C_SMBUS_I2C_BLOCK_DATA) {
#if USE_I2C_BLOCK
        res = 0;
        for (i = 0; i < 256; i+=32) {
          res2 = i2c_smbus_read_i2c_block_data(file, i, cblock+i);
          if(res2 <= 0)
            break;
          res += res2;
        }
#else
        fprintf(stderr, "Error: I2C block read unsupported in i2c-core\n");
        exit(1);
#endif
      }
      if(res <= 0) {
        fprintf(stderr, "Error: Block read failed, return code %d\n", res);
        exit(1);
      }
      if(res >= 256)
        res = 256;
      for (i = 0; i < res; i++)
        block[i] = cblock[i];
      for (i = res; i < 256; i++)
        block[i] = -1;
    }

    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f    0123456789abcdef\n");
    for (i = 0; i < 256; i+=16) {
      printf("%02x: ",i);
      for(j = 0; j < 16; j++) {
        if(size == I2C_SMBUS_BYTE_DATA) {
          res = i2c_smbus_read_byte_data(file,i+j);
          block[i+j] = res;
        } else
          res = block[i+j];
        if (res < 0)
          printf("XX ");
        else
          printf("%02x ",res & 0xff);
      }
      printf("   ");
      for(j = 0; j < 16; j++) {
        res = block[i+j];
        if (res < 0)
          printf("X");
        else if (((res & 0xff) == 0x00) || ((res & 0xff) == 0xff))
          printf(".");
        else if (((res & 0xff) < 32) || ((res & 0xff) >= 127))
          printf("?");
        else
          printf("%c",res & 0xff);
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
  if(bank) {
    i2c_smbus_write_byte_data(file,bankreg,0x80);
  }
  exit(0);
}
