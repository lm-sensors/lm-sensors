/*
    maxilife.c - Part of lm_sensors, Linux kernel modules for hardware
                 monitoring
    Copyright (c) 1999  Fons Rademakers <Fons.Rademakers@cern.ch> 

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

/* The is the driver for the HP MaxiLife Health monitoring system
   as used in the line of HP Kayak Workstation PC's.
   
   The driver supports the following MaxiLife firmware versions:
   
   0) HP KAYAK XU/XAs (Dual Pentium II Slot 1, Deschutes/Klamath)
   1) HP KAYAK XU (Dual Xeon [Slot 2] 400/450 Mhz)
   2) HP KAYAK XA (Pentium II Slot 1, monoprocessor)
   
   Currently firmware auto detection is not implemented. To use the
   driver load it with the correct option for you Kayak. For example:
   
   insmod maxilife.o maxi_version=0 | 1 | 2
   
   maxi_version=0 is the default
*/

static const char *version = "1.00 25/2/99 Fons Rademakers";


#include <linux/module.h>
#include <linux/malloc.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/sysctl.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include "smbus.h"
#include "version.h"
#include "sensors.h"
#include "i2c.h"
#include "compat.h"


/*#define AUTODETECT          /* try to autodetect MaxiLife version */
#define NOWRITE             /* don't allow writing to MaxiLife registers */


/* The MaxiLife registers */
#define MAXI_REG_TEMP(nr)      (0x60 + (nr))

#define MAXI_REG_FAN(nr)       (0x65 + (nr))
#define MAXI_REG_FAN_MIN(nr)   ((nr)==0 ? 0xb3 : (nr)==1 ? 0xb3 : 0xab)
#define MAXI_REG_FAN_MINAS(nr) ((nr)==0 ? 0xb3 : (nr)==1 ? 0xab : 0xb3)
#define MAXI_REG_FAN_SPEED(nr) ((nr)==0 ? 0xe4 : (nr)==1 ? 0xe5 : 0xe9)

#define MAXI_REG_PLL           0xb9
#define MAXI_REG_PLL_MIN       0xba
#define MAXI_REG_PLL_MAX       0xbb

#define MAXI_REG_VID(nr)       ((nr)==0 ? 0xd1 : (nr)==1 ? 0xd9 : \
                                (nr)==2 ? 0xd4 : 0xc5)
#define MAXI_REG_VID_MIN(nr)   MAXI_REG_VID(nr)+1
#define MAXI_REG_VID_MAX(nr)   MAXI_REG_VID(nr)+2

#define MAXI_REG_DIAG_RT1      0x2c
#define MAXI_REG_DIAG_RT2      0x2d

#define MAXI_REG_BIOS_CTRL     0x2a
#define MAXI_REG_LED_STATE     0x96

/* Conversions */
                               /* 0xfe: fan off, 0xff: stopped (alarm) */
                               /* 19531 / val * 60 == 1171860 / val */
#define FAN_FROM_REG(val)      ((val)==0xfe ? -1 : (val)==0xff ? 0 : \
                                (1171860 / (val)))
#define FAN_TO_REG(val)        (1171860 / (val))

#define TEMP_FROM_REG(val)     ((val) * 5)
#define TEMP_TO_REG(val)       ((val) / 5)
#define PLL_FROM_REG(val)      (((val) * 1000) / 32)
#define PLL_TO_REG(val)        (((val) * 32) / 1000)
#define VID_FROM_REG(val)      ((val) ? (((val) * 27390) / 256) + 3208 : 0)
#define VID_TO_REG(val)        ((((val) - 3208) * 256) / 27390)
#define ALARMS_FROM_REG(val)   (val)



#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* The following product codenames apply:
     Cristal/Geronimo: HP KAYAK XU/XAs
                       (Dual Pentium II Slot 1, Deschutes/Klamath)
     Cognac: HP KAYAK XU (Dual Xeon [Slot 2] 400/450 Mhz)
     Ashaki: HP KAYAK XA (Pentium II Slot 1, monoprocessor) */

enum maxi_type { cristal, cognac, ashaki };

/* For each registered MaxiLife controller, we need to keep some data in
   memory. That data is pointed to by maxi_list[NR]->data. The structure
   itself is dynamically allocated, at the same time when a new MaxiLife
   client is allocated. We assume MaxiLife will only be present on the
   SMBus and not on the ISA bus. */
