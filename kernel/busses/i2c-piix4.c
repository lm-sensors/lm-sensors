/*
    piix4.c - Part of lm_sensors, Linux kernel modules for hardware
              monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
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

/* Note: we assume there can only be one PIIX4, with one SMBus interface */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/ioport.h>
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

/* PIIX4 SMBus address offsets */
#define SMBHSTSTS (0 + piix4_smba)
#define SMBHSLVSTS (1 + piix4_smba)
#define SMBHSTCNT (2 + piix4_smba)
#define SMBHSTCMD (3 + piix4_smba)
#define SMBHSTADD (4 + piix4_smba)
#define SMBHSTDAT0 (5 + piix4_smba)
#define SMBHSTDAT1 (6 + piix4_smba)
#define SMBBLKDAT (7 + piix4_smba)
#define SMBSLVCNT (8 + piix4_smba)
#define SMBSHDWCMD (9 + piix4_smba)
#define SMBSLVEVT (0xA + piix4_smba)
#define SMBSLVDAT (0xC + piix4_smba)

/* PCI Address Constants */
#define SMBBA     0x090
#define SMBHSTCFG 0x0D2
#define SMBSLVC   0x0D3
#define SMBSHDW1  0x0D4
#define SMBSHDW2  0x0D5
#define SMBREV    0x0D6

/* Other settings */
#define MAX_TIMEOUT 500
#define  ENABLE_INT9 0

/* PIIX4 constants */
#define PIIX4_QUICK      0x00
#define PIIX4_BYTE       0x04
#define PIIX4_BYTE_DATA  0x08
#define PIIX4_WORD_DATA  0x0C
#define PIIX4_BLOCK_DATA 0x14

/* insmod parameters */

/* If force is set to anything different from 0, we forcibly enable the
   PIIX4. DANGEROUS! */
static int force = 0;
MODULE_PARM(force,"i");
MODULE_PARM_DESC(force,"Forcibly enable the PIIX4. DANGEROUS!");

/* If force_addr is set to anything different from 0, we forcibly enable
   the PIIX4 at the given address. VERY DANGEROUS! */
static int force_addr = 0;
MODULE_PARM(force_addr,"i");
MODULE_PARM_DESC(force_addr,"Forcibly enable the PIIX4 at the given address. "
                            "EXTREMELY DANGEROUS!");

#ifdef MODULE
static
#else
extern
#endif
       int __init i2c_piix4_init(void);
static int __init piix4_cleanup(void);
static int piix4_setup(void);
static s32 piix4_access(struct i2c_adapter *adap, u16 addr, 
                        unsigned short flags, char read_write,
                        u8 command, int size, union i2c_smbus_data * data);
static void piix4_do_pause( unsigned int amount );
static int piix4_transaction(void);
static void piix4_inc(struct i2c_adapter *adapter);
static void piix4_dec(struct i2c_adapter *adapter);
static u32 piix4_func(struct i2c_adapter *adapter);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static struct i2c_algorithm smbus_algorithm = {
  /* name */		"Non-I2C SMBus adapter",
  /* id */		I2C_ALGO_SMBUS,
  /* master_xfer */	NULL,
  /* smbus_access */    piix4_access,
  /* slave_send */	NULL,
  /* slave_rcv */	NULL,
  /* algo_control */	NULL,
  /* functionality */   piix4_func,
};

static struct i2c_adapter piix4_adapter = {
  "unset",
  I2C_ALGO_SMBUS | I2C_HW_SMBUS_PIIX4,
  &smbus_algorithm,
  NULL,
  piix4_inc,
  piix4_dec,
  NULL,
  NULL,
};

static int __initdata piix4_initialized;
static unsigned short piix4_smba = 0;


/* Detect whether a PIIX4 can be found, and initialize it, where necessary.
   Note the differences between kernels with the old PCI BIOS interface and
   newer kernels with the real PCI interface. In compat.h some things are
   defined to make the transition easier. */
int piix4_setup(void)
{
  int error_return=0;
  unsigned char temp;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,54))
  struct pci_dev *PIIX4_dev;
#else
  unsigned char PIIX4_bus, PIIX4_devfn;
  int i,res;
