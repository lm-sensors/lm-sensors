/*
    sis5595.c - Part of lm_sensors, Linux kernel modules
                for hardware monitoring
                
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>,
                        Kyösti Mälkki <kmalkki@cc.hut.fi> 

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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/malloc.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/sysctl.h>
#include <linux/pci.h>
#if LINUX_VERSION_CODE < 0x020136 /* 2.1.54 */
#include <linux/bios32.h>
#endif
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include "version.h"
#include "i2c-isa.h"
#include "sensors.h"
#include "compat.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,53)
#include <linux/init.h>
#else
#define __init
#define __initdata
#endif


/* Addresses to scan.
   Note that we can't determine the ISA address until we have initialized
   our module */
static unsigned short normal_i2c[] = {SENSORS_I2C_END};
static unsigned short normal_i2c_range[] = {SENSORS_I2C_END};
static unsigned int normal_isa[] = {0x0000,SENSORS_ISA_END};
static unsigned int normal_isa_range[] = {SENSORS_ISA_END};

/* Insmod parameters */
SENSORS_INSMOD_1(sis5595);

/*
   SiS southbridge has a LM78-like chip integrated on the same IC.
   This driver is a customized copy of lm78.c
*/

/* Many SIS5595 constants specified below */

/* Length of ISA address segment */
#define SIS5595_EXTENT 8
#define SIS5595_BASE_REG 0x68

/* Where are the ISA address/data registers relative to the base address */
#define SIS5595_ADDR_REG_OFFSET 5
#define SIS5595_DATA_REG_OFFSET 6

/* The SIS5595 registers */
#define SIS5595_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define SIS5595_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define SIS5595_REG_IN(nr) (0x20 + (nr))

#define SIS5595_REG_FAN_MIN(nr) (0x3a + (nr))
#define SIS5595_REG_FAN(nr) (0x27 + (nr))

#define SIS5595_REG_TEMP 0x27
#define SIS5595_REG_TEMP_OVER 0x39
#define SIS5595_REG_TEMP_HYST 0x3a

#define SIS5595_REG_ALARM1 0x41

#define SIS5595_REG_FANDIV 0x47

#define SIS5595_REG_CONFIG 0x40

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

#define IN_TO_REG(val)  (SENSORS_LIMIT((((val) * 10 + 8)/16),0,255))
#define IN_FROM_REG(val) (((val) *  16) / 10)

extern inline u8 FAN_TO_REG(long rpm, int div)
{
  if (rpm == 0)
    return 255;
  rpm = SENSORS_LIMIT(rpm,1,1000000);
  return SENSORS_LIMIT((1350000 + rpm*div/2) / (rpm*div),1,254);
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

#define TEMP_TO_REG(val) (SENSORS_LIMIT(((val)<0?(((val)-5)/10):\
                                                 ((val)+5)/10),0,255))
#define TEMP_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*10)

#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)

/* Initial limits. To keep them sane, we use the 'standard' translation as
   specified in the SIS5595 sheet. Use the config file to set better limits. */
#define SIS5595_INIT_IN_0 (((1200)  * 10)/38)
#define SIS5595_INIT_IN_1 (((500)   * 100)/168)
#define SIS5595_INIT_IN_2 330
#define SIS5595_INIT_IN_3 250

#define SIS5595_INIT_IN_PERCENTAGE 10

#define SIS5595_INIT_IN_MIN_0 \
        (SIS5595_INIT_IN_0 - SIS5595_INIT_IN_0 * SIS5595_INIT_IN_PERCENTAGE / 100) 
#define SIS5595_INIT_IN_MAX_0 \
        (SIS5595_INIT_IN_0 + SIS5595_INIT_IN_0 * SIS5595_INIT_IN_PERCENTAGE / 100) 
#define SIS5595_INIT_IN_MIN_1 \
        (SIS5595_INIT_IN_1 - SIS5595_INIT_IN_1 * SIS5595_INIT_IN_PERCENTAGE / 100) 
#define SIS5595_INIT_IN_MAX_1 \
        (SIS5595_INIT_IN_1 + SIS5595_INIT_IN_1 * SIS5595_INIT_IN_PERCENTAGE / 100) 
#define SIS5595_INIT_IN_MIN_2 \
        (SIS5595_INIT_IN_2 - SIS5595_INIT_IN_2 * SIS5595_INIT_IN_PERCENTAGE / 100) 
#define SIS5595_INIT_IN_MAX_2 \
        (SIS5595_INIT_IN_2 + SIS5595_INIT_IN_2 * SIS5595_INIT_IN_PERCENTAGE / 100) 
