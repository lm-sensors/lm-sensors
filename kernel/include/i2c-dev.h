/*
    i2c-dev.h - Part of lm_sensors, Linux kernel modules for hardware
            monitoring
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl> 

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

#ifndef SENSORS_I2C_DEV_H
#define SENSORS_I2C_DEV_H

#ifdef LM_SENSORS
#include "i2c.h"
#include "smbus.h"
#else /* ndef LM_SENSORS */
#include <linux/i2c.h>
#include <linux/smbus.h>
#endif /* def LM_SENSORS */

/* Some IOCTL commands are defined in <linux/i2c.h> */
/* Note: 10-bit addresses are NOT supported! */

#define I2C_SMBUS 0x0720

/* This is the structure as used in the I2C_SMBUS ioctl call */
struct i2c_smbus_data {
  char read_write;
  u8 command;
  int size;
  union smbus_data *data;
};

#ifndef __KERNEL__

#include <linux/ioctl.h>

extern inline s32 i2c_smbus_access(int file, char read_write, u8 command, 
                                   int size, union smbus_data *data)
{
  struct i2c_smbus_data args;
  int res;

  args.read_write = read_write;
  args.command = command;
  args.size = size;
  args.data = data;
  return ioctl(file,I2C_SMBUS,&args);
}


extern inline s32 i2c_smbus_write_quick(int file, u8 value)
{
  return i2c_smbus_access(file,value,0,SMBUS_QUICK,NULL);
}

extern inline s32 i2c_smbus_read_byte(int file)
{
  union smbus_data data;
  if (i2c_smbus_access(file,SMBUS_READ,0,SMBUS_BYTE,&data))
    return -1;
  else
    return 0x0FF & data.byte;
}

extern inline s32 i2c_smbus_write_byte(int file, u8 value)
{
  return i2c_smbus_access(file,SMBUS_WRITE,value, SMBUS_BYTE,NULL);
}

extern inline s32 i2c_smbus_read_byte_data(int file, u8 command)
{
  union smbus_data data;
  if (i2c_smbus_access(file,SMBUS_READ,command,SMBUS_BYTE_DATA,&data))
    return -1;
  else
    return 0x0FF & data.byte;
}

extern inline s32 i2c_smbus_write_byte_data(int file, u8 command, u8 value)
{
  union smbus_data data;
  data.byte = value;
  return i2c_smbus_access(file,SMBUS_WRITE,command,SMBUS_BYTE_DATA,&data);
}

extern inline s32 i2c_smbus_read_word_data(int file, u8 command)
{
  union smbus_data data;
  if (i2c_smbus_access(file,SMBUS_READ,command,SMBUS_WORD_DATA,&data))
    return -1;
  else
    return 0x0FFFF & data.word;
}

extern inline s32 i2c_smbus_write_word_data(int file, u8 command, u16 value)
{
  union smbus_data data;
  data.word = value;
  return i2c_smbus_access(file,SMBUS_WRITE,command,SMBUS_WORD_DATA, &data);
}

extern inline s32 i2c_smbus_process_call(int file, u8 command, u16 value)
{
  union smbus_data data;
  data.word = value;
  if (i2c_smbus_access(file,SMBUS_WRITE,command,SMBUS_PROC_CALL,&data))
    return -1;
  else
    return 0x0FFFF & data.word;
}


/* Returns the number of read bytes */
extern inline s32 i2c_smbus_read_block_data(int file, u8 command, u8 *values)
{
  union smbus_data data;
  int i;
  if (i2c_smbus_access(file,SMBUS_READ,command,SMBUS_BLOCK_DATA,&data))
    return -1;
  else {
    for (i = 1; i <= data.block[0]; i++)
      values[i-1] = data.block[i];
    return data.block[0];
  }
}

extern inline s32 i2c_smbus_write_block_data(int file, u8 command, u8 length,
                                             u8 *values)
{
  union smbus_data data;
  int i;
  if (length > 32)
    length = 32;
  for (i = 1; i <= length; i++)
    data.block[i] = values[i-1];
  data.block[0] = length;
  return i2c_smbus_access(file,SMBUS_WRITE,command,SMBUS_BLOCK_DATA,&data);
}

#endif /* ndef __KERNEL__ */

#endif
