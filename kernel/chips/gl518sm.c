/*
    gl518sm.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
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
#include "smbus.h"
#include "sensors.h"
#include "i2c.h"
#include "version.h"
#include "compat.h"

/* Addresses to scan */
static unsigned short normal_i2c[] = {0x2c,0x2d,SENSORS_I2C_END};
static unsigned short normal_i2c_range[] = {SENSORS_I2C_END};
static unsigned int normal_isa[] = {SENSORS_ISA_END};
static unsigned int normal_isa_range[] = {SENSORS_ISA_END};

/* Insmod parameters */
SENSORS_INSMOD_2(gl518sm_r00,gl518sm_r80);

/* Defining this will enable debug messages for the voltage iteration
   code used with rev 0 ICs */
#undef DEBUG_VIN

/* Many GL518 constants specified below */

/* The GL518 registers */
#define GL518_REG_CHIP_ID 0x00
#define GL518_REG_REVISION 0x01
#define GL518_REG_VENDOR_ID 0x02
#define GL518_REG_CONF 0x03
#define GL518_REG_TEMP 0x04
#define GL518_REG_TEMP_OVER 0x05
#define GL518_REG_TEMP_HYST 0x06
#define GL518_REG_FAN_COUNT 0x07
#define GL518_REG_FAN_LIMIT 0x08
#define GL518_REG_VIN1_LIMIT 0x09
#define GL518_REG_VIN2_LIMIT 0x0a
#define GL518_REG_VIN3_LIMIT 0x0b
#define GL518_REG_VDD_LIMIT 0x0c
#define GL518_REG_VIN3 0x0d
#define GL518_REG_MISC 0x0f
#define GL518_REG_ALARM 0x10
#define GL518_REG_MASK 0x11
#define GL518_REG_INT 0x12
#define GL518_REG_VIN2 0x13
#define GL518_REG_VIN1 0x14
#define GL518_REG_VDD 0x15


/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

#define TEMP_TO_REG(val) (SENSORS_LIMIT(((((val)<0?(val)-5:(val)+5) / 10)+119),\
                                        0,255))
#define TEMP_FROM_REG(val) (((val) - 119) * 10)

extern inline u8 FAN_TO_REG(long rpm, int div)
{
  if (rpm == 0)
    return 255;
  rpm = SENSORS_LIMIT(rpm,1,1000000);
  return SENSORS_LIMIT((960000 + rpm*div/2) / (rpm*div),1,254);
}

#define FAN_FROM_REG(val,div) \
 ( (val)==0 ? 0 : (val)==255 ? 0 : (960000/((val)*(div))) )

#define IN_TO_REG(val) (SENSORS_LIMIT((((val)*10+8)/19),0,255))
#define IN_FROM_REG(val) (((val)*19)/10)

#define VDD_TO_REG(val) (SENSORS_LIMIT((((val)*10+11)/23),0,255))
#define VDD_FROM_REG(val) (((val)*23)/10)

#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)
#define DIV_FROM_REG(val) (1 << (val))

#define ALARMS_FROM_REG(val) val

#define BEEP_ENABLE_TO_REG(val) ((val)?0:1)
#define BEEP_ENABLE_FROM_REG(val) ((val)?0:1)

#define BEEPS_TO_REG(val) ((val) & 0x7f)
#define BEEPS_FROM_REG(val) (val)

/* Initial values */
#define GL518_INIT_TEMP_OVER 600
#define GL518_INIT_TEMP_HYST 500
#define GL518_INIT_FAN_MIN_1 3000
#define GL518_INIT_FAN_MIN_2 3000

/* These are somewhat sane */
#define GL518_INIT_VIN_1 330	/* 3.3 V */
#define GL518_INIT_VIN_2 286	/* 12 V */
#define GL518_INIT_VIN_3 260	/* Vcore */
#define GL518_INIT_VDD 500	/* 5 V */