#define SIS5595_INIT_IN_MIN_3 \
        (SIS5595_INIT_IN_3 - SIS5595_INIT_IN_3 * SIS5595_INIT_IN_PERCENTAGE / 100) 
#define SIS5595_INIT_IN_MAX_3 \
        (SIS5595_INIT_IN_3 + SIS5595_INIT_IN_3 * SIS5595_INIT_IN_PERCENTAGE / 100) 

#define SIS5595_INIT_FAN_MIN_1 3000
#define SIS5595_INIT_FAN_MIN_2 3000

#define SIS5595_INIT_TEMP_OVER 600
#define SIS5595_INIT_TEMP_HYST 500

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* This module may seem overly long and complicated. In fact, it is not so
   bad. Quite a lot of bookkeeping is done. A real driver can often cut
   some corners. */

/* For each registered SIS5595, we need to keep some data in memory. That
   data is pointed to by sis5595_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new sis5595 client is
   allocated. */
struct sis5595_data {
         struct semaphore lock;
         int sysctl_id;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 in[4];                   /* Register value */
         u8 in_max[4];               /* Register value */
         u8 in_min[4];               /* Register value */
         u8 fan[2];                  /* Register value */
         u8 fan_min[2];              /* Register value */
         u8 temp;                    /* Register value */
         u8 temp_over;               /* Register value */
         u8 temp_hyst;               /* Register value */
         u8 fan_div[2];              /* Register encoding, shifted right */
         u8 alarms;                 /* Register encoding, combined */
};


#ifdef MODULE
static
#else
extern
#endif
       int __init sensors_sis5595_init(void);
static int __init sis5595_cleanup(void);

static int sis5595_attach_adapter(struct i2c_adapter *adapter);
static int sis5595_detect(struct i2c_adapter *adapter, int address, 
                          unsigned short flags, int kind);
static int sis5595_detach_client(struct i2c_client *client);
static int sis5595_command(struct i2c_client *client, unsigned int cmd, 
                        void *arg);
static void sis5595_inc_use (struct i2c_client *client);
static void sis5595_dec_use (struct i2c_client *client);

static int sis5595_read_value(struct i2c_client *client, u8 register);
static int sis5595_write_value(struct i2c_client *client, u8 register, u8 value);
static void sis5595_update_client(struct i2c_client *client);
static void sis5595_init_client(struct i2c_client *client);
static int sis5595_find_sis(int *address);


static void sis5595_in(struct i2c_client *client, int operation, int ctl_name,
                    int *nrels_mag, long *results);
static void sis5595_fan(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void sis5595_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void sis5595_alarms(struct i2c_client *client, int operation, int ctl_name,
                        int *nrels_mag, long *results);
static void sis5595_fan_div(struct i2c_client *client, int operation, int ctl_name,
                         int *nrels_mag, long *results);

static int sis5595_id = 0;

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver sis5595_driver = {
  /* name */		"SiS 5595",
  /* id */		I2C_DRIVERID_SIS5595,
  /* flags */		I2C_DF_NOTIFY,
  /* attach_adapter */  &sis5595_attach_adapter,
  /* detach_client */	&sis5595_detach_client,
  /* command */		&sis5595_command,
  /* inc_use */		&sis5595_inc_use,
  /* dec_use */		&sis5595_dec_use
};

/* Used by sis5595_init/cleanup */
static int __initdata sis5595_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected SIS5595. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table sis5595_dir_table_template[] = {
  { SIS5595_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_in },
  { SIS5595_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_in },
  { SIS5595_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_in },
  { SIS5595_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_in },
  { SIS5595_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_fan },
  { SIS5595_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_fan },
  { SIS5595_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_temp },
  { SIS5595_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_fan_div },
  { SIS5595_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &sis5595_alarms },
  { 0 }
};

/* This is called when the module is loaded */
int sis5595_attach_adapter(struct i2c_adapter *adapter)
{
  return sensors_detect(adapter,&addr_data,sis5595_detect);
}

/* Locate SiS bridge and correct base address for SIS5595 */
int sis5595_find_sis(int *address)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,54))
  struct pci_dev *s_bridge;
#else
  unsigned char SIS_bus, SIS_devfn;
#endif
  u16 val;

  if (! pci_present())
    return -ENODEV;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,54))
  if (! (s_bridge = pci_find_device(
                   PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_503, NULL)))
		
#else
  if(pcibios_find_device(
                PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_503, 0,
                &SIS_bus, &SIS_devfn))