struct maxi_data {
   struct semaphore lock;
   int              sysctl_id;
   enum maxi_type   type;

   struct semaphore update_lock;
   char             valid;         /* !=0 if following fields are valid */
   unsigned long    last_updated;  /* In jiffies */

   u8  fan[3];                  /* Register value */
   u8  fan_min[3];              /* Register value */
   u8  fan_speed[3];            /* Register value */
   u8  fan_div[3];              /* Static value */
   u8  temp[5];                 /* Register value */
   u8  temp_max[5];             /* Static value */
   u8  temp_hyst[5];            /* Static value */
   u8  pll;                     /* Register value */
   u8  pll_min;                 /* Register value */
   u8  pll_max;                 /* register value */
   u8  vid[4];                  /* Register value */
   u8  vid_min[4];              /* Register value */
   u8  vid_max[4];              /* Register value */
   u16 alarms;                  /* Register encoding, combined */
};


static int  maxi_init(void);
static int  maxi_cleanup(void);

static int  maxi_attach_adapter(struct i2c_adapter *adapter);
static int  maxi_detect_smbus(struct i2c_adapter *adapter);
static int  maxi_detach_client(struct i2c_client *client);
static int  maxi_detach_smbus(struct i2c_client *client);
static int  maxi_new_client(struct i2c_adapter *adapter,
                           struct i2c_client *new_client);
static void maxi_remove_client(struct i2c_client *client);
static int  maxi_command(struct i2c_client *client, unsigned int cmd, 
                        void *arg);
static void maxi_inc_use (struct i2c_client *client);
static void maxi_dec_use (struct i2c_client *client);

static int  maxi_read_value(struct i2c_client *client, u8 register);
static int  maxi_write_value(struct i2c_client *client, u8 register, u8 value);
static void maxi_update_client(struct i2c_client *client);
static void maxi_init_client(struct i2c_client *client);

