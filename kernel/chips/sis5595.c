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
#include "smbus.h"
#include "version.h"
#include "i2c-isa.h"
#include "sensors.h"
#include "i2c.h"
#include "compat.h"

/*
   SiS southbridge has lm78 integrated on the same IC,
   This drives is a customized copy of lm78.c
*/

/* Many LM78 constants specified below */

/* Length of ISA address segment */
#define LM78_EXTENT 8
#define LM78_BASE_REG 0x68

/* Where are the ISA address/data registers relative to the base address */
#define LM78_ADDR_REG_OFFSET 5
#define LM78_DATA_REG_OFFSET 6

/* The LM78 registers */
#define LM78_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define LM78_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define LM78_REG_IN(nr) (0x20 + (nr))

#define LM78_REG_FAN_MIN(nr) (0x3a + (nr))
#define LM78_REG_FAN(nr) (0x27 + (nr))

#define LM78_REG_TEMP 0x27
#define LM78_REG_TEMP_OVER 0x39
#define LM78_REG_TEMP_HYST 0x3a

#define LM78_REG_ALARM1 0x41
/* Not available in SiS 5595
#define LM78_REG_ALARM2 0x42
*/

#define LM78_REG_VID_FANDIV 0x47

#define LM78_REG_CONFIG 0x40

/* Conversions. Rounding is only done on the TO_REG variants. */
#define IN_TO_REG(val)  (((val) * 10 + 8)/16)
#define IN_FROM_REG(val) (((val) *  16) / 10)

static inline unsigned char
FAN_TO_REG (unsigned rpm, unsigned divisor)
{
  unsigned val;
  
  if (rpm == 0)
      return 255;

  val = (1350000 + rpm * divisor / 2) / (rpm * divisor);
  if (val > 255)
      val = 255;
  return val;
}
#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

#define TEMP_TO_REG(val) (((val)<0?(((val)-5)/10)&0xff:((val)+5)/10) & 0xff)
#define TEMP_FROM_REG(val) (((val)>0x80?(val)-0x100:(val))*10)

#define VID_FROM_REG(val) ((val)==0x1f?0:(val)>=0x10?510-(val)*10:\
                           (val)>=0x06?0:205-(val)*5)
#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)

/* Initial limits. To keep them sane, we use the 'standard' translation as
   specified in the LM78 sheet. Use the config file to set better limits. */
#define LM78_INIT_IN_0 (vid==350?280:vid)
#define LM78_INIT_IN_1 (vid==350?280:vid)
#define LM78_INIT_IN_2 330
#define LM78_INIT_IN_3 (((500)   * 100)/168)
#define LM78_INIT_IN_4 (((1200)  * 10)/38)
#define LM78_INIT_IN_5 (((-1200) * -604)/2100)
#define LM78_INIT_IN_6 (((-500)  * -604)/909)

#define LM78_INIT_IN_PERCENTAGE 10