#endif
    return -ENODEV;


  if ( PCIBIOS_SUCCESSFUL !=
	pci_read_config_word_united(s_bridge, SIS_bus, SIS_devfn, 
                                    SIS5595_BASE_REG, &val))
    return -ENODEV;
		
  *address = (val & 0xfff8);
  return 0;
}

int sis5595_detect(struct i2c_adapter *adapter, int address, 
                   unsigned short flags, int kind)
{
  int i;
  struct i2c_client *new_client;
  struct sis5595_data *data;
  int err=0;
  const char *type_name = "";
  const char *client_name = "";

  /* Make sure we are probing the ISA bus!!  */
  if (!i2c_is_isa_adapter(adapter)) {
    printk("sis5595.o: sis5595_detect called for an I2C bus adapter?!?\n");
    return 0;
  }

  if (check_region(address,SIS5595_EXTENT))
    goto ERROR0;

  /* If this is the address as indicated by the SIS5595 chipset, we don't
     do any futher probing */
  if ((kind < 0) && (address == normal_isa[0]))
    kind = 0;

  /* Probe whether there is anything available on this address. */
  if (kind < 0) {
#define REALLY_SLOW_IO
    /* We need the timeouts for at least some LM78-like chips. But only
       if we read 'undefined' registers. */
    i = inb_p(address + 1);
    if (inb_p(address + 2) != i)
      goto ERROR0;
    if (inb_p(address + 3) != i)
      goto ERROR0;
    if (inb_p(address + 7) != i)
      goto ERROR0;
#undef REALLY_SLOW_IO

    /* Let's just hope nothing breaks here */
    i = inb_p(address + 5) & 0x7f;
    outb_p(~i & 0x7f,address+5);
    if ((inb_p(address + 5) & 0x7f) != (~i & 0x7f)) {
      outb_p(i,address+5);
      return 0;
    }
  }

  /* OK. For now, we presume we have a valid client. We now create the
     client structure, even though we cannot fill it completely yet.
     But it allows us to access sis5595_{read,write}_value. */

  if (! (new_client = kmalloc(sizeof(struct i2c_client) +
                              sizeof(struct sis5595_data),
                              GFP_KERNEL))) {
    err = -ENOMEM;
    goto ERROR0;
  }

  data = (struct sis5595_data *) (new_client + 1);
  new_client->addr = address;
  init_MUTEX(&data->lock);
  new_client->data = data;
  new_client->adapter = adapter;
  new_client->driver = &sis5595_driver;
  new_client->flags = 0;

  /* Now, we do the remaining detection. */

  if (kind < 0) {
    if (sis5595_read_value(new_client,SIS5595_REG_CONFIG) & 0x80)
      goto ERROR1;
  }

  /* Determine the chip type. */
  if (kind <= 0) 
    kind = sis5595;

  if (kind == sis5595) {
    type_name = "sis5595";
    client_name = "SIS5595 chip";
  } else {
#ifdef DEBUG
    printk("sis5595.o: Internal error: unknown kind (%d)?!?",kind);
#endif
    goto ERROR1;
  }

  /* Reserve the ISA region */
  request_region(address, SIS5595_EXTENT, type_name);

  /* Fill in the remaining client fields and put it into the global list */
  strcpy(new_client->name,client_name);

  new_client->id = sis5595_id++;
  data->valid = 0;
  init_MUTEX(&data->update_lock);

  /* Tell the I2C layer a new client has arrived */
  if ((err = i2c_attach_client(new_client)))
    goto ERROR3;

  /* Register a new directory entry with module sensors */
  if ((i = sensors_register_entry((struct i2c_client *) new_client,
                                  type_name,
                                  sis5595_dir_table_template,
				  THIS_MODULE)) < 0) {
    err = i;
    goto ERROR4;
  }
  data->sysctl_id = i;

  /* Initialize the SIS5595 chip */
  sis5595_init_client(new_client);
  return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR4:
  i2c_detach_client(new_client);
ERROR3:
  release_region(address,SIS5595_EXTENT);
ERROR1:
  kfree(new_client);
ERROR0:
  return err;
}