static void maxi_fan(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void maxi_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void maxi_pll(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void maxi_vid(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void maxi_alarms(struct i2c_client *client, int operation, int ctl_name,
                        int *nrels_mag, long *results);


/* I choose here for semi-static MaxiLife allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_MAXI_NR 4
static struct i2c_client *maxi_list[MAX_MAXI_NR];

/* The driver. I choose to use type i2c_driver, as at is identical to
   the smbus_driver. */
static struct i2c_driver maxi_driver = {
  /* name */		"HP MaxiLife driver",
  /* id */		I2C_DRIVERID_MAXILIFE,
  /* flags */		DF_NOTIFY,
  /* attach_adapter */  &maxi_attach_adapter,
  /* detach_client */	&maxi_detach_client,
  /* command */		&maxi_command,
  /* inc_use */		&maxi_inc_use,
  /* dec_use */		&maxi_dec_use
};

/* Used by maxi_init/cleanup */
static int maxi_initialized = 0;

/* Default firmware version. Use module option "maxi_version"
   to set desired version. Auto detect is not yet working */
static int maxi_version = cristal;

/* The /proc/sys entries */
/* These files are created for each detected MaxiLife processor.
   This is just a template; though at first sight, you might think we
   could use a statically allocated list, we need some way to get back
   to the parent - which is done through one of the 'extra' fields 
   which are initialized when a new copy is allocated. */
static ctl_table maxi_dir_table_template[] = {
   { MAXI_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_fan },
   { MAXI_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_fan },
   { MAXI_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_fan },
   { MAXI_SYSCTL_TEMP1, "temp1", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_temp },
   { MAXI_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_temp },
   { MAXI_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_temp },
   { MAXI_SYSCTL_TEMP4, "temp4", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_temp },
   { MAXI_SYSCTL_TEMP5, "temp5", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_temp },
   { MAXI_SYSCTL_PLL, "pll", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_pll },
   { MAXI_SYSCTL_VID1, "vid1", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_vid },
   { MAXI_SYSCTL_VID2, "vid2", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_vid },
   { MAXI_SYSCTL_VID3, "vid3", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_vid },
   { MAXI_SYSCTL_VID4, "vid4", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_vid },
   { MAXI_SYSCTL_ALARMS, "alarms", NULL, 0, 0644, NULL, &sensors_proc_real,
     &sensors_sysctl_real, NULL, &maxi_alarms },
   { 0 }
};

/* This function is called when:
    - maxi_driver is inserted (when this module is loaded), for each
      available adapter
    - when a new adapter is inserted (and maxi_driver is still present) */
int maxi_attach_adapter(struct i2c_adapter *adapter)
{
   return maxi_detect_smbus(adapter);
}

/* This function is called whenever a client should be removed:
    - maxi_driver is removed (when this module is unloaded)
    - when an adapter is removed which has a maxi client (and maxi_driver
      is still present). */
int maxi_detach_client(struct i2c_client *client)
{
   return maxi_detach_smbus(client);
}

int maxi_detect_smbus(struct i2c_adapter *adapter)
{
   u8  biosctl;
   int address, err;
   struct i2c_client *new_client;
   enum maxi_type type;
   const char *type_name, *client_name;

   /* OK, this is no detection. I know. It will do for now, though. */
   err = 0;
   for (address = 0x10; (! err) && (address <= 0x14); address++) {

      /* Later on, we will keep a list of registered addresses for each
         adapter, and check whether they are used here */

      if (smbus_read_byte_data(adapter, address, MAXI_REG_LED_STATE) < 0) 
         continue;

#ifdef AUTODETECT
      /* The right way to get the platform info is to read the firmware
         revision from serial EEPROM (addr=0x54), at offset 0x0045.
         This is a string as:
           "CG 00.04" -> Cristal [XU] / Geronimo [XAs]
           "CO 00.03" -> Cognac [XU]
           "AS 00.01" -> Ashaki [XA] */
      biosctl = smbus_read_byte_data(adapter, address, MAXI_REG_BIOS_CTRL);
      smbus_write_byte_data(adapter, address, MAXI_REG_BIOS_CTRL, biosctl|4);
      err = eeprom_read_byte_data(adapter, 0x54, 0x45);
      smbus_write_byte_data(adapter, address, MAXI_REG_BIOS_CTRL, biosctl);
#endif

      if (maxi_version == cristal) {
         type = cristal;
         type_name = "maxilife-cg";
         client_name = "HP MaxiLife Rev CG 00.04";
         printk("maxilife: HP KAYAK XU/XAs (Dual Pentium II Slot 1)\n");
      } else if (maxi_version == cognac) {
         type = cognac;
         type_name = "maxilife-co";
         client_name = "HP MaxiLife Rev CO 00.03";
         printk("maxilife: HP KAYAK XU (Dual Xeon Slot 2 400/450 Mhz)\n");
      } else if (maxi_version == ashaki) {
         type = ashaki;
         type_name = "maxilife-as";
         client_name = "HP MaxiLife Rev AS 00.01";
         printk("maxilife: HP KAYAK XA (Pentium II Slot 1, monoprocessor)\n");
      } else {
#if AUTODETECT
         printk("maxilife: Warning: probed non-maxilife chip?!? (%x)\n", err);
#else
         printk("maxilife: Error: specified wrong maxi_version (%d)\n",
                maxi_version);
#endif
         continue;
      }

      /* Allocate space for a new client structure. To counter memory
         fragmentation somewhat, we only do one kmalloc. */
      if (! (new_client = kmalloc(sizeof(struct i2c_client) + 
                                  sizeof(struct maxi_data),
                                  GFP_KERNEL))) {
         err = -ENOMEM;
         continue;
      }

      /* Fill the new client structure with data */
      new_client->data = (struct maxi_data *) (new_client + 1);
      new_client->addr = address;
      strcpy(new_client->name, client_name);
      ((struct maxi_data *) (new_client->data))->type = type;
      if ((err = maxi_new_client(adapter, new_client)))
         goto ERROR2;

      /* Tell i2c-core a new client has arrived */
      if ((err = i2c_attach_client(new_client))) 
         goto ERROR3;

      /* Register a new directory entry with module sensors */
      if ((err = sensors_register_entry(new_client, type_name,
                                        maxi_dir_table_template)) < 0)
         goto ERROR4;
      ((struct maxi_data *) (new_client->data))->sysctl_id = err;
      err = 0;

      /* Initialize the MaxiLife chip */
      maxi_init_client(new_client);
      continue;

      /* OK, this is not exactly good programming practice, usually.
         But it is very code-efficient in this case. */
ERROR4:
      i2c_detach_client(new_client);
ERROR3:
      maxi_remove_client((struct i2c_client *) new_client);
ERROR2:
      kfree(new_client);
   }
   return err;
}

int maxi_detach_smbus(struct i2c_client *client)
{
   int err,i;
   for (i = 0; i < MAX_MAXI_NR; i++)
      if (client == maxi_list[i])
         break;
   if ((i == MAX_MAXI_NR)) {
      printk("maxilife: Client to detach not found.\n");
      return -ENOENT;
   }

   sensors_deregister_entry(((struct maxi_data *)(client->data))->sysctl_id);

   if ((err = i2c_detach_client(client))) {
      printk("maxilife: Client deregistration failed, client not detached.\n");
      return err;
   }
   maxi_remove_client(client);
   kfree(client);
   return 0;
}

/* Find a free slot, and initialize most of the fields */
int maxi_new_client(struct i2c_adapter *adapter,
                    struct i2c_client *new_client)
{
   int i;
   struct maxi_data *data;

   /* First, seek out an empty slot */
   for (i = 0; i < MAX_MAXI_NR; i++)
      if (!maxi_list[i])
         break;
   if (i == MAX_MAXI_NR) {
      printk("maxilife: No empty slots left, recompile and heighten "
             "MAX_MAXI_NR!\n");
      return -ENOMEM;
   }
  
   maxi_list[i] = new_client;
   new_client->id = i;
   new_client->adapter = adapter;
   new_client->driver = &maxi_driver;
   data = new_client->data;
   data->valid = 0;
   data->lock = MUTEX;
   data->update_lock = MUTEX;
   return 0;
}

/* Inverse of maxi_new_client */
void maxi_remove_client(struct i2c_client *client)
{
   int i;
   for (i = 0; i < MAX_MAXI_NR; i++)
      if (client == maxi_list[i]) 
         maxi_list[i] = NULL;
}

/* No commands defined yet */
int maxi_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
   return 0;
}

/* Nothing here yet */
void maxi_inc_use (struct i2c_client *client)
{
#ifdef MODULE
   MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void maxi_dec_use (struct i2c_client *client)
{
#ifdef MODULE
   MOD_DEC_USE_COUNT;
#endif
}


/* Read byte from specified register. */
int maxi_read_value(struct i2c_client *client, u8 reg)
{
   return smbus_read_byte_data(client->adapter, client->addr, reg);
}

/* Write byte to specified register. */ 
int maxi_write_value(struct i2c_client *client, u8 reg, u8 value)
{
   return smbus_write_byte_data(client->adapter, client->addr, reg, value);
}

/* Called when we have found a new MaxiLife. It should set limits, etc. */
void maxi_init_client(struct i2c_client *client)
{
   /* start with default settings */  
}

void maxi_update_client(struct i2c_client *client)
{
   struct maxi_data *data = client->data;
   int i;

   down(&data->update_lock);

   if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
       (jiffies < data->last_updated) || !data->valid) {

#ifdef DEBUG
      printk("maxilife: Starting MaxiLife update\n");
#endif
      for (i = 0; i < 5; i++)
         data->temp[i] = maxi_read_value(client, MAXI_REG_TEMP(i));
      switch (data->type) {
         case cristal:
            data->temp[0]      = 0;   /* not valid */
            data->temp_max[0]  = 0;
            data->temp_hyst[0] = 0;
            data->temp_max[1]  = 110; /* max PCI slot temp */
            data->temp_hyst[1] = 100;
            data->temp_max[2]  = 120; /* max BX chipset temp */
            data->temp_hyst[2] = 110;
            data->temp_max[3]  = 100; /* max HDD temp */
            data->temp_hyst[3] = 90;
            data->temp_max[4]  = 120; /* max CPU temp */
            data->temp_hyst[4] = 110;
            break;
            
         case cognac:
            data->temp_max[0]  = 120; /* max CPU1 temp */
            data->temp_hyst[0] = 110;
            data->temp_max[1]  = 110; /* max PCI slot temp */
            data->temp_hyst[1] = 100;
            data->temp_max[2]  = 120; /* max CPU2 temp */
            data->temp_hyst[2] = 110;
            data->temp_max[3]  = 100; /* max HDD temp */
            data->temp_hyst[3] = 90;
            data->temp_max[4]  = 120; /* max reference CPU temp */
            data->temp_hyst[4] = 110;
            break;
            
         case ashaki:
            data->temp[0]      = 0;   /* not valid */
            data->temp_max[0]  = 0;
            data->temp_hyst[0] = 0;
            data->temp_max[1]  = 110; /* max PCI slot temp */
            data->temp_hyst[1] = 100;
            data->temp[2]      = 0;   /* not valid */
            data->temp_max[2]  = 0;
            data->temp_hyst[2] = 0;
            data->temp_max[3]  = 100; /* max HDD temp */
            data->temp_hyst[3] = 90;
            data->temp_max[4]  = 120; /* max CPU temp */
            data->temp_hyst[4] = 110;
            break;
            
         default:
            printk("maxilife: Unknown MaxiLife chip\n");
      }

      for (i = 0; i < 3; i++) {      
         data->fan[i]       = maxi_read_value(client, MAXI_REG_FAN(i));
         data->fan_speed[i] = maxi_read_value(client, MAXI_REG_FAN_SPEED(i));
         data->fan_div[i]   = 4;
         if (data->type == ashaki)
            data->fan_min[i] = maxi_read_value(client, MAXI_REG_FAN_MINAS(i));
         else
            data->fan_min[i] = maxi_read_value(client, MAXI_REG_FAN_MIN(i));
      }
      
      data->pll     = maxi_read_value(client, MAXI_REG_PLL);
      data->pll_min = maxi_read_value(client, MAXI_REG_PLL_MIN);
      data->pll_max = maxi_read_value(client, MAXI_REG_PLL_MAX);

      for (i = 0; i < 4; i++) {
         data->vid[i]     = maxi_read_value(client, MAXI_REG_VID(i));
         data->vid_min[i] = maxi_read_value(client, MAXI_REG_VID_MIN(i));
         data->vid_max[i] = maxi_read_value(client, MAXI_REG_VID_MAX(i));
      }
      switch (data->type) {
         case cristal:
            data->vid[3]     = 0;   /* no voltage cache L2 */
            data->vid_min[3] = 0;
            data->vid_max[3] = 0;
            break;

         case cognac:
            break;

         case ashaki:
            data->vid[1]     = 0;   /* no voltage CPU 2 */
            data->vid_min[1] = 0;
            data->vid_max[1] = 0;
            data->vid[3]     = 0;   /* no voltage cache L2 */
            data->vid_min[3] = 0;
            data->vid_max[3] = 0;
            break;

         default:
            printk("maxilife: Unknown MaxiLife chip\n");
      }

      data->alarms = maxi_read_value(client, MAXI_REG_DIAG_RT1) +
                    (maxi_read_value(client, MAXI_REG_DIAG_RT2) << 8);
      data->last_updated = jiffies;
      data->valid = 1;
   }

   up(&data->update_lock);
}

/* The next few functions are the call-back functions of the /proc/sys and
   sysctl files. Which function is used is defined in the ctl_table in
   the extra1 field.
   Each function must return the magnitude (power of 10 to divide the data
   with) if it is called with operation==SENSORS_PROC_REAL_INFO. It must
   put a maximum of *nrels elements in results reflecting the data of this
   file, and set *nrels to the number it actually put in it, if operation==
   SENSORS_PROC_REAL_READ. Finally, it must get upto *nrels elements from
   results and write them to the chip, if operations==SENSORS_PROC_REAL_WRITE.
   Note that on SENSORS_PROC_REAL_READ, I do not check whether results is
   large enough (by checking the incoming value of *nrels). This is not very
   good practice, but as long as you put less than about 5 values in results,
   you can assume it is large enough. */
void maxi_fan(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
   struct maxi_data *data = client->data;
   int nr = ctl_name - MAXI_SYSCTL_FAN1 + 1;

   if (operation == SENSORS_PROC_REAL_INFO)
      *nrels_mag = 0;
   else if (operation == SENSORS_PROC_REAL_READ) {
      maxi_update_client(client);
      results[0] = FAN_FROM_REG(data->fan_min[nr-1]);
      results[1] = data->fan_div[nr-1];
      results[2] = FAN_FROM_REG(data->fan[nr-1]);
      *nrels_mag = 3;
   } else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
      if (*nrels_mag >= 1) {
         data->fan_min[nr-1] = FAN_TO_REG(results[0]);
         maxi_write_value(client, MAXI_REG_FAN_MIN(nr), data->fan_min[nr-1]);
      }
#endif
   }
}

void maxi_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
   struct maxi_data *data = client->data;
   int nr = ctl_name - MAXI_SYSCTL_TEMP1 + 1;

   if (operation == SENSORS_PROC_REAL_INFO)
      *nrels_mag = 1;
   else if (operation == SENSORS_PROC_REAL_READ) {
      maxi_update_client(client);
      results[0] = TEMP_FROM_REG(data->temp_max[nr-1]);
      results[1] = TEMP_FROM_REG(data->temp_hyst[nr-1]);
      results[2] = TEMP_FROM_REG(data->temp[nr-1]);
      *nrels_mag = 3;
   } else if (operation == SENSORS_PROC_REAL_WRITE) {
      /* temperature range can not be changed */
   }
}

