/*
    voodoo3.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
    Philip Edelbrock <phil@netroedge.com>
    
    Based on code written by Ralph  Metzler <rjkm@thp.uni-koeln.de>

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

/* This interfaces to the I2C bus of the Voodoo3 to gain access to
    the BT869 and possibly other I2C devices. */

#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include "smbus.h"
#include "version.h"
#include "compat.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,54))
#include <linux/bios32.h>
#endif

/* 3DFX defines */
#ifndef PCI_VENDOR_ID_3DFX
#define PCI_VENDOR_ID_3DFX 0x121a
#endif
#ifndef PCI_DEVICE_ID_3DFX_VOODOO3
#define PCI_DEVICE_ID_3DFX_VOODOO3 0x05
#endif

/* insmod parameters */

static int voodoo3_init(void);
static int voodoo3_cleanup(void);
static int voodoo3_setup(void);
static s32 voodoo3_access(u8 addr, char read_write,
                        u8 command, int size, union smbus_data * data);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static struct smbus_adapter voodoo3_adapter;
static int voodoo3_initialized;
static unsigned short voodoo3_smba = 0;
static unsigned int state=0xcf980020;
static unsigned char *mem;
static int v3_num;

inline outlong(int off,unsigned int dat)
{
        *((unsigned int*)(mem+off))=dat;
}


inline unsigned int readlong(int off)
{
        return *((unsigned int*)(mem+off));
}

inline out(void)
{
        outlong(0x78,state);
        udelay(10);
}

inline void dat(int data)
{
  state&=~(1<<25);
  if (data)
    state|=(1<<25);
}

inline void clkon(void)
{
  state|=(1<<24);
}

inline void clkoff(void)
{
  state&=~(1<<24);
}

inline int rdat(void)
{
        dat(1);
        out();
        return((readlong(0x78)&(1<<27) )!=0 );
}

void Voodoo3_I2CStart(void)
{
  dat(1);
  out();
  clkon();
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
}

int Voodoo3_I2CAck(int ack)
{
  dat(ack);
  out();
  clkon();
  out();
  ack=rdat();
  clkoff();
  out();
  return ack;
}

unsigned char Voodoo3_I2CReadByte(int ack)
{
  int i;
  unsigned char data=0;

  clkoff();
  dat(1);
  out();
  for (i=7; i>=0; i--) {
    clkon();
    out();
    data|=(rdat()<<i);
    clkoff();
    out();
  }
  Voodoo3_I2CAck(ack);
  return data;
}

int Voodoo3_I2CSendByte(unsigned char data)
{
  int i;

  clkoff();
  dat(0);
  out();

  for (i=7; i>=0; i--) {
    dat(data&(1<<i));
    out();
    clkon();
    out();
    clkoff();
    out();
  }
  return Voodoo3_I2CAck(1);
}

/* Note, this is actually a byte read and not a byte_data read */
/* I.e., the command value is dumped.                          */
unsigned char Voodoo3_I2CRead(int adr,int command)
{
        int i,j;
        unsigned char dat;

        Voodoo3_I2CStart();
        Voodoo3_I2CSendByte(adr);
        dat=Voodoo3_I2CReadByte(1);
        Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Byte read at addr:0x%X (command:0x%X) result:0x%X\n",adr,command,dat);
#endif
        return dat;
}

void Voodoo3_I2CWrite(int addr,int command,int data)
{
        Voodoo3_I2CStart();
        Voodoo3_I2CSendByte(addr);
        Voodoo3_I2CSendByte(command);
        Voodoo3_I2CSendByte(data);
        Voodoo3_I2CStop();
#ifdef DEBUG
	printk("i2c-voodoo3: Byte write at addr:0x%X command:0x%X data:0x%X\n",addr,command,data);
#endif
}

void config_v3(struct pci_dev *dev, int num)
{
        unsigned int cadr;

        /* map Voodoo3 memory */
        cadr=dev->base_address[0];
        cadr&=PCI_BASE_ADDRESS_MEM_MASK;
        mem=ioremap(cadr, 0x1000);
        
        /* Enable TV out mode, Voodoo3_I2C bus, etc. */
        *((unsigned int *)(mem+0x70))=0x8160;
        *((unsigned int *)(mem+0x78))=0xcf980020;
}



/* Detect whether a Voodoo3 can be found, and initialize it, where necessary.
   Note the differences between kernels with the old PCI BIOS interface and
   newer kernels with the real PCI interface. In compat.h some things are
   defined to make the transition easier. */
static int voodoo3_setup(void)
{
        struct pci_dev *dev = pci_devices;
        int result=0;

        v3_num=0;

        while (dev)
        {
                if (dev->vendor == PCI_VENDOR_ID_3DFX)
                        if (dev->device == PCI_DEVICE_ID_3DFX_VOODOO3)
                                config_v3(dev,v3_num++);                  
                if (result)
                        return result;
                dev = dev->next;
        }
        if(v3_num) {
                printk(KERN_INFO "v3tv: %d Voodoo3(s) found.\n", v3_num);
		return 0;
        } else {
                printk(KERN_INFO "v3tv: No Voodoo3 found.\n");
        	return -ENODEV;
	}
}


/* Return -1 on error. See smbus.h for more information */
s32 voodoo3_access(u8 addr, char read_write,
                 u8 command, int size, union smbus_data * data)
{
  if ((size == SMBUS_BYTE_DATA) || (size == SMBUS_BYTE)) {
        addr=((addr & 0x7f) << 1) | (read_write & 0x01);
  	if (read_write == SMBUS_READ) {
	  data->byte=Voodoo3_I2CRead(addr,command); 
printk("i2c-voodoo3: access returning 0x%X\n",data->byte);
	} else {
	  Voodoo3_I2CWrite(addr,command,data->byte);
	}
  	return 0;
  } else { return -1; } /* The rest isn't implemented yet */
}


int voodoo3_init(void)
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
  voodoo3_adapter.id = ALGO_SMBUS | SMBUS_VOODOO3;
  voodoo3_adapter.algo = &smbus_algorithm;
  voodoo3_adapter.smbus_access = &voodoo3_access;
  if ((res = smbus_add_adapter(&voodoo3_adapter))) {
    printk("i2c-voodoo3.o: Adapter registration failed, module not inserted.\n");
    voodoo3_cleanup();
    return res;
  }
  voodoo3_initialized++;
  printk("i2c-voodoo3.o: Voodoo3 I2C bus detected and initialized\n");
  return 0;
}

int voodoo3_cleanup(void)
{
  int res;
  
  iounmap(mem);
  if (voodoo3_initialized >= 2)
  {
    if ((res = smbus_del_adapter(&voodoo3_adapter))) {
      printk("i2c-voodoo3.o: smbus_del_adapter failed, module not removed\n");
      return res;
    } else
      voodoo3_initialized--;
  }
  if (voodoo3_initialized >= 1) {
    voodoo3_initialized--;
  }
  return 0;
}

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com> and Ralph");
MODULE_DESCRIPTION("Voodoo3 I2C/SMBus driver");

int init_module(void)
{
  return voodoo3_init();
}

int cleanup_module(void)
{
  return voodoo3_cleanup();
}

#endif /* MODULE */