#define GL518_INIT_PERCENTAGE 10

#define GL518_INIT_VIN_MIN_1 \
        (GL518_INIT_VIN_1 - GL518_INIT_VIN_1 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MAX_1 \
        (GL518_INIT_VIN_1 + GL518_INIT_VIN_1 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MIN_2 \
        (GL518_INIT_VIN_2 - GL518_INIT_VIN_2 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MAX_2 \
        (GL518_INIT_VIN_2 + GL518_INIT_VIN_2 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MIN_3 \
        (GL518_INIT_VIN_3 - GL518_INIT_VIN_3 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VIN_MAX_3 \
        (GL518_INIT_VIN_3 + GL518_INIT_VIN_3 * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VDD_MIN \
        (GL518_INIT_VDD - GL518_INIT_VDD * GL518_INIT_PERCENTAGE / 100) 
#define GL518_INIT_VDD_MAX \
        (GL518_INIT_VDD + GL518_INIT_VDD * GL518_INIT_PERCENTAGE / 100) 


/* Each client has this additional data */
struct gl518_data {
         int sysctl_id;
         enum chips type;

         struct semaphore update_lock;
         char valid;                 /* !=0 if following fields are valid */
         unsigned long last_updated; /* In jiffies */
         unsigned long last_updated_v00; 
	                             /* In jiffies (used only by rev00 chips) */

         u8 voltage[4];              /* Register values; [0] = VDD */
         u8 voltage_min[4];          /* Register values; [0] = VDD */
         u8 voltage_max[4];          /* Register values; [0] = VDD */
         u8 fan[2];
         u8 fan_min[2];
         u8 temp;                    /* Register values */
         u8 temp_over;               /* Register values */
         u8 temp_hyst;               /* Register values */
         u8 alarms,beeps;            /* Register value */
         u8 fan_div[2];              /* Register encoding, shifted right */
	 u8 beep_enable;             /* Boolean */	 
};

/* load time parameter that says if we want to spend 10 seconds for
   reading all voltages from a GL518 rev 0, or if we want to read zeros */
static int readall = 0;

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
MODULE_PARM(readall,"i");
#endif /* MODULE */

static int gl518_init(void);
static int gl518_cleanup(void);
static int gl518_attach_adapter(struct i2c_adapter *adapter);
static int gl518_detect(struct i2c_adapter *adapter, int address, int kind);
static void gl518_init_client(struct i2c_client *client);
static int gl518_detach_client(struct i2c_client *client);
static int gl518_command(struct i2c_client *client, unsigned int cmd,
                        void *arg);
static void gl518_inc_use (struct i2c_client *client);
static void gl518_dec_use (struct i2c_client *client);
static u16 swap_bytes(u16 val);
static int gl518_read_value(struct i2c_client *client, u8 reg);
static int gl518_write_value(struct i2c_client *client, u8 reg, u16 value);
static void gl518_update_client(struct i2c_client *client);
static void gl518_update_client_rev00(struct i2c_client *client);

static void gl518_vin(struct i2c_client *client, int operation, 
                      int ctl_name, int *nrels_mag, long *results);
static void gl518_fan(struct i2c_client *client, int operation, 
                      int ctl_name, int *nrels_mag, long *results);
static void gl518_temp(struct i2c_client *client, int operation, 
                       int ctl_name, int *nrels_mag, long *results);
static void gl518_fan_div(struct i2c_client *client, int operation, 
                          int ctl_name, int *nrels_mag, long *results);
static void gl518_alarms(struct i2c_client *client, int operation, 
                         int ctl_name, int *nrels_mag, long *results);
static void gl518_beep(struct i2c_client *client, int operation, int ctl_name, 
                int *nrels_mag, long *results);

/* This is the driver that will be inserted */
static struct i2c_driver gl518_driver = {
  /* name */            "GL518SM sensor chip driver",
  /* id */              I2C_DRIVERID_GL518,
  /* flags */           DF_NOTIFY,
  /* attach_adapter */  &gl518_attach_adapter,
  /* detach_client */   &gl518_detach_client,
  /* command */         &gl518_command,
  /* inc_use */         &gl518_inc_use,
  /* dec_use */         &gl518_dec_use
};

/* These files are created for each detected GL518. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table gl518_dir_table_template[] = {
  { GL518_SYSCTL_VIN1, "vin1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_VIN2, "vin2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_VIN3, "vin3", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_VDD, "vdd", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_vin },
  { GL518_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_fan },
  { GL518_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_fan },
  { GL518_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_temp },
  { GL518_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_fan_div },
  { GL518_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_alarms },
  { GL518_SYSCTL_BEEP, "beep", NULL, 0, 0644, NULL, &sensors_proc_real,
    &sensors_sysctl_real, NULL, &gl518_beep },
  { 0 }
};

/* Used by init/cleanup */
static int gl518_initialized = 0;

/* I choose here for semi-static GL518SM allocation. Complete dynamic
   allocation could also be used; the code needed for this would probably
   take more memory than the datastructure takes now. */
#define MAX_GL518_NR 4
static struct i2c_client *gl518_list[MAX_GL518_NR];

int gl518_attach_adapter(struct i2c_adapter *adapter)
{
  return sensors_detect(adapter,&addr_data,gl518_detect);
}

static int gl518_detect(struct i2c_adapter *adapter, int address, int kind)
{ 
  int i;
  struct i2c_client *new_client;
  struct gl518_data *data;
  int err=0;
  const char *type_name = "";
  const char *client_name = "";

  /* Make sure we aren't probing the ISA bus!! This is just a safety check
     at this moment; sensors_detect really won't call us. */
#ifdef DEBUG
  if (i2c_is_isa_adapter(adapter)) {
    printk("gl518sm.o: gl518_detect called for an ISA bus adapter?!?\n");
    return 0;
  }
#endif


  /* We need address registration for the I2C bus too. That is not yet
     implemented. */

  /* OK. For now, we presume we have a valid client. We now create the
     client structure, even though we cannot fill it completely yet.
     But it allows us to access gl518_{read,write}_value. */

  if (! (new_client = kmalloc(sizeof(struct i2c_client) +
                               sizeof(struct gl518_data),
                               GFP_KERNEL))) {
    err = -ENOMEM;
    goto ERROR0;
  }

  data = (struct gl518_data *) (new_client + 1);
  new_client->addr = address;
  new_client->data = data;
  new_client->adapter = adapter;
  new_client->driver = &gl518_driver;

  /* Now, we do the remaining detection. */

  if (kind < 0) {
    if ((gl518_read_value(new_client,GL518_REG_CHIP_ID) != 0x80) ||
        (gl518_read_value(new_client,GL518_REG_CONF) & 0x80))
      goto ERROR1;
  }

  /* Determine the chip type. */
  if (kind <= 0) {
    i = gl518_read_value(new_client,GL518_REG_REVISION);
    if (i == 0x00)
      kind = gl518sm_r00;
    else if (i == 0x80)
      kind = gl518sm_r80;
    else {
      if (kind == 0)
        printk("gl518sm.o: Ignoring 'force' parameter for unknown chip at "
               "adapter %d, address 0x%02x\n",i2c_adapter_id(adapter),address);
      goto ERROR1;
    }
  }

  if (kind == gl518sm_r00) {
    type_name = "gl518sm-r00";
    client_name = "GL518SM Revision 0x00 chip";
  } else if (kind == gl518sm_r80) {
    type_name = "gl518sm-r80";
    client_name = "GL518SM Revision 0x80 chip";
  } else {
#ifdef DEBUG
    printk("gl518sm.o: Internal error: unknown kind (%d)?!?",kind);
#endif
    goto ERROR1;
  }

  /* Fill in the remaining client fields and put it into the global list */
  strcpy(new_client->name,client_name);
  data->type = kind;

  for(i = 0; i < MAX_GL518_NR; i++)
    if (! gl518_list[i])
      break;
  if (i == MAX_GL518_NR) {
    printk("gl518sm.o: No empty slots left, recompile and heighten "
           "MAX_GL518_NR!\n");
    err = -ENOMEM;
    goto ERROR2;
  }
  gl518_list[i] = new_client;
  new_client->id = i;
  data->valid = 0;
  data->update_lock = MUTEX;

  /* Tell the I2C layer a new client has arrived */
  if ((err = i2c_attach_client(new_client)))
    goto ERROR3;

  /* Register a new directory entry with module sensors */
  if ((i = sensors_register_entry((struct i2c_client *) new_client,
                                  type_name,
                                  gl518_dir_table_template)) < 0) {
    err = i;
    goto ERROR4;
  }
  data->sysctl_id = i;

  /* Initialize the GL518SM chip */
  gl518_init_client((struct i2c_client *) new_client);
  return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

ERROR4:
  i2c_detach_client(new_client);
ERROR3:
  for (i = 0; i < MAX_GL518_NR; i++)
    if (new_client == gl518_list[i])
      gl518_list[i] = NULL;
ERROR2:
ERROR1:
  kfree(new_client);
ERROR0:
  return err;
}


/* Called when we have found a new GL518SM. It should set limits, etc. */
void gl518_init_client(struct i2c_client *client)
{
  /* Power-on defaults (bit 7=1) */
  gl518_write_value(client,GL518_REG_CONF,0x80); 

  /* No noisy output (bit 2=1), Comparator mode (bit 3=0), two fans (bit4=0),
     standby mode (bit6=0) */
  gl518_write_value(client,GL518_REG_CONF,0x04); 

  /* Never interrupts */
  gl518_write_value(client,GL518_REG_MASK,0x00);
    
  gl518_write_value(client,GL518_REG_TEMP_HYST,
                    TEMP_TO_REG(GL518_INIT_TEMP_HYST));
  gl518_write_value(client,GL518_REG_TEMP_OVER,
                    TEMP_TO_REG(GL518_INIT_TEMP_OVER));
  gl518_write_value(client,GL518_REG_MISC,(DIV_TO_REG(2) << 6) | 
                                              (DIV_TO_REG(2) << 4));
  gl518_write_value(client,GL518_REG_FAN_LIMIT,
                    (FAN_TO_REG(GL518_INIT_FAN_MIN_1,2) << 8) |
                    FAN_TO_REG(GL518_INIT_FAN_MIN_2,2));
  gl518_write_value(client,GL518_REG_VIN1_LIMIT,
                    (IN_TO_REG(GL518_INIT_VIN_MAX_1) << 8) |
                    IN_TO_REG(GL518_INIT_VIN_MIN_1));
  gl518_write_value(client,GL518_REG_VIN2_LIMIT,
                    (IN_TO_REG(GL518_INIT_VIN_MAX_2) << 8) |
                    IN_TO_REG(GL518_INIT_VIN_MIN_2));
  gl518_write_value(client,GL518_REG_VIN3_LIMIT,
                    (IN_TO_REG(GL518_INIT_VIN_MAX_3) << 8) |
                    IN_TO_REG(GL518_INIT_VIN_MIN_3));
  gl518_write_value(client,GL518_REG_VDD_LIMIT,
                    (VDD_TO_REG(GL518_INIT_VDD_MAX) << 8) |
                    VDD_TO_REG(GL518_INIT_VDD_MIN));

  /* Clear status register (bit 5=1), start (bit6=1) */
  gl518_write_value(client,GL518_REG_CONF,0x24);
  gl518_write_value(client,GL518_REG_CONF,0x44);
}

int gl518_detach_client(struct i2c_client *client)
{
  int err,i;

  sensors_deregister_entry(((struct gl518_data *)(client->data))->sysctl_id);

  if ((err = i2c_detach_client(client))) {
    printk("gl518sm.o: Client deregistration failed, client not detached.\n");
    return err;
  }

  for (i = 0; i < MAX_GL518_NR; i++)
    if (client == gl518_list[i])
      break;
  if ((i == MAX_GL518_NR)) {
    printk("gl518sm.o: Client to detach not found.\n");
    return -ENOENT;
  }
  gl518_list[i] = NULL;

  kfree(client);

  return 0;
}


/* No commands defined yet */
int gl518_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
  return 0;
}

/* Nothing here yet */
void gl518_inc_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void gl518_dec_use (struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

u16 swap_bytes(u16 val)
{
  return (val >> 8) | (val << 8);
}

/* Registers 0x07 to 0x0c are word-sized, others are byte-sized 
   GL518 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int gl518_read_value(struct i2c_client *client, u8 reg)
{
  if ((reg >= 0x07) && (reg <= 0x0c)) 
    return swap_bytes(smbus_read_word_data(client->adapter,client->addr,reg));
  else
    return smbus_read_byte_data(client->adapter,client->addr,reg);
}

/* Registers 0x07 to 0x0c are word-sized, others are byte-sized 
   GL518 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
int gl518_write_value(struct i2c_client *client, u8 reg, u16 value)
{
  if ((reg >= 0x07) && (reg <= 0x0c)) 
    return smbus_write_word_data(client->adapter,client->addr,reg,
                                 swap_bytes(value));
  else
    return smbus_write_byte_data(client->adapter,client->addr,reg,value);
}

void gl518_update_client(struct i2c_client *client)
{
  struct gl518_data *data = client->data;
  int val;

  down(&data->update_lock);

  if ((jiffies - data->last_updated > HZ+HZ/2 ) ||
      (jiffies < data->last_updated) || ! data->valid) {

#ifdef DEBUG
    printk("Starting gl518 update\n");
#endif

    data->alarms = gl518_read_value(client,GL518_REG_INT);
    data->beeps = gl518_read_value(client,GL518_REG_ALARM);

    val = gl518_read_value(client,GL518_REG_VDD_LIMIT);
    data->voltage_min[0] = val & 0xff;
    data->voltage_max[0] = (val >> 8) & 0xff;
    val = gl518_read_value(client,GL518_REG_VIN1_LIMIT);
    data->voltage_min[1] = val & 0xff;
    data->voltage_max[1] = (val >> 8) & 0xff;
    val = gl518_read_value(client,GL518_REG_VIN2_LIMIT);
    data->voltage_min[2] = val & 0xff;
    data->voltage_max[2] = (val >> 8) & 0xff;
    val = gl518_read_value(client,GL518_REG_VIN3_LIMIT);
    data->voltage_min[3] = val & 0xff;
    data->voltage_max[3] = (val >> 8) & 0xff;

    if (data->type != gl518sm_r00) {
      data->voltage[0] = gl518_read_value(client,GL518_REG_VDD);
      data->voltage[1] = gl518_read_value(client,GL518_REG_VIN1);
      data->voltage[2] = gl518_read_value(client,GL518_REG_VIN2);
    } else {
      if (!readall) {
        data->voltage[0] = 0;
        data->voltage[1] = 0;
        data->voltage[2] = 0;
      }
    }
    data->voltage[3] = gl518_read_value(client,GL518_REG_VIN3);

    val = gl518_read_value(client,GL518_REG_FAN_COUNT);
    data->fan[0] = (val >> 8) & 0xff;
    data->fan[1] = val & 0xff;

    val = gl518_read_value(client,GL518_REG_FAN_LIMIT);
    data->fan_min[0] = (val >> 8) & 0xff;
    data->fan_min[1] = val & 0xff;

    data->temp = gl518_read_value(client,GL518_REG_TEMP);
    data->temp_over = gl518_read_value(client,GL518_REG_TEMP_OVER);
    data->temp_hyst = gl518_read_value(client,GL518_REG_TEMP_HYST);

    val = gl518_read_value(client,GL518_REG_MISC);
    data->fan_div[0] = (val >> 6) & 0x03;
    data->fan_div[1] = (val >> 4) & 0x03;
    data->beep_enable = (gl518_read_value(client,GL518_REG_CONF) >> 2) & 1;

    data->last_updated = jiffies;
    data->valid = 1;
  }

  up(&data->update_lock);
}

/* Similar to gl518_update_client() but updates vdd, vin1, vin2 values 
   by doing slow and multiple comparisons for the GL518SM rev 00 that
   lacks support for direct reading of these values.
   (added by Ludovic Drolez (ldrolez@usa.net) */

void gl518_update_client_rev00(struct i2c_client *client)
{
  struct gl518_data *data = client->data;
  int j, min, max[3], delta;

  down(&data->update_lock);

#ifndef DEBUG_VIN
  /* as that update is slow, we consider the data valid for 30 seconds */
  if (jiffies - data->last_updated_v00 > 30*HZ ) {
#else
  if (jiffies - data->last_updated_v00 > HZ+HZ/2 ) {
#endif

     /* disable audible alarm */
     gl518_write_value(client,GL518_REG_CONF,
                        (gl518_read_value(client,GL518_REG_CONF) & 0xfb));

    min = 1;
    max[0] = VDD_TO_REG(GL518_INIT_VDD);
    max[1] = IN_TO_REG(GL518_INIT_VIN_1); 
    max[2] = IN_TO_REG(GL518_INIT_VIN_2);
    delta = 32;
    do {		        
      gl518_write_value(client,GL518_REG_VDD_LIMIT,(max[0] << 8) | min);
      gl518_write_value(client,GL518_REG_VIN1_LIMIT,(max[1] << 8) | min);
      gl518_write_value(client,GL518_REG_VIN2_LIMIT,(max[2] << 8) | min);

      /* we wait now 1.5 seconds before comparing */
      current->state = TASK_INTERRUPTIBLE;
      schedule_timeout(HZ + HZ/2);

      /* read comparators */	
      j = gl518_read_value(client,GL518_REG_INT);
      if ((j & 1) == 0) {
        max[0] = max[0] - delta;    
      } else {
        max[0] = max[0] + delta;
      }
      if ((j & 2) == 0) {
        max[1] = max[1] - delta;    
      } else {
        max[1] = max[1] + delta;
      }
      if ((j & 4) == 0) {
        max[2] = max[2] - delta;    
      } else {
        max[2] = max[2] + delta;
      }
      delta >>= 1;
#ifdef DEBUG_VIN
      printk("Iterations: %3d %3d %3d \n", max[0], max[1], max[2]);
#endif
    } while (delta != 0);
    
    /* do a final comparison to get to least significant bit */
    gl518_write_value(client,GL518_REG_VDD_LIMIT,(max[0] << 8) | min);
    gl518_write_value(client,GL518_REG_VIN1_LIMIT,(max[1] << 8) | min);
    gl518_write_value(client,GL518_REG_VIN2_LIMIT,(max[2] << 8) | min);
 
    /* we wait now 1.5 seconds before comparing */
    current->state = TASK_INTERRUPTIBLE;
    schedule_timeout(HZ + HZ/2);

    /* read comparators */	
    j = gl518_read_value(client,GL518_REG_INT);
    if ((j & 1) == 0) max[0]--;
    if ((j & 2) == 0) max[1]--;
    if ((j & 4) == 0) max[2]--;

#ifdef DEBUG_VIN
  if (data->type == gl518sm_r00) {
     printk("Avdd: Meter: %3d, Search: %3d, Diff: %3d mV\n",
     	     data->voltage[0], max[0], (max[0] - data->voltage[0]) * 23);
     printk("Vin1: Meter: %3d, Search: %3d, Diff: %3d mV\n",
     	     data->voltage[1], max[1], (max[1] - data->voltage[1]) * 19);
     printk("Vin2: Meter: %3d, Search: %3d, Diff: %3d mV\n",
     	     data->voltage[2], max[2], (max[2] - data->voltage[2]) * 19);
  } else
     printk("No voltage meter to read on rev 00 ICs\n");     
#endif  

    /* update voltages values */    
    data->voltage[0] = max[0];
    data->voltage[1] = max[1];
    data->voltage[2] = max[2];
    
    /* restore original comparator values */
    gl518_write_value(client,GL518_REG_VDD_LIMIT,
                      (data->voltage_max[0] << 8) | data->voltage_min[0]);
    gl518_write_value(client,GL518_REG_VIN1_LIMIT,
                      (data->voltage_max[1] << 8) | data->voltage_min[1]);
    gl518_write_value(client,GL518_REG_VIN2_LIMIT,
                      (data->voltage_max[2] << 8) | data->voltage_min[2]);

    data->last_updated_v00 = jiffies;
    
    /* reset audible alarm state */
    gl518_write_value(client,GL518_REG_CONF,
           (gl518_read_value(client,GL518_REG_CONF) & 0xfb) | 
           (data->beep_enable << 2));
  }
  up(&data->update_lock);
}

void gl518_temp(struct i2c_client *client, int operation, int ctl_name,
                int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 1;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = TEMP_FROM_REG(data->temp_over);
    results[1] = TEMP_FROM_REG(data->temp_hyst);
    results[2] = TEMP_FROM_REG(data->temp);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->temp_over = TEMP_TO_REG(results[0]);
      gl518_write_value(client,GL518_REG_TEMP_OVER,data->temp_over);
    }
    if (*nrels_mag >= 2) {
      data->temp_hyst = TEMP_TO_REG(results[1]);
      gl518_write_value(client,GL518_REG_TEMP_HYST,data->temp_hyst);
    }
  }
}

void gl518_vin(struct i2c_client *client, int operation, int ctl_name, 
               int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  int nr = ctl_name - GL518_SYSCTL_VDD;
  int regnr,old=0;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 2;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
#ifndef DEBUG_VIN    
    if ((data->type == gl518sm_r00) && (nr != 3) && readall)
       /* only update VDD, VIN1, VIN2 voltages */
       gl518_update_client_rev00(client);
#else
    gl518_update_client_rev00(client);
#endif
    results[0] = nr?IN_FROM_REG(data->voltage_min[nr]):
                    VDD_FROM_REG(data->voltage_min[nr]);
    results[1] = nr?IN_FROM_REG(data->voltage_max[nr]):
                    VDD_FROM_REG(data->voltage_max[nr]);
    results[2] = nr?IN_FROM_REG(data->voltage[nr]):
                    VDD_FROM_REG(data->voltage[nr]);
    *nrels_mag = 3;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    regnr=nr==0?GL518_REG_VDD_LIMIT:nr==1?GL518_REG_VIN1_LIMIT:nr==2?
                GL518_REG_VIN2_LIMIT:GL518_REG_VIN3_LIMIT;
    if (*nrels_mag == 1)
      old = gl518_read_value(client,regnr) & 0xff00;
    if (*nrels_mag >= 2) {
      data->voltage_max[nr] = nr?IN_TO_REG(results[1]):VDD_TO_REG(results[1]);
      old = data->voltage_max[nr] << 8;
    }
    if (*nrels_mag >= 1) {
      data->voltage_min[nr] = nr?IN_TO_REG(results[0]):VDD_TO_REG(results[0]);
      old |= data->voltage_min[nr];
      gl518_write_value(client,regnr,old);
    }
  }
} 


void gl518_fan(struct i2c_client *client, int operation, int ctl_name, 
               int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  int nr = ctl_name - GL518_SYSCTL_FAN1;
  int old;

  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = FAN_FROM_REG(data->fan_min[nr],
                 DIV_FROM_REG(data->fan_div[nr]));
    results[1] = FAN_FROM_REG(data->fan[nr],DIV_FROM_REG(data->fan_div[nr]));
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->fan_min[nr] = FAN_TO_REG(results[0],
                                     DIV_FROM_REG(data->fan_div[nr]));
      old = gl518_read_value(client,GL518_REG_FAN_LIMIT);
      if (nr == 0)
        old = (old & 0x00ff) | (data->fan_min[nr] << 8);
      else
        old = (old & 0xff00) | data->fan_min[nr];
      gl518_write_value(client,GL518_REG_FAN_LIMIT,old);
    }
  }
}


void gl518_alarms(struct i2c_client *client, int operation, int ctl_name, 
                  int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = ALARMS_FROM_REG(data->alarms);
    *nrels_mag = 1;
  }
}

void gl518_beep(struct i2c_client *client, int operation, int ctl_name, 
                int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = BEEP_ENABLE_FROM_REG(data->beep_enable);
    results[1] = BEEPS_FROM_REG(data->beeps);
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    if (*nrels_mag >= 1) {
      data->beep_enable = BEEP_ENABLE_TO_REG(results[0]);
      gl518_write_value(client,GL518_REG_CONF,
                        (gl518_read_value(client,GL518_REG_CONF) & 0xfb) | 
                         (data->beep_enable << 2));
    }
    if (*nrels_mag >= 2) {
      data->beeps = BEEPS_TO_REG(results[1]);
      gl518_write_value(client,GL518_REG_ALARM,data->beeps);
    }
  }
}