void maxi_pll(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
   struct maxi_data *data = client->data;
  
   if (operation == SENSORS_PROC_REAL_INFO)
      *nrels_mag = 2;
   else if (operation == SENSORS_PROC_REAL_READ) {
      maxi_update_client(client);
      results[0] = PLL_FROM_REG(data->pll_min);
      results[1] = PLL_FROM_REG(data->pll_max);
      results[2] = PLL_FROM_REG(data->pll);
      *nrels_mag = 3;
   } else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
      if (*nrels_mag >= 1) {
         data->pll_min = PLL_TO_REG(results[0]);
         maxi_write_value(client, MAXI_REG_PLL_MIN, data->pll_min);
      }
      if (*nrels_mag >= 2) {
         data->pll_max = PLL_TO_REG(results[1]);
         maxi_write_value(client, MAXI_REG_PLL_MAX, data->pll_max);
      }
#endif
   }
}

void maxi_vid(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
   struct maxi_data *data = client->data;
   int nr = ctl_name - MAXI_SYSCTL_VID1 + 1;

   if (operation == SENSORS_PROC_REAL_INFO)
      *nrels_mag = 4;
   else if (operation == SENSORS_PROC_REAL_READ) {
      maxi_update_client(client);
      results[0] = VID_FROM_REG(data->vid_min[nr-1]);
      results[1] = VID_FROM_REG(data->vid_max[nr-1]);
      results[2] = VID_FROM_REG(data->vid[nr-1]);
      *nrels_mag = 3;
   } else if (operation == SENSORS_PROC_REAL_WRITE) {
#ifndef NOWRITE
      if (*nrels_mag >= 1) {
         data->vid_min[nr-1] = VID_TO_REG(results[0]);
         maxi_write_value(client, MAXI_REG_VID_MIN(nr), data->vid_min[nr-1]);
      }
      if (*nrels_mag >= 2) {
         data->vid_max[nr-1] = VID_TO_REG(results[1]);
         maxi_write_value(client, MAXI_REG_VID_MAX(nr), data->vid_max[nr-1]);
      }
#endif
   }
}