#define LM78_INIT_IN_MIN_0 \
        (LM78_INIT_IN_0 - LM78_INIT_IN_0 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_0 \
        (LM78_INIT_IN_0 + LM78_INIT_IN_0 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_1 \
        (LM78_INIT_IN_1 - LM78_INIT_IN_1 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_1 \
        (LM78_INIT_IN_1 + LM78_INIT_IN_1 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_2 \
        (LM78_INIT_IN_2 - LM78_INIT_IN_2 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_2 \
        (LM78_INIT_IN_2 + LM78_INIT_IN_2 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_3 \
        (LM78_INIT_IN_3 - LM78_INIT_IN_3 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_3 \
        (LM78_INIT_IN_3 + LM78_INIT_IN_3 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_4 \
        (LM78_INIT_IN_4 - LM78_INIT_IN_4 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_4 \
        (LM78_INIT_IN_4 + LM78_INIT_IN_4 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_5 \
        (LM78_INIT_IN_5 - LM78_INIT_IN_5 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_5 \
        (LM78_INIT_IN_5 + LM78_INIT_IN_5 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MIN_6 \
        (LM78_INIT_IN_6 - LM78_INIT_IN_6 * LM78_INIT_IN_PERCENTAGE / 100) 
#define LM78_INIT_IN_MAX_6 \
        (LM78_INIT_IN_6 + LM78_INIT_IN_6 * LM78_INIT_IN_PERCENTAGE / 100) 

#define LM78_INIT_FAN_MIN_1 3000
#define LM78_INIT_FAN_MIN_2 3000
#define LM78_INIT_FAN_MIN_3 3000

#define LM78_INIT_TEMP_OVER 600
#define LM78_INIT_TEMP_HYST 500

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

/* This module may seem overly long and complicated. In fact, it is not so
   bad. Quite a lot of bookkeeping is done. A real driver can often cut
   some corners. */

/* Types of chips supported */
enum lm78_type { lm78 };

/* For each registered LM78, we need to keep some data in memory. That
   data is pointed to by lm78_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new lm78 client is
   allocated. */
struct lm78_data {
         struct semaphore lock;
         int sysctl_id;
         enum lm78_type type;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */

         u8 in[7];                   /* Register value */
         u8 in_max[7];               /* Register value */
         u8 in_min[7];               /* Register value */
         u8 fan[3];                  /* Register value */
         u8 fan_min[3];              /* Register value */
         u8 temp;                    /* Register value */
         u8 temp_over;               /* Register value */
         u8 temp_hyst;               /* Register value */
         u8 fan_div[3];              /* Register encoding, shifted right */
         u8 vid;                     /* Register encoding, combined */
         u16 alarms;                 /* Register encoding, combined */
};


static int lm78_init(void);
static int lm78_cleanup(void);

static int lm78_attach_adapter(struct i2c_adapter *adapter);
static int lm78_detect_isa(struct isa_adapter *adapter);
static int lm78_detach_client(struct i2c_client *client);
static int lm78_detach_isa(struct isa_client *client);
static int lm78_new_client(struct i2c_adapter *adapter,
                           struct i2c_client *new_client);
static void lm78_remove_client(struct i2c_client *client);
static int lm78_command(struct i2c_client *client, unsigned int cmd, 
                        void *arg);
static void lm78_inc_use (struct i2c_client *client);
static void lm78_dec_use (struct i2c_client *client);

static int lm78_read_value(struct i2c_client *client, u8 register);
static int lm78_write_value(struct i2c_client *client, u8 register, u8 value);
static void lm78_update_client(struct i2c_client *client);
static void lm78_init_client(struct i2c_client *client);


static void lm78_in(struct i2c_client *client, int operation, int ctl_name,
                    int *nrels_mag, long *results);
static void lm78_fan(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void lm78_temp(struct i2c_client *client, int operation, int ctl_name,
                      int *nrels_mag, long *results);
static void lm78_vid(struct i2c_client *client, int operation, int ctl_name,
                     int *nrels_mag, long *results);
static void lm78_alarms(struct i2c_client *client, int operation, int ctl_name,
                        int *nrels_mag, long *results);
static void lm78_fan_div(struct i2c_client *client, int operation, int ctl_name,
                         int *nrels_mag, long *results);

/* I choose here for semi-static LM78 allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_LM78_NR 4
static struct i2c_client *lm78_list[MAX_LM78_NR];

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver lm78_driver = {
  /* name */		"SiS 5595",
  /* id */		I2C_DRIVERID_SIS5595,
  /* flags */		DF_NOTIFY,
  /* attach_adapter */  &lm78_attach_adapter,
  /* detach_client */	&lm78_detach_client,
  /* command */		&lm78_command,
  /* inc_use */		&lm78_inc_use,
  /* dec_use */		&lm78_dec_use
};

/* Used by lm78_init/cleanup */
static int lm78_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected LM78. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table lm78_dir_table_template[] = {
  { LM78_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_in },
  { LM78_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_in },
  { LM78_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_in },
  { LM78_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_in },
  { LM78_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_in },
  { LM78_SYSCTL_IN5, "in5", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_in },
  { LM78_SYSCTL_IN6, "in6", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_in },
  { LM78_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_fan },
  { LM78_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_fan },
  { LM78_SYSCTL_FAN3, "fan3", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_fan },
  { LM78_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_temp },
  { LM78_SYSCTL_VID, "vid", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_vid },
  { LM78_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_fan_div },
  { LM78_SYSCTL_ALARMS, "alarms", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &lm78_alarms },
  { 0 }
};

/* This is called when the module is loaded */
int lm78_attach_adapter(struct i2c_adapter *adapter)
{
    return lm78_detect_isa((struct isa_adapter *) adapter);
}

/* This is called when the module is loaded */
int lm78_detach_client(struct i2c_client *client)
{
    return lm78_detach_isa((struct isa_client *) client);
}

/* Locate SiS bridge and correct base address for LM78 */
#if LINUX_VERSION_CODE >= 0x020136 /* 2.1.54 */
static int find_sis(int *address)
{
  struct pci_dev *s_bridge;
  u16 val;

  if (! pci_present())
    return -ENODEV;

  s_bridge = pci_find_device(
		PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_503, NULL);
		
  if (! s_bridge)
    return -ENODEV;

  if ( PCIBIOS_SUCCESSFUL !=
	pci_read_config_word(s_bridge, LM78_BASE_REG, &val))
    return -ENODEV;
		
  *address = (val & 0xfff8);
  return 0;
}

#else

static int find_sis(int *address)
{
  unsigned char SIS_bus, SIS_devfn;
  u16 val;
  
  if (! pcibios_present())
    return -ENODEV;
		
  if(pcibios_find_device(
		PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_503, 0,
   		&SIS_bus, &SIS_devfn))
    return -ENODEV;
	
  if ( PCIBIOS_SUCCESSFUL !=
    pcibios_read_config_word(SIS_bus, SIS_devfn, LM78_BASE_REG, &val))
    return -ENODEV;

  *address = (val & 0xfff8);
  return 0;
}
#endif


/* Register and initialize lm78 it. */
int lm78_detect_isa(struct isa_adapter *adapter)
{
  int address = 0, err = 0;
  struct isa_client *new_client;
  enum lm78_type type;
  const char *type_name;
  const char *client_name;

  /* OK, this is detection. */

  if ((err=find_sis(&address))) {
    printk("SiS 5595 southbridge not found\n");
    return err;
  } 
  
  if ( (err = check_region(address, LM78_EXTENT)) < 0 )
  {
    printk("sis5595.o: IO 0x%x-0x%x already in use\n",
            address, address + LM78_EXTENT);
    return err;
  }
/*
  printk("sis5595.o: lm78 io base at 0x%4x\n", address);
*/	
  type = lm78;
  type_name = "sis5595";
  client_name = "SiS 5595 Sensor";

  request_region(address, LM78_EXTENT, type_name);

  /* Allocate space for a new client structure */
  if (! (new_client = kmalloc(sizeof(struct isa_client) + 
                              sizeof(struct lm78_data),
                             GFP_KERNEL)))
  {
    err=-ENOMEM;
    goto ERROR1;
  } 

  /* Fill the new client structure with data */
  new_client->data = (struct lm78_data *) (new_client + 1);
  new_client->addr = 0;
  strcpy(new_client->name,client_name);
  ((struct lm78_data *) (new_client->data))->type = type;
  new_client->isa_addr = address;
  if ((err = lm78_new_client((struct i2c_adapter *) adapter,
                             (struct i2c_client *) new_client)))
    goto ERROR2;

  /* Tell i2c-core a new client has arrived */
  if ((err = isa_attach_client(new_client)))
    goto ERROR3;
    
  /* Register a new directory entry with module sensors */
  if ((err = sensors_register_entry((struct i2c_client *) new_client,
                                    type_name,
                                    lm78_dir_table_template)) < 0)
    goto ERROR4;
  ((struct lm78_data *) (new_client->data)) -> sysctl_id = err;
  err = 0;

  /* Initialize the LM78 chip */
  lm78_init_client((struct i2c_client *) new_client);
  return err;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR4:
    isa_detach_client(new_client);
ERROR3:
    lm78_remove_client((struct i2c_client *) new_client);
ERROR2:
    kfree(new_client);
ERROR1:
    release_region(address, LM78_EXTENT);

  return err;
}

/* Deregister and remove a LM78 client */
int lm78_detach_isa(struct isa_client *client)
{
  int err,i;
  for (i = 0; i < MAX_LM78_NR; i++)
    if ((client == (struct isa_client *) (lm78_list[i])))
      break;
  if (i == MAX_LM78_NR) {
    printk("sis5595.o: Client to detach not found.\n");
    return -ENOENT;
  }

  sensors_deregister_entry(((struct lm78_data *)(client->data))->sysctl_id);

  if ((err = isa_detach_client(client))) {
    printk("sis5595.o: Client deregistration failed, client not detached.\n");
    return err;
  }
  lm78_remove_client((struct i2c_client *) client);
  release_region(client->isa_addr,LM78_EXTENT);
  kfree(client);
  return 0;
}

/* Find a free slot, and initialize most of the fields */
int lm78_new_client(struct i2c_adapter *adapter,
                    struct i2c_client *new_client)
{
  int i;
  struct lm78_data *data;

  /* First, seek out an empty slot */
  for(i = 0; i < MAX_LM78_NR; i++)
    if (! lm78_list[i])
      break;
  if (i == MAX_LM78_NR) {
    printk("sis5595.o: No empty slots left, recompile and heighten "
           "MAX_LM78_NR!\n");
    return -ENOMEM;
  }
  
  lm78_list[i] = new_client;
  new_client->id = i;
  new_client->adapter = adapter;
  new_client->driver = &lm78_driver;
  data = new_client->data;
  data->valid = 0;
  data->lock = MUTEX;
  data->update_lock = MUTEX;
  return 0;
}

/* Inverse of lm78_new_client */
void lm78_remove_client(struct i2c_client *client)
{
  int i;
  for (i = 0; i < MAX_LM78_NR; i++)
    if (client == lm78_list[i]) 
      lm78_list[i] = NULL;
}

/* No commands defined yet */
int lm78_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void lm78_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void lm78_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}
 

/* The SMBus locks itself, but ISA access must be locked explicitely! 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int lm78_read_value(struct i2c_client *client, u8 reg)
{
    int res;
    
    down((struct semaphore *) (client->data));
    outb_p(reg,(((struct isa_client *) client)->isa_addr) + 
               LM78_ADDR_REG_OFFSET);
    res = inb_p((((struct isa_client *) client)->isa_addr) + 
                LM78_DATA_REG_OFFSET);
    up((struct semaphore *) (client->data));
    return res;
}

/* The SMBus locks itself, but ISA access muse be locked explicitely! 
   There are some ugly typecasts here, but the good new is - they should
   nowhere else be necessary! */
int lm78_write_value(struct i2c_client *client, u8 reg, u8 value)
{
    down((struct semaphore *) (client->data));
    outb_p(reg,((struct isa_client *) client)->isa_addr + LM78_ADDR_REG_OFFSET);
    outb_p(value,((struct isa_client *) client)->isa_addr + LM78_DATA_REG_OFFSET);
    up((struct semaphore *) (client->data));
    return 0;
}

/* Called when we have found a new LM78. It should set limits, etc. */
void lm78_init_client(struct i2c_client *client)
{
  int vid;

  /* Reset all except Watchdog values and last conversion values
     This sets fan-divs to 2, among others */
  lm78_write_value(client,LM78_REG_CONFIG,0x80);

  vid = lm78_read_value(client,LM78_REG_VID_FANDIV) & 0x0f;
  vid |= 0x10;
  vid = VID_FROM_REG(vid);

  lm78_write_value(client,LM78_REG_IN_MIN(0),IN_TO_REG(LM78_INIT_IN_MIN_0));
  lm78_write_value(client,LM78_REG_IN_MAX(0),IN_TO_REG(LM78_INIT_IN_MAX_0));
  lm78_write_value(client,LM78_REG_IN_MIN(1),IN_TO_REG(LM78_INIT_IN_MIN_1));
  lm78_write_value(client,LM78_REG_IN_MAX(1),IN_TO_REG(LM78_INIT_IN_MAX_1));
  lm78_write_value(client,LM78_REG_IN_MIN(2),IN_TO_REG(LM78_INIT_IN_MIN_2));
  lm78_write_value(client,LM78_REG_IN_MAX(2),IN_TO_REG(LM78_INIT_IN_MAX_2));
  lm78_write_value(client,LM78_REG_IN_MIN(3),IN_TO_REG(LM78_INIT_IN_MIN_3));
  lm78_write_value(client,LM78_REG_IN_MAX(3),IN_TO_REG(LM78_INIT_IN_MAX_3));
  lm78_write_value(client,LM78_REG_IN_MIN(4),IN_TO_REG(LM78_INIT_IN_MIN_4));
  lm78_write_value(client,LM78_REG_IN_MAX(4),IN_TO_REG(LM78_INIT_IN_MAX_4));
  lm78_write_value(client,LM78_REG_IN_MIN(5),IN_TO_REG(LM78_INIT_IN_MIN_5));
  lm78_write_value(client,LM78_REG_IN_MAX(5),IN_TO_REG(LM78_INIT_IN_MAX_5));
  lm78_write_value(client,LM78_REG_IN_MIN(6),IN_TO_REG(LM78_INIT_IN_MIN_6));
  lm78_write_value(client,LM78_REG_IN_MAX(6),IN_TO_REG(LM78_INIT_IN_MAX_6));
  lm78_write_value(client,LM78_REG_FAN_MIN(1),
                   FAN_TO_REG(LM78_INIT_FAN_MIN_1,2));
  lm78_write_value(client,LM78_REG_FAN_MIN(2),
                   FAN_TO_REG(LM78_INIT_FAN_MIN_2,2));
  lm78_write_value(client,LM78_REG_FAN_MIN(3),
                   FAN_TO_REG(LM78_INIT_FAN_MIN_3,2));
  lm78_write_value(client,LM78_REG_TEMP_OVER,TEMP_TO_REG(LM78_INIT_TEMP_OVER));
  lm78_write_value(client,LM78_REG_TEMP_HYST,TEMP_TO_REG(LM78_INIT_TEMP_HYST));

  /* Start monitoring */
  lm78_write_value(client,LM78_REG_CONFIG,
                   (lm78_read_value(client,LM78_REG_CONFIG) & 0xf7) | 0x01);
  
}

void lm78_update_client(struct i2c_client *client)
{
  struct lm78_data *data = client->data;
  int i;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting lm78 update\n");
#endif
    for (i = 0; i <= 6; i++) {
      data->in[i]     = lm78_read_value(client,LM78_REG_IN(i));
      data->in_min[i] = lm78_read_value(client,LM78_REG_IN_MIN(i));
      data->in_max[i] = lm78_read_value(client,LM78_REG_IN_MAX(i));
    }
    for (i = 1; i <= 3; i++) {
      data->fan[i-1] = lm78_read_value(client,LM78_REG_FAN(i));
      data->fan_min[i-1] = lm78_read_value(client,LM78_REG_FAN_MIN(i));
    }
    data->temp = lm78_read_value(client,LM78_REG_TEMP);
    data->temp_over = lm78_read_value(client,LM78_REG_TEMP_OVER);
    data->temp_hyst = lm78_read_value(client,LM78_REG_TEMP_HYST);
    i = lm78_read_value(client,LM78_REG_VID_FANDIV);
    data->vid = i & 0x0f;
    data->vid |= 0x10;
    data->fan_div[0] = (i >> 4) & 0x03;
    data->fan_div[1] = i >> 6;
    data->alarms = lm78_read_value(client,LM78_REG_ALARM1);
/* Not available in SiS 5595
                 + (lm78_read_value(client,LM78_REG_ALARM2) << 8);
*/
    data->last_updated = jiffies;
    data->valid = 1;

    data->fan_div[2] = 1;
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
void lm78_in(struct i2c_client *client, int operation, int ctl_name, 
             int *nrels_mag, long *results)
{
  struct lm78_data *data = client->data;
  int nr = ctl_name - LM78_SYSCTL_IN0;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm78_update_client(client);
    results[0] = IN_FROM_REG(data->in_min[nr]);
    results[1] = IN_FROM_REG(data->in_max[nr]);
    results[2] = IN_FROM_REG(data->in[nr]);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
      if (*nrels_mag >= 1) {
        data->in_min[nr] = IN_TO_REG(results[0]);
        lm78_write_value(client,LM78_REG_IN_MIN(nr),data->in_min[nr]);
      }
      if (*nrels_mag >= 2) {
        data->in_max[nr] = IN_TO_REG(results[1]);
        lm78_write_value(client,LM78_REG_IN_MAX(nr),data->in_max[nr]);
      }
  }
}

void lm78_fan(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
  struct lm78_data *data = client->data;
  int nr = ctl_name - LM78_SYSCTL_FAN1 + 1;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm78_update_client(client);
    results[0] = FAN_FROM_REG(data->fan_min[nr-1],
                 DIV_FROM_REG(data->fan_div[nr-1]));
    results[1] = FAN_FROM_REG(data->fan[nr-1],
                 DIV_FROM_REG(data->fan_div[nr-1]));
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->fan_min[nr-1] = FAN_TO_REG(results[0],
                            DIV_FROM_REG(data->fan_div[nr-1]));
      lm78_write_value(client,LM78_REG_FAN_MIN(nr),data->fan_min[nr-1]);
    }
  }
}


void lm78_temp(struct i2c_client *client, int operation, int ctl_name,
               int *nrels_mag, long *results)
{
  struct lm78_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm78_update_client(client);
    results[0] = TEMP_FROM_REG(data->temp_over);
    results[1] = TEMP_FROM_REG(data->temp_hyst);
    results[2] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_over = TEMP_TO_REG(results[0]);
      lm78_write_value(client,LM78_REG_TEMP_OVER,data->temp_over);
    }
    if (*nrels_mag >= 2) {
      data->temp_hyst = TEMP_TO_REG(results[1]);
      lm78_write_value(client,LM78_REG_TEMP_HYST,data->temp_hyst);
    }
  }
}

void lm78_vid(struct i2c_client *client, int operation, int ctl_name,
              int *nrels_mag, long *results)
{
  struct lm78_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm78_update_client(client);
    results[0] = VID_FROM_REG(data->vid);
    *nrels_mag = 1;
  }
}

void lm78_alarms(struct i2c_client *client, int operation, int ctl_name,
                 int *nrels_mag, long *results)
{
  struct lm78_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm78_update_client(client);
    results[0] = ALARMS_FROM_REG(data->alarms);
    *nrels_mag = 1;
  }
}

void lm78_fan_div(struct i2c_client *client, int operation, int ctl_name,
                  int *nrels_mag, long *results)
{
  struct lm78_data *data = client->data;
  int old;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    lm78_update_client(client);
    results[0] = DIV_FROM_REG(data->fan_div[0]);
    results[1] = DIV_FROM_REG(data->fan_div[1]);
    results[2] = 2;
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    old = lm78_read_value(client,LM78_REG_VID_FANDIV);
    if (*nrels_mag >= 2) {
      data->fan_div[1] = DIV_TO_REG(results[1]);
      old = (old & 0x3f) | (data->fan_div[1] << 6);
    }
    if (*nrels_mag >= 1) {
      data->fan_div[0] = DIV_TO_REG(results[0]);
      old = (old & 0xcf) | (data->fan_div[0] << 4);
      lm78_write_value(client,LM78_REG_VID_FANDIV,old);
    }
  }
}

int lm78_init(void)
{
  int res;

  printk("sis5595.o version %s (%s)\n",LM_VERSION,LM_DATE);
  lm78_initialized = 0;

  if ((res =i2c_add_driver(&lm78_driver))) {
    printk("sis5595.o: Driver registration failed, module not inserted.\n");
    lm78_cleanup();
    return res;
  }
  lm78_initialized ++;
  return 0;
}

int lm78_cleanup(void)
{
  int res;

  if (lm78_initialized >= 1) {
    if ((res = i2c_del_driver(&lm78_driver))) {
      printk("sis5595.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    lm78_initialized --;
  }
  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Kyösti Mälkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("SiS 5595 Sensor device");

int init_module(void)
{
  return lm78_init();
}

int cleanup_module(void)
{
  return lm78_cleanup();
}

#endif /* MODULE */