int sis5595_detach_client(struct i2c_client *client)
{
  int err;

  sensors_deregister_entry(((struct sis5595_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("sis5595.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  release_region(client->addr,SIS5595_EXTENT);
  kfree(client);

  return 0;
}

/* No commands defined yet */
int sis5595_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void sis5595_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void sis5595_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}
 

/* The SMBus locks itself, but ISA access must be locked explicitely! 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int sis5595_read_value(struct i2c_client *client, u8 reg)
{
    int res;
    
    down(& (((struct sis5595_data *) (client->data)) -> lock));
    outb_p(reg,client->addr + SIS5595_ADDR_REG_OFFSET);
    res = inb_p(client->addr + SIS5595_DATA_REG_OFFSET);
    up( & (((struct sis5595_data *) (client->data)) -> lock));
    return res;
}

/* The SMBus locks itself, but ISA access muse be locked explicitely! 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int sis5595_write_value(struct i2c_client *client, u8 reg, u8 value)
{
    down(& (((struct sis5595_data *) (client->data)) -> lock));
    outb_p(reg,client->addr + SIS5595_ADDR_REG_OFFSET);
    outb_p(value,client->addr + SIS5595_DATA_REG_OFFSET);
    up( & (((struct sis5595_data *) (client->data)) -> lock));
    return 0;
}

/* Called when we have found a new SIS5595. It should set limits, etc. */
void sis5595_init_client(struct i2c_client *client)
{
  /* Reset all except Watchdog values and last conversion values
     This sets fan-divs to 2, among others */
  sis5595_write_value(client,SIS5595_REG_CONFIG,0x80);

  sis5595_write_value(client,SIS5595_REG_IN_MIN(0),IN_TO_REG(SIS5595_INIT_IN_MIN_0));
  sis5595_write_value(client,SIS5595_REG_IN_MAX(0),IN_TO_REG(SIS5595_INIT_IN_MAX_0));
  sis5595_write_value(client,SIS5595_REG_IN_MIN(1),IN_TO_REG(SIS5595_INIT_IN_MIN_1));
  sis5595_write_value(client,SIS5595_REG_IN_MAX(1),IN_TO_REG(SIS5595_INIT_IN_MAX_1));
  sis5595_write_value(client,SIS5595_REG_IN_MIN(2),IN_TO_REG(SIS5595_INIT_IN_MIN_2));
  sis5595_write_value(client,SIS5595_REG_IN_MAX(2),IN_TO_REG(SIS5595_INIT_IN_MAX_2));
  sis5595_write_value(client,SIS5595_REG_IN_MIN(3),IN_TO_REG(SIS5595_INIT_IN_MIN_3));
  sis5595_write_value(client,SIS5595_REG_IN_MAX(3),IN_TO_REG(SIS5595_INIT_IN_MAX_3));
  sis5595_write_value(client,SIS5595_REG_FAN_MIN(1),
                   FAN_TO_REG(SIS5595_INIT_FAN_MIN_1,2));
  sis5595_write_value(client,SIS5595_REG_FAN_MIN(2),
                   FAN_TO_REG(SIS5595_INIT_FAN_MIN_2,2));
  sis5595_write_value(client,SIS5595_REG_TEMP_OVER,TEMP_TO_REG(SIS5595_INIT_TEMP_OVER));
  sis5595_write_value(client,SIS5595_REG_TEMP_HYST,TEMP_TO_REG(SIS5595_INIT_TEMP_HYST));

  /* Start monitoring */
  sis5595_write_value(client,SIS5595_REG_CONFIG,
                   (sis5595_read_value(client,SIS5595_REG_CONFIG) & 0xf7) | 0x01);
  
}

void sis5595_update_client(struct i2c_client *client)
{
  struct sis5595_data *data = client->data;
  int i;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting sis5595 update\n");
#endif
    for (i = 0; i <= 3; i++) {
      data->in[i]     = sis5595_read_value(client,SIS5595_REG_IN(i));
      data->in_min[i] = sis5595_read_value(client,SIS5595_REG_IN_MIN(i));
      data->in_max[i] = sis5595_read_value(client,SIS5595_REG_IN_MAX(i));
    }
    for (i = 1; i <= 2; i++) {
      data->fan[i-1] = sis5595_read_value(client,SIS5595_REG_FAN(i));
      data->fan_min[i-1] = sis5595_read_value(client,SIS5595_REG_FAN_MIN(i));
    }
    data->temp = sis5595_read_value(client,SIS5595_REG_TEMP);
    data->temp_over = sis5595_read_value(client,SIS5595_REG_TEMP_OVER);
    data->temp_hyst = sis5595_read_value(client,SIS5595_REG_TEMP_HYST);
    i = sis5595_read_value(client,SIS5595_REG_FANDIV);
    data->fan_div[0] = (i >> 4) & 0x03;
    data->fan_div[1] = i >> 6;
    data->alarms = sis5595_read_value(client,SIS5595_REG_ALARM1);
    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}


/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the date
   with) if it is called with operation==SENSORS_PROC_REAL_INFO. It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ. Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */
void sis5595_in(struct i2c_client *client, int operation, int ctl_name, 
             int *nrels_mag, long *results)
{
  struct sis5595_data *data = client->data;
  int nr = ctl_name - SIS5595_SYSCTL_IN0;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    sis5595_update_client(client);
    results[0] = IN_FROM_REG(data->in_min[nr]);
    results[1] = IN_FROM_REG(data->in_max[nr]);
    results[2] = IN_FROM_REG(data->in[nr]);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
      if (*nrels_mag >= 1) {
        data->in_min[nr] = IN_TO_REG(results[0]);
        sis5595_write_value(client,SIS5595_REG_IN_MIN(nr),data->in_min[nr]);
      }
      if (*nrels_mag >= 2) {
        data->in_max[nr] = IN_TO_REG(results[1]);
        sis5595_write_value(client,SIS5595_REG_IN_MAX(nr),data->in_max[nr]);
      }
  }
}

void sis5595_fan(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
  struct sis5595_data *data = client->data;
  int nr = ctl_name - SIS5595_SYSCTL_FAN1 + 1;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    sis5595_update_client(client);
    results[0] = FAN_FROM_REG(data->fan_min[nr-1],
                 DIV_FROM_REG(data->fan_div[nr-1]));
    results[1] = FAN_FROM_REG(data->fan[nr-1],
                 DIV_FROM_REG(data->fan_div[nr-1]));
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->fan_min[nr-1] = FAN_TO_REG(results[0],
                            DIV_FROM_REG(data->fan_div[nr-1]));
      sis5595_write_value(client,SIS5595_REG_FAN_MIN(nr),data->fan_min[nr-1]);
    }
  }
}