void gl518_fan_div(struct i2c_client *client, int operation, int ctl_name,
                   int *nrels_mag, long *results)
{
  struct gl518_data *data = client->data;
  int old;
  if (operation == SENSORS_PROC_REAL_INFO)
    *nrels_mag = 0;
  else if (operation == SENSORS_PROC_REAL_READ) {
    gl518_update_client(client);
    results[0] = DIV_FROM_REG(data->fan_div[0]);
    results[1] = DIV_FROM_REG(data->fan_div[1]);
    *nrels_mag = 2;
  } else if (operation == SENSORS_PROC_REAL_WRITE) {
    old = gl518_read_value(client,GL518_REG_MISC);
    if (*nrels_mag >= 2) {
      data->fan_div[1] = DIV_TO_REG(results[1]);
      old = (old & 0xcf) | (data->fan_div[1] << 4);
    }
    if (*nrels_mag >= 1) {
      data->fan_div[0] = DIV_TO_REG(results[0]);
      old = (old & 0x3f) | (data->fan_div[0] << 6);
    }
    gl518_write_value(client,GL518_REG_MISC,old);
  }
}


int gl518_init(void)
{
  int res;

  printk("gl518sm.o version %s (%s)\n",LM_VERSION,LM_DATE);
  gl518_initialized = 0;
  if ((res = i2c_add_driver(&gl518_driver))) {
    printk("gl518sm.o: Driver registration failed, module not inserted.\n");
    gl518_cleanup();
    return res;
  }
  gl518_initialized ++;
  return 0;
}

int gl518_cleanup(void)
{
  int res;

  if (gl518_initialized >= 1) {
    if ((res = i2c_del_driver(&gl518_driver))) {
      printk("gl518.o: Driver deregistration failed, module not removed.\n");
      return res;
    }
    gl518_initialized --;
  }

  return 0;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("GL518SM driver");

int init_module(void)
{
  return gl518_init();
}

int cleanup_module(void)
{
  return gl518_cleanup();
}

#endif /* MODULE */