#endif

  /* First check whether we can access PCI at all */
  if (pci_present() == 0) {
    printk("i2c-piix4.o: Error: No PCI-bus found!\n");
    error_return=-ENODEV;
    goto END;
  }

  /* Look for the PIIX4, function 3 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,54))
  /* Note: we keep on searching until we have found 'function 3' */
  PIIX4_dev = NULL;
  do
    PIIX4_dev = pci_find_device(PCI_VENDOR_ID_INTEL, 
                                PCI_DEVICE_ID_INTEL_82371AB_3, PIIX4_dev);
  while(PIIX4_dev && (PCI_FUNC(PIIX4_dev->devfn) != 3));
  if(PIIX4_dev == NULL) {
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,1,54) */
  for (i = 0; 
       ! (res = pcibios_find_device(PCI_VENDOR_ID_INTEL,
                                    PCI_DEVICE_ID_INTEL_82371AB_3,
                                    i,&PIIX4_bus, &PIIX4_devfn)) && 
         PCI_FUNC(PIIX4_devfn) != 3; 
       i++);
     
  if (res) {
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,54) */
    printk("i2c-piix4.o: Error: Can't detect PIIX4, function 3!\n");
    error_return=-ENODEV;
    goto END;
  } 

/* Determine the address of the SMBus areas */
  if (force_addr) {
    piix4_smba = force_addr & 0xfff0;
    force = 0;
  } else {

    pci_read_config_word_united(PIIX4_dev, PIIX4_bus ,PIIX4_devfn,
                                SMBBA,&piix4_smba);
    piix4_smba &= 0xfff0;
  }

  if (check_region(piix4_smba, 8)) {
    printk("i2c-piix4.o: PIIX4_smb region 0x%x already in use!\n", piix4_smba);
    error_return=-ENODEV;
    goto END;
  }

  pci_read_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn,
                              SMBHSTCFG, &temp);
/* If force_addr is set, we program the new address here. Just to make
   sure, we disable the PIIX4 first. */
  if (force_addr) {
    pci_write_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn,
                                SMBHSTCFG, temp & 0xfe);
    pci_write_config_word_united(PIIX4_dev, PIIX4_bus ,PIIX4_devfn,
                                 SMBBA,piix4_smba);
    pci_write_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn,
                                SMBHSTCFG, temp | 0x01);
    printk("i2c-piix4.o: WARNING: PIIX4 SMBus interface set to new "
           "address %04x!\n",piix4_smba);
  } else if ((temp & 1) == 0) {
    if (force) {
/* This should never need to be done, but has been noted that
   many Dell machines have the SMBus interface on the PIIX4
   disabled!? NOTE: This assumes I/O space and other allocations WERE
   done by the Bios!  Don't complain if your hardware does weird 
   things after enabling this. :') Check for Bios updates before
   resorting to this.  */
      pci_write_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn,
                                       SMBHSTCFG, temp | 1);
      printk("i2c-piix4.o: WARNING: PIIX4 SMBus interface has been FORCEFULLY "
             "ENABLED!\n");
    } else {
      printk("SMBUS: Error: Host SMBus controller not enabled!\n");     
      error_return=-ENODEV;
      goto END;
    }
  }

  /* Everything is happy, let's grab the memory and set things up. */
  request_region(piix4_smba, 8, "piix4-smbus");       

#ifdef DEBUG
  if ((temp & 0x0E) == 8)
     printk("i2c-piix4.o: PIIX4 using Interrupt 9 for SMBus.\n");
  else if ((temp & 0x0E) == 0)
     printk("i2c-piix4.o: PIIX4 using Interrupt SMI# for SMBus.\n");
  else 
     printk("i2c-piix4.o: PIIX4: Illegal Interrupt configuration (or code out "
            "of date)!\n");

  pci_read_config_byte_united(PIIX4_dev, PIIX4_bus, PIIX4_devfn, SMBREV, 
                              &temp);
  printk("i2c-piix4.o: SMBREV = 0x%X\n",temp);
  printk("i2c-piix4.o: PIIX4_smba = 0x%X\n",piix4_smba);
#endif /* DEBUG */

END:
  return error_return;
}