void sis5595_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct sis5595_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    sis5595_update_client(client);
    results[0] = TEMP_FROM_REG(data->temp_over);
    results[1] = TEMP_FROM_REG(data->temp_hyst);
    results[2] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_over = TEMP_TO_REG(results[0]);
      sis5595_write_value(client,SIS5595_REG_TEMP_OVER,data->temp_over);
    }
    if (*nrels_mag >= 2) {
      data->temp_hyst = TEMP_TO_REG(results[1]);
      sis5595_write_value(client,SIS5595_REG_TEMP_HYST,data->temp_hyst);
    }
  }
}

void sis5595_alarms(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
  struct sis5595_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    sis5595_update_client(client);
    results[0] = ALARMS_FROM_REG(data->alarms);
    *nrels_mag = 1;
  }
}

void sis5595_fan_div(struct i2c_client *client, int operation, int ctl_name,
                  int *nrels_mag, long *results)
{
  struct sis5595_data *data = client->data;
  int old;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    sis5595_update_client(client);
    results[0] = DIV_FROM_REG(data->fan_div[0]);
    results[1] = DIV_FROM_REG(data->fan_div[1]);
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    old = sis5595_read_value(client,SIS5595_REG_FANDIV);
    if (*nrels_mag >= 2) {
      data->fan_div[1] = DIV_TO_REG(results[1]);
      old = (old & 0x3f) | (data->fan_div[1] << 6);
    }
    if (*nrels_mag >= 1) {
      data->fan_div[0] = DIV_TO_REG(results[0]);
      old = (old & 0xcf) | (data->fan_div[0] << 4);
      sis5595_write_value(client,SIS5595_REG_FANDIV,old);
    }
  }
}

int __init sensors_sis5595_init(void)
{
  int res,addr;

  printk("sis5595.o version %s (%s)\n",LM_VERSION,LM_DATE);
  sis5595_initialized = 0;

  if (sis5595_find_sis(&addr)) {
    normal_isa[0] = SENSORS_ISA_END;
    printk("sis5595.o: Warning: No SIS5595 southbridge found!\n");
  } else
    normal_isa[0] = addr;

  if ((res =i2c_add_driver(&sis5595_driver))) {
    printk("sis5595.o: Driver registration failed, module not inserted.\n");
    sis5595_cleanup();
    return res;
  }
  sis5595_initialized ++;
  return 0;
}

int __init sis5595_cleanup(void)
{
  int res;

  if (sis5595_initialized >= 1) {
    if ((res = i2c_del_driver(&sis5595_driver))) {
      printk("sis5595.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    sis5595_initialized --;
  }
  return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Kyösti Mälkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("SiS 5595 Sensor device");

int init_module(void)
{
  return sensors_sis5595_init();
}

int cleanup_module(void)
{
  return sis5595_cleanup();
}

#endif /* MODULE */

