/*
    voodoo3.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com> and 
    Ralph Metzler <rjkm@thp.uni-koeln.de>
    
    Based on code written by Ralph Metzler <rjkm@thp.uni-koeln.de> and
    Simon Vogl

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

#define DEBUG 1

/* This interfaces to the I2C bus of the Voodoo3 to gain access to
    the BT869 and possibly other I2C devices. */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include "version.h"
#include "compat.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,54))
#include <linux/bios32.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,53)
#include <linux/init.h>
#else
#define __init
#define __initdata
#endif

/* 3DFX defines */
#ifndef PCI_VENDOR_ID_3DFX
#define PCI_VENDOR_ID_3DFX 0x121a
#endif
#ifndef PCI_DEVICE_ID_3DFX_VOODOO3
#define PCI_DEVICE_ID_3DFX_VOODOO3 0x05
#endif

#ifdef MODULE
static
#else
extern
#endif
       int __init i2c_voodoo3_init(void);
static int __init voodoo3_cleanup(void);
static int voodoo3_setup(void);
static s32 voodoo3_access(struct i2c_adapter *adap, u16 addr, 
                          unsigned short flags, char read_write,
                          u8 command, int size, union i2c_smbus_data * data);
static void Voodoo3_I2CStart(void);
static void Voodoo3_I2CStop(void);
static int Voodoo3_I2CAck(int ackit);
static int Voodoo3_I2CReadByte(int ackit);
static int Voodoo3_I2CSendByte(unsigned char data);
static int Voodoo3_BusCheck(void);
static int Voodoo3_I2CRead_byte(int addr);
static int Voodoo3_I2CRead_byte_data(int addr,int command);
static int Voodoo3_I2CRead_word(int addr,int command);
static int Voodoo3_I2CWrite_byte(int addr,int command);
static int Voodoo3_I2CWrite_byte_data(int addr,int command,int data);
static int Voodoo3_I2CWrite_word(int addr,int command,long data);
static void config_v3(struct pci_dev *dev, int num);
static void voodoo3_inc(struct i2c_adapter *adapter);
static void voodoo3_dec(struct i2c_adapter *adapter);
static u32 voodoo3_func(struct i2c_adapter *adapter);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static struct i2c_algorithm smbus_algorithm = {
  /* name */		"Non-I2C SMBus adapter",
  /* id */		I2C_ALGO_SMBUS,
  /* master_xfer */	NULL,
  /* smbus_access */    &voodoo3_access,
  /* slave_send */	NULL,
  /* slave_rcv */	NULL,
  /* algo_control */	NULL,
  /* functionality */   voodoo3_func,
};

static struct i2c_adapter voodoo3_adapter = {
 "unset",
 I2C_ALGO_SMBUS | I2C_HW_SMBUS_VOODOO3,
 &smbus_algorithm,
 NULL,
 voodoo3_inc,
 voodoo3_dec,
 NULL,
 NULL,
};

static int __initdata voodoo3_initialized;
static unsigned short voodoo3_smba = 0;
static unsigned int state=0xcf980020;
static unsigned char *mem;
static int v3_num;

extern inline void outlong(int off,unsigned int dat)
{
        *((unsigned int*)(mem+off))=dat;
}


extern inline unsigned int readlong(int off)
{
        return *((unsigned int*)(mem+off));
}

extern inline void out(void)
{
        outlong(0x78,state);
        udelay(10);
}

extern inline void dat(int data)
{
  state&=~(1<<25);
  if (data)
    state|=(1<<25);
}

extern inline void clkon(void)
{
  state|=(1<<24);
}

extern inline void clkoff(void)
{
  state&=~(1<<24);
}

extern inline int rdat(void)
{
        dat(1);
        out();
        return((readlong(0x78)&(1<<27) )!=0 );
}

/* Changing the Data line while clock is 'on' (high) is a
   no-no, except for a start or stop.  */
   
void Voodoo3_I2CStart(void)
{
  clkon(); /* in theory, clk is already on */
  out();
  out();
  out();
  out();
  dat(0);
  out();
  clkoff();
  out();
}