void maxi_alarms(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
   struct maxi_data *data = client->data;
  
   if (operation == SENSORS_PROC_REAL_INFO)
      *nrels_mag = 0;
   else if (operation == SENSORS_PROC_REAL_READ) {
      maxi_update_client(client);
      results[0] = ALARMS_FROM_REG(data->alarms);
      *nrels_mag = 1;
   }
}

int maxi_init(void)
{
   int res;

   printk("maxilife: Version %s (lm_sensors %s (%s))\n", version,
          LM_VERSION, LM_DATE);
   maxi_initialized = 0;

   if ((res = i2c_add_driver(&maxi_driver))) {
      printk("maxilife: Driver registration failed, module not inserted.\n");
      maxi_cleanup();
      return res;
   }
   maxi_initialized++;
   return 0;
}

int maxi_cleanup(void)
{
   int res;

   if (maxi_initialized >= 1) {
      if ((res = i2c_del_driver(&maxi_driver))) {
         printk("maxilife: Driver deregistration failed, module not removed.\n");
         return res;
      }
      maxi_initialized--;
   }
   return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Fons Rademakers <Fons.Rademakers@cern.ch>");
MODULE_DESCRIPTION("HP MaxiLife driver");
MODULE_PARM(maxi_version, "i");
MODULE_PARM_DESC(maxi_version, "MaxiLife firmware version");

int init_module(void)
{
   return maxi_init();
}

int cleanup_module(void)
{
   return maxi_cleanup();
}

#endif /* MODULE */


