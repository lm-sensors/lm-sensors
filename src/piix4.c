/*
    smbus.c - A Linux module for reading sensor data.
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl> and
    Philip Edelbrock <phil@netroedge.com>

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

/* Note: New PCI (non-BIOS) interface introduced in 2.1.54! */

#include <linux/pci.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include "smbus.h"
#include "version.h"
#include "compat.h"
#include "piix4.h"

static int piix4_init(void);
static int piix4_cleanup(void);
static int piix4_setup(void);
static s32 piix4_access(u8 addr, char read_write,
                        u8 command, int size, union smbus_data * data);
/* Internal functions */
static void do_pause( unsigned int amount );
static int SMBus_PIIX4_Transaction(void);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static struct smbus_adapter piix4_adapter;
static int piix4_initialized;

/* Global variables  -- At least some of these may be modified/moved later */
static unsigned short PIIX4_smba = 0;
static struct semaphore smbus_piix4_sem = MUTEX;


/* Detect whether a PIIX4 can be found, and initialize it, where necessary. 
   Return -ENODEV if not found. */
int piix4_setup(void)
{
  int error_return=0;
  unsigned char temp;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0))
  struct pci_dev *PIIX4_dev;
#else
  unsigned char PIIX4_bus, PIIX4_devfn;
#endif

  if (pci_present() == 0) {
    printk("SMBus: Error: No PCI-bus found!\n");
    error_return=-ENODEV;
  } else {

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0))
    PIIX4_dev = pci_find_device(PCI_VENDOR_ID_INTEL, 
                                 PCI_DEVICE_ID_INTEL_82371AB_0, NULL);
    if(PIIX4_dev == NULL) {
#else
    if(pcibios_find_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_0, 
                           0, &PIIX4_bus, &PIIX4_devfn)) {
#endif
      printk("SMBus: Error: Can't detect PIIX4!\n");
      error_return=-ENODEV;
    } else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0))
      /* For some reason, pci_read_config fails here sometimes! */
      pcibios_read_config_word(PIIX4_dev->bus->number, PIIX4_dev->devfn | 3, 
                               SMBBA, &PIIX4_smba);
#else
      pci_read_config_word_united(PIIX4_dev, PIIX4_bus ,PIIX4_devfn | 3,
                                  SMBBA,&PIIX4_smba);
#endif
      PIIX4_smba &= 0xfff0;
      if (check_region(PIIX4_smba, 8)) {
        printk("PIIX4_smb region 0x%x already in use!\n",
               PIIX4_smba);
        error_return=-ENODEV;
      } else {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0))
        pcibios_read_config_byte(PIIX4_dev->bus->number, 
		PIIX4_dev->devfn | 3, SMBHSTCFG, &temp);
#else
        pci_read_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn | 3, 
                                    SMBHSTCFG, &temp);
#endif
#ifdef FORCE_PIIX4_ENABLE
/* This should never need to be done, but has been noted that
   many Dell machines have the SMBus interface on the PIIX4
   disabled!? NOTE: This assumes I/O space and other allocations WERE
   done by the Bios!  Don't complain if your hardware does weird 
   things after enabling this. :') Check for Bios updates before
   resorting to this.  */
	if ((temp & 1) == 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0))
          pcibios_write_config_byte(PIIX4_dev->bus->number, PIIX4_dev->devfn | 3, 
                               SMBHSTCFG, temp | 1);
#else
          pcibios_write_config_byte(PIIX4_bus, PIIX4_devfn | 3, SMBHSTCFG, temp | 1);
#endif
	  printk("SMBus: WARNING: PIIX4 SMBus interface has been FORCEFULLY ENABLED!!\n");
	  /* Update configuration value */
          pci_read_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn | 3, SMBHSTCFG, &temp);
	  /* Note: We test the bit again in the next 'if' just to be sure... */
	}
#endif
	if ((temp & 1) == 0) {
          printk("SMBUS: Error: Host SMBus controller not enabled!\n");     
          piix4_initialized=0;
	  error_return=-ENODEV;
	} else {
        	/* Everything is happy, let's grab the memory and set things up. */
        	request_region(PIIX4_smba, 8, "SMBus");       
        	piix4_initialized=1;
	}
#ifdef DEBUG
        if ((temp & 0x0E) == 8)
          printk("SMBUS: PIIX4 using Interrupt 9 for SMBus.\n");
        else if ((temp & 0x0E) == 0)
          printk("SMBUS: PIIX4 using Interrupt SMI# for SMBus.\n");
        else 
          printk( "SMBUS: PIIX4: Illegal Interrupt configuration (or code out of date)!\n");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0))
        pcibios_read_config_byte(PIIX4_dev->bus->number, 
		PIIX4_dev->devfn | 3, SMBREV, &temp);
#else
        pci_read_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn | 3, 
                                    SMBREV, &temp);
#endif
        printk("SMBUS: SMBREV = 0x%X\n",temp);
#endif
      }
    }
  if (error_return)
    printk("SMBus setup failed.\n");
#ifdef DEBUG
  else
    printk("PIIX4_smba = 0x%X\n",PIIX4_smba);
#endif
  }
  return error_return;
}

/* Internally used pause function */
void do_pause( unsigned int amount )
{
      current->state = TASK_INTERRUPTIBLE;
      schedule_timeout(amount);
}