void Voodoo3_I2CStop(void)
{
  dat(0);
  out();
  clkon();
  out();
  dat(1);
  out();
  clkoff();
  out();
  out();
  clkon();
  out();
}

int Voodoo3_I2CAck(int ackit)
{
int ack;

  out();
  clkon();
  if (!ackit) {
   ack=rdat();
  } else { dat(0); }
  out();
  clkoff();
  out();
  return ack;
}

int Voodoo3_I2CReadByte(int ackit)
{
  int i,temp;
  unsigned char data=0;

  for (i=7; i>=0; i--) {
    out();
    clkon();
    data|=(rdat()<<i);
    out();
    clkoff();
    out();
  }
  temp=Voodoo3_I2CAck(ackit);
#ifdef DEBUG
  printk("i2c-voodoo3: Readbyte ack -->0x%X, read result -->0x%X\n",temp,data);
#endif
  return data;
}

int Voodoo3_I2CSendByte(unsigned char data)
{
  int i,temp;

  for (i=7; i>=0; i--) {
    dat(temp=data&(1<<i));
    out();
    clkon();
    out();
//    dat(temp);
    out();
    clkoff();
    out();
  }
  temp=Voodoo3_I2CAck(0);
#ifdef DEBUG
  printk("i2c-voodoo3: Writebyte ack -->0x%X\n",temp);
#endif
  return temp;
}


int Voodoo3_BusCheck(void) {
/* Check the bus to see if it is in use */

  if (! rdat()) {
    printk("i2c-voodoo3: I2C bus in use or hung!  Try again later.\n");
    return 1;
  }
  return 0;
}

int Voodoo3_I2CRead_byte(int addr)
{
        int this_dat=0;

        Voodoo3_I2CStart();
        if (Voodoo3_I2CSendByte(addr)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on addr WriteByte to addr 0x%X\n",addr);
#endif
	  this_dat=-1;
	  goto ENDREAD1; }
        this_dat=Voodoo3_I2CReadByte(0);
ENDREAD1: Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Byte read at addr:0x%X result:0x%X\n",addr,this_dat);
#endif
        return this_dat;
}

int Voodoo3_I2CRead_byte_data(int addr,int command)
{
        int this_dat=0;

        Voodoo3_I2CStart();
        if (Voodoo3_I2CSendByte(addr)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on addr WriteByte to addr 0x%X\n",addr);
#endif
	  this_dat=-1;
	  goto ENDREAD2; }
        if (!Voodoo3_I2CSendByte(command)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on cmd WriteByte to addr 0x%X\n",addr);
#endif
	  this_dat=-1;
	  goto ENDREAD2; }
        this_dat=Voodoo3_I2CReadByte(0);
ENDREAD2: Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Byte read at addr:0x%X (command:0x%X) result:0x%X\n",addr,command,this_dat);
#endif
        return this_dat;
}

int Voodoo3_I2CRead_word(int addr,int command)
{
        int this_dat=0;

        Voodoo3_I2CStart();
        if (Voodoo3_I2CSendByte(addr)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on addr WriteByte to addr 0x%X\n",addr);
#endif
	  this_dat=-1;
	  goto ENDREAD3; }
        this_dat=Voodoo3_I2CReadByte(1);
        this_dat|=(Voodoo3_I2CReadByte(0)<<8);
ENDREAD3: Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Word read at addr:0x%X (command:0x%X) result:0x%X\n",addr,command,this_dat);
#endif
        return this_dat;
}

int Voodoo3_I2CWrite_byte(int addr,int command)
{
int result=0;

        Voodoo3_I2CStart();
        if (Voodoo3_I2CSendByte(addr)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on addr WriteByte to addr 0x%X\n",addr);
#endif
	  result=-1;
	  goto ENDWRITE1; }
        if (Voodoo3_I2CSendByte(command)) {
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on command WriteByte to addr 0x%X\n",addr);
#endif
	  result=-1; }
ENDWRITE1: Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Byte write at addr:0x%X command:0x%X\n",addr,command);
#endif
	return result;
}