/* Internally used pause function */
void piix4_do_pause( unsigned int amount )
{
      current->state = TASK_INTERRUPTIBLE;
      schedule_timeout(amount);
}

/* Another internally used function */
int piix4_transaction(void) 
{
  int temp;
  int result=0;
  int timeout=0;

#ifdef DEBUG
  printk("i2c-piix4.o: Transaction (pre): CNT=%02x, CMD=%02x, ADD=%02x, DAT0=%02x, "
         "DAT1=%02x\n",
         inb_p(SMBHSTCNT),inb_p(SMBHSTCMD),inb_p(SMBHSTADD),inb_p(SMBHSTDAT0),
         inb_p(SMBHSTDAT1));
#endif

  /* Make sure the SMBus host is ready to start transmitting */
  if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
    printk("i2c-piix4.o: SMBus busy (%02x). Resetting... \n",temp);
#endif
    outb_p(temp, SMBHSTSTS);
    if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
      printk("i2c-piix4.o: Failed! (%02x)\n",temp);
#endif
      return -1;
    } else {
#ifdef DEBUG
      printk("i2c-piix4.o: Successfull!\n");
#endif
    }
  }

  /* start the transaction by setting bit 6 */
  outb_p(inb(SMBHSTCNT) | 0x040, SMBHSTCNT); 

  /* We will always wait for a fraction of a second! (See PIIX4 docs errata) */
  do {
    piix4_do_pause(1);
    temp=inb_p(SMBHSTSTS);
  } while ((temp & 0x01) && (timeout++ < MAX_TIMEOUT));

  /* If the SMBus is still busy, we give up */
  if (timeout >= MAX_TIMEOUT) {
#ifdef DEBUG
    printk("i2c-piix4.o: SMBus Timeout!\n"); 
    result = -1;
#endif
  }

  if (temp & 0x10) {
    result = -1;
#ifdef DEBUG
    printk("i2c-piix4.o: Error: Failed bus transaction\n");
#endif
  }

  if (temp & 0x08) {
    result = -1;
    printk("i2c-piix4.o: Bus collision! SMBus may be locked until next hard
           reset. (sorry!)\n");
    /* Clock stops and slave is stuck in mid-transmission */
  }

  if (temp & 0x04) {
    result = -1;
#ifdef DEBUG
    printk("i2c-piix4.o: Error: no response!\n");
#endif
  }

  if (inb_p(SMBHSTSTS) != 0x00)
    outb_p( inb(SMBHSTSTS), SMBHSTSTS);

  if ((temp = inb_p(SMBHSTSTS)) != 0x00) {
#ifdef DEBUG
    printk("i2c-piix4.o: Failed reset at end of transaction (%02x)\n",temp);
#endif
  }
#ifdef DEBUG
  printk("i2c-piix4.o: Transaction (post): CNT=%02x, CMD=%02x, ADD=%02x, "
         "DAT0=%02x, DAT1=%02x\n",
         inb_p(SMBHSTCNT),inb_p(SMBHSTCMD),inb_p(SMBHSTADD),inb_p(SMBHSTDAT0),
         inb_p(SMBHSTDAT1));
#endif
  return result;
}