/* Another internally used function */
int SMBus_PIIX4_Transaction(void) 
{
  int temp;
  int result=0;
  int timeout=0;

  /* Make sure the SMBus host is ready to start transmitting */
  if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
    printk("SMBus: SMBus_Read: SMBus busy (%02x). Resetting... ",temp);
#endif
    outb_p(temp, SMBHSTSTS);
    if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
      printk("Failed! (%02x)\n",temp);
#endif
      return -1;
    } else {
#ifdef DEBUG
      printk("Successfull!\n");
#endif
    }
  }

  /* start the transaction by setting bit 6 */
  outb_p(inb(SMBHSTCNT) | 0x040, SMBHSTCNT); 

  /* Wait for a fraction of a second! (See PIIX4 docs errata) */
  do_pause(1);

  /* Poll Host_busy bit */
  temp=inb_p(SMBHSTSTS) & 0x01;
  while (temp & (timeout++ < MAX_TIMEOUT)) {
    /* Wait for a while and try again*/
    do_pause(1);
    temp = inb_p(SMBHSTSTS) & 0x01;
  }

  /* If the SMBus is still busy, we give up */
  if (timeout >= MAX_TIMEOUT) {
#ifdef DEBUG
    printk("SMBus: SMBus_Read: SMBus Timeout!\n"); 
    result = -1;
#endif
  }

  temp = inb_p(SMBHSTSTS);

  if (temp  & 0x10) {
    result = -1;
#ifdef DEBUG
    printk("SMBus error: Failed bus transaction\n");
#endif
  }

  if (temp & 0x08) {
    result = -1;
    printk("SMBus error: Bus collision! SMBus may be locked until next hard reset. (sorry!)\n");
    /* Clock stops and slave is stuck in mid-transmission */
  }

  if (temp & 0x04) {
    result = -1;
#ifdef DEBUG
    printk("SMBus error: no response!\n");
#endif
  }

  if (inb_p(SMBHSTSTS) != 0x00)
    outb_p( inb(SMBHSTSTS), SMBHSTSTS);

  if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
    printk("SMBus error: Failed reset at end of transaction (%02x)\n",temp);
#endif
  }
  return result;
}

/* Return -1 on error. See smbus.h for more information */
s32 piix4_access(u8 addr, char read_write,
                 u8 command, int size, union smbus_data * data)
{
  int i,len;

  down(&smbus_piix4_sem);

  outb_p((size & 0x1C) + (ENABLE_INT9 & 1), SMBHSTCNT);

  switch(size) {
    case SMBUS_QUICK:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      break;
    case SMBUS_BYTE:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      if (read_write == SMBUS_WRITE)
        outb_p(command, SMBHSTCMD);
      break;
    case SMBUS_BYTE_DATA:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      outb_p(command, SMBHSTCMD);
      if (read_write == SMBUS_WRITE)
        outb_p(data->byte,SMBHSTDAT0);
      break;
    case SMBUS_WORD_DATA:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      outb_p(command, SMBHSTCMD);
      if (read_write == SMBUS_WRITE) {
        outb_p(data->word & 0xff,SMBHSTDAT0);
        outb_p((data->word & 0xff00) >> 8,SMBHSTDAT1);
      }
      break;
    case SMBUS_BLOCK_DATA:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      outb_p(command, SMBHSTCMD);
      if (read_write == SMBUS_WRITE) {
        len = data->block[0];
        if (len < 0) 
          len = 0;
        if (len > 32)
          len = 32;
        outb_p(len,SMBHSTDAT0);
        i = inb_p(SMBHSTCNT); /* Reset SMBBLKDAT */
        for (i = 1; i <= len; i ++)
          outb_p(data->block[i],SMBBLKDAT);
        break;
      }
  }

  if (SMBus_PIIX4_Transaction()) { /* Error in transaction */ 
    up(&smbus_piix4_sem);
    return -1; 
  }

  if ((read_write == SMBUS_WRITE) || (size == SMBUS_QUICK)) {
    up(&smbus_piix4_sem);
    return 0;
  }
}  


int piix4_init(void)
{
  int res;
  printk("piix4.o version %s (%s)\n",LM_VERSION,LM_DATE);
#ifdef DEBUG
/* PE- It might be good to make this a permanent part of the code! */
  if (piix4_initialized) {
    printk("piix4.o: Oops, piix4_init called a second time!\n");
    return -EBUSY;
  }
#endif
  piix4_initialized = 0;
  if ((res = piix4_setup())) {
    printk("piix4.o: PIIX4 not detected, module not inserted.\n");
    piix4_cleanup();
    return res;
  }
  piix4_initialized ++;
  strcpy(piix4_adapter.name,"SMBus PIIX4 adapter");
  piix4_adapter.id = ALGO_SMBUS | SMBUS_PIIX4;
  piix4_adapter.algo = &smbus_algorithm;
  piix4_adapter.smbus_access = &piix4_access;
  if ((res = smbus_add_adapter(&piix4_adapter))) {
    printk("piix4.o: Adapter registration failed, module not inserted.\n");
    piix4_cleanup();
    return res;
  }
  piix4_initialized++;
  printk("piix4.o: PIIX4 bus detected and initialized\n");
  return 0;
}

int piix4_cleanup(void)
{
  int res;
  if (piix4_initialized >= 2)
  {
    if ((res = smbus_del_adapter(&piix4_adapter))) {
      printk("piix4.o: smbus_del_adapter failed, module not removed\n");
      return res;
    } else
      piix4_initialized=0;
  }
  if (piix4_initialized >= 1) {
    if (piix4_initialized) {
		release_region(PIIX4_smba, 8);
		piix4_initialized=0;
    }
  }
  return 0;
}

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("PIIX4 SMBus driver");

int init_module(void)
{
  return piix4_init();
}

int cleanup_module(void)
{
  return piix4_cleanup();
}

#endif /* MODULE */