int Voodoo3_I2CWrite_byte_data(int addr,int command,int data)
{
int result=0;

        Voodoo3_I2CStart();
        if (Voodoo3_I2CSendByte(addr)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on addr WriteByte to addr 0x%X\n",addr);
#endif
	  result=-1;
	  goto ENDWRITE2; }
        if (Voodoo3_I2CSendByte(command)) {
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on command WriteByte to addr 0x%X\n",addr);
#endif
	  result=-1;
	  goto ENDWRITE2; }
        if (Voodoo3_I2CSendByte(data)) {
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on data WriteByte to addr 0x%X\n",addr);
#endif
	result=-1; }
ENDWRITE2: Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Byte write at addr:0x%X command:0x%X data:0x%X\n",addr,command,data);
#endif
	return result;
}


int Voodoo3_I2CWrite_word(int addr,int command,long data)
{
int result=0;

        Voodoo3_I2CStart();
        if (Voodoo3_I2CSendByte(addr)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on addr WriteByte to addr 0x%X\n",addr);
#endif
	  result=-1;
	  goto ENDWRITE3; }
        if (Voodoo3_I2CSendByte(command)) {
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on command WriteByte to addr 0x%X\n",addr);
#endif
	  result=-1;
	  goto ENDWRITE3; }
        if (Voodoo3_I2CSendByte(data & 0x0FF)) {
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on data WriteByte to addr 0x%X\n",addr);
#endif
	result=-1; 
	goto ENDWRITE3; }
        if (Voodoo3_I2CSendByte((data &0x0FF00)>>8)) {
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on data byte 2 WriteByte to addr 0x%X\n",addr);
#endif
	result=-1; }
ENDWRITE3: Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Word write at addr:0x%X command:0x%X data:0x%lX\n",addr,command,data);
#endif
	return result;
}

void config_v3(struct pci_dev *dev, int num)
{
        unsigned int cadr;

        /* map Voodoo3 memory */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,13)
        cadr = dev->resource[0].start;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,54)
        cadr = dev->base_address[0];
#else
        pcibios_read_config_dword(dev->bus->number, dev->devfn,
                                  PCI_BASE_ADDRESS_0,&cadr);
#endif
        cadr&=PCI_BASE_ADDRESS_MEM_MASK;
        mem=ioremap(cadr, 0x1000);
        
        *((unsigned int *)(mem+0x70))=0x8160;
        *((unsigned int *)(mem+0x78))=0xcf980020;
	printk("i2c-voodoo3: Using Banshee/Voodoo3 at 0x%p\n",mem);
}



/* Detect whether a Voodoo3 can be found, and initialize it, where necessary.
   Note the differences between kernels with the old PCI BIOS interface and
   newer kernels with the real PCI interface. In compat.h some things are
   defined to make the transition easier. */
static int voodoo3_setup(void)
{
        struct pci_dev *dev = pci_devices;
        int result=0;
	int flag=0;

        v3_num=0;

        while (dev)
        {
                if (dev->vendor == PCI_VENDOR_ID_3DFX)
                        if (dev->device == PCI_DEVICE_ID_3DFX_VOODOO3) {
			  if (!flag) {
                                config_v3(dev,v3_num++);
			  } else { v3_num++; }
			  flag=1;
			}
                if (result)
                        return result;
                dev = dev->next;
        }
        if(v3_num > 0) {
                printk(KERN_INFO "i2c-voodoo3: %d Banshee/Voodoo3(s) found.\n", v3_num);
		return 0;
        } else {
                printk(KERN_INFO "v3tv: No Voodoo3 found.\n");
        	return -ENODEV;
	}
}