/* Return -1 on error. See smbus.h for more information */
s32 piix4_access(struct i2c_adapter *adap, u16 addr, 
                 unsigned short flags, char read_write,
                 u8 command, int size, union i2c_smbus_data * data)
{
  int i,len;

  switch(size) {
    case I2C_SMBUS_PROC_CALL:
      printk("i2c-piix4.o: I2C_SMBUS_PROC_CALL not supported!\n");
      return -1;
    case I2C_SMBUS_QUICK:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      size = PIIX4_QUICK;
      break;
    case I2C_SMBUS_BYTE:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      if (read_write == I2C_SMBUS_WRITE)
        outb_p(command, SMBHSTCMD);
      size = PIIX4_BYTE;
      break;
    case I2C_SMBUS_BYTE_DATA:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      outb_p(command, SMBHSTCMD);
      if (read_write == I2C_SMBUS_WRITE)
        outb_p(data->byte,SMBHSTDAT0);
      size = PIIX4_BYTE_DATA;
      break;
    case I2C_SMBUS_WORD_DATA:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      outb_p(command, SMBHSTCMD);
      if (read_write == I2C_SMBUS_WRITE) {
        outb_p(data->word & 0xff,SMBHSTDAT0);
        outb_p((data->word & 0xff00) >> 8,SMBHSTDAT1);
      }
      size = PIIX4_WORD_DATA;
      break;
    case I2C_SMBUS_BLOCK_DATA:
      outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), SMBHSTADD);
      outb_p(command, SMBHSTCMD);
      if (read_write == I2C_SMBUS_WRITE) {
        len = data->block[0];
        if (len < 0) 
          len = 0;
        if (len > 32)
          len = 32;
        outb_p(len,SMBHSTDAT0);
        i = inb_p(SMBHSTCNT); /* Reset SMBBLKDAT */
        for (i = 1; i <= len; i ++)
          outb_p(data->block[i],SMBBLKDAT);
      }
      size = PIIX4_BLOCK_DATA;
      break;
  }

  outb_p((size & 0x1C) + (ENABLE_INT9 & 1), SMBHSTCNT);

  if (piix4_transaction()) /* Error in transaction */ 
    return -1; 
  
  if ((read_write == I2C_SMBUS_WRITE) || (size == PIIX4_QUICK))
    return 0;
  

  switch(size) {
    case PIIX4_BYTE: /* Where is the result put? I assume here it is in
                        SMBHSTDAT0 but it might just as well be in the
                        SMBHSTCMD. No clue in the docs */
 
      data->byte = inb_p(SMBHSTDAT0);
      break;
    case PIIX4_BYTE_DATA:
      data->byte = inb_p(SMBHSTDAT0);
      break;
    case PIIX4_WORD_DATA:
      data->word = inb_p(SMBHSTDAT0) + (inb_p(SMBHSTDAT1) << 8);
      break;
    case PIIX4_BLOCK_DATA:
      data->block[0] = inb_p(SMBHSTDAT0);
      i = inb_p(SMBHSTCNT); /* Reset SMBBLKDAT */
      for (i = 1; i <= data->block[0]; i++)
        data->block[i] = inb_p(SMBBLKDAT);
      break;
  }
  return 0;
}

void piix4_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void piix4_dec(struct i2c_adapter *adapter)
{

	MOD_DEC_USE_COUNT;
}

u32 piix4_func(struct i2c_adapter *adapter)
{
  return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE | 
         I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA | 
         I2C_FUNC_SMBUS_BLOCK_DATA;
}

int __init i2c_piix4_init(void)
{
  int res;
  printk("piix4.o version %s (%s)\n",LM_VERSION,LM_DATE);
#ifdef DEBUG
/* PE- It might be good to make this a permanent part of the code! */
  if (piix4_initialized) {
    printk("i2c-piix4.o: Oops, piix4_init called a second time!\n");
    return -EBUSY;
  }
#endif
  piix4_initialized = 0;
  if ((res = piix4_setup())) {
    printk("i2c-piix4.o: PIIX4 not detected, module not inserted.\n");
    piix4_cleanup();
    return res;
  }
  piix4_initialized ++;
  sprintf(piix4_adapter.name,"SMBus PIIX4 adapter at %04x",piix4_smba);
  if ((res = i2c_add_adapter(&piix4_adapter))) {
    printk("i2c-piix4.o: Adapter registration failed, module not inserted.\n");
    piix4_cleanup();
    return res;
  }
  piix4_initialized++;
  printk("i2c-piix4.o: PIIX4 bus detected and initialized\n");
  return 0;
}

int __init piix4_cleanup(void)
{
  int res;
  if (piix4_initialized >= 2)
  {
    if ((res = i2c_del_adapter(&piix4_adapter))) {
      printk("i2c-piix4.o: i2c_del_adapter failed, module not removed\n");
      return res;
    } else
      piix4_initialized--;
  }
  if (piix4_initialized >= 1) {
    release_region(piix4_smba, 8);
    piix4_initialized--;
  }
  return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl> and Philip Edelbrock <phil@netroedge.com>");
MODULE_DESCRIPTION("PIIX4 SMBus driver");

int init_module(void)
{
  return i2c_piix4_init();
}

int cleanup_module(void)
{
  return piix4_cleanup();
}

#endif /* MODULE */