/* Return -1 on error. See smbus.h for more information */
s32 voodoo3_access(struct i2c_adapter *adap, u16 addr, 
                   unsigned short flags, char read_write,
                   u8 command, int size, union i2c_smbus_data * data)
{
int temp=0;

  if (Voodoo3_BusCheck()) return -1;

  if ((read_write != I2C_SMBUS_READ) && (read_write != I2C_SMBUS_WRITE)) {
	printk("i2c-voodoo3: Unknown read_write command! (0x%X)\n",read_write);
	return -1;
  }
  addr=((addr & 0x7f) << 1) | (read_write & 0x01);
  switch(size) {
    case I2C_SMBUS_QUICK:
    	Voodoo3_I2CStart();
	if (Voodoo3_I2CSendByte(addr)) { 
#ifdef DEBUG
	  printk("i2c-voodoo3: No Ack on addr WriteByte to addr 0x%X\n",addr);
#endif
	  return -1; }
    case I2C_SMBUS_BYTE:
    	if (read_write == I2C_SMBUS_READ) { temp=Voodoo3_I2CRead_byte(addr); data->byte=temp;
	} else { temp=Voodoo3_I2CWrite_byte(addr,command); }
	goto TESTACK;
    case I2C_SMBUS_BYTE_DATA:
    	if (read_write == I2C_SMBUS_READ) { temp=Voodoo3_I2CRead_byte_data(addr,command); data->byte=temp;
	} else { temp=Voodoo3_I2CWrite_byte_data(addr,command,data->byte); }
	goto TESTACK;
    case I2C_SMBUS_WORD_DATA:
    	if (read_write == I2C_SMBUS_READ) { temp=Voodoo3_I2CRead_word(addr,command); data->word=temp;
	} else { temp=Voodoo3_I2CWrite_word(addr,command,data->word); }
	goto TESTACK;
    case I2C_SMBUS_PROC_CALL:
	printk("i2c-voodoo3: Proc call not supported.\n");
	return -1;
    case I2C_SMBUS_BLOCK_DATA:
	printk("i2c-voodoo3: Block transfers not supported yet.\n");
	return -1;
  }

TESTACK: if (temp < 0) {
#ifdef DEBUG
		printk("i2c-voodoo3: no device at 0x%X\n",addr);
#endif
		return -1;
	  }
	return 0;
}

void voodoo3_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void voodoo3_dec(struct i2c_adapter *adapter)
{

	MOD_DEC_USE_COUNT;
}

u32 voodoo3_func(struct i2c_adapter *adapter)
{
  return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE | 
         I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA;
}

int __init i2c_voodoo3_init(void)
{
  int res;
  printk("i2c-voodoo3.o version %s (%s)\n",LM_VERSION,LM_DATE);
#ifdef DEBUG
/* PE- It might be good to make this a permanent part of the code! */
  if (voodoo3_initialized) {
    printk("i2c-voodoo3.o: Oops, voodoo3_init called a second time!\n");
    return -EBUSY;
  }
#endif
  voodoo3_initialized = 0;
  if ((res = voodoo3_setup())) {
    printk("i2c-voodoo3.o: Voodoo3 not detected, module not inserted.\n");
    voodoo3_cleanup();
    return res;
  }
  voodoo3_initialized ++;
  sprintf(voodoo3_adapter.name,"SMBus Voodoo3 adapter at %04x",voodoo3_smba);
  if ((res = i2c_add_adapter(&voodoo3_adapter))) {
    printk("i2c-voodoo3.o: Adapter registration failed, module not inserted.\n");
    voodoo3_cleanup();
    return res;
  }
  voodoo3_initialized++;
  printk("i2c-voodoo3.o: Voodoo3 I2C bus detected and initialized\n");
  return 0;
}

int __init voodoo3_cleanup(void)
{
  int res;
  
  iounmap(mem);
  if (voodoo3_initialized >= 2)
  {
    if ((res = i2c_del_adapter(&voodoo3_adapter))) {
      printk("i2c-voodoo3.o: i2c_del_adapter failed, module not removed\n");
      return res;
    } else
      voodoo3_initialized--;
  }
  if (voodoo3_initialized >= 1) {
    voodoo3_initialized--;
  }
  return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com> and Ralph Metzler <rjkm@thp.uni-koeln.de>");
MODULE_DESCRIPTION("Voodoo3 I2C/SMBus driver");


int init_module(void)
{
  return i2c_voodoo3_init();
}

int cleanup_module(void)
{
  return voodoo3_cleanup();
}

#endif /* MODULE */

