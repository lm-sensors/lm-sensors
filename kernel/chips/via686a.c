/*
    via686a.c - Part of lm_sensors, Linux kernel modules
                for hardware monitoring
                
    Copyright (c) 1998, 1999,2000  Frodo Looijaard <frodol@dds.nl>,
                        Kyösti Mälkki <kmalkki@cc.hut.fi>,
			Mark Studebaker <mdsxyz123@yahoo.com>,
			and Bob Dougherty <bobd@stanford.edu>
    (Some conversion-factor data were contributed by Jonathan Teh Soon Yew <j.teh@iname.com>
    and Alex van Kaam <darkside@chello.nl>.)

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
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include "version.h"
#include "i2c-isa.h"
#include "sensors.h"
#include <linux/init.h>

#ifndef PCI_DEVICE_ID_VIA_82C686_4
#define PCI_DEVICE_ID_VIA_82C686_4 0x3057
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,1))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,13)
#define THIS_MODULE NULL
#endif

/* Addresses to scan.
   Note that we can't determine the ISA address until we have initialized
   our module */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(via686a);

/*
   The Via 686a southbridge has a LM78-like chip integrated on the same IC.
   This driver is a customized copy of lm78.c
*/

/* Many VIA686A constants specified below */

/* Length of ISA address segment */
#define VIA686A_EXTENT 0x80
#define VIA686A_BASE_REG 0x70
#define VIA686A_ENABLE_REG 0x74

/* The VIA686A registers */
/* ins numbered 0-4 */
#define VIA686A_REG_IN_MAX(nr) (0x2b + ((nr) * 2))
#define VIA686A_REG_IN_MIN(nr) (0x2c + ((nr) * 2))
#define VIA686A_REG_IN(nr)     (0x22 + (nr))

/* fans numbered 1-2 */
#define VIA686A_REG_FAN_MIN(nr) (0x3a + (nr))
#define VIA686A_REG_FAN(nr)     (0x28 + (nr))

//static const u8 regtemp[] = {0x20, 0x21, 0x19};
static const u8 regtemp[] = {0x20, 0x21, 0x1f};
static const u8 regover[] = {0x39, 0x3d, 0x1d};
static const u8 reghyst[] = {0x3a, 0x3e, 0x1e};
/* temps numbered 1-3 */
#define VIA686A_REG_TEMP(nr)		(regtemp[(nr) - 1])
#define VIA686A_REG_TEMP_OVER(nr)	(regover[(nr) - 1])
#define VIA686A_REG_TEMP_HYST(nr)	(reghyst[(nr) - 1])
#define VIA686A_REG_TEMP_LOW1	0x4b
#define VIA686A_REG_TEMP_LOW23	0x49

#define VIA686A_REG_ALARM1 0x41
#define VIA686A_REG_ALARM2 0x42
#define VIA686A_REG_FANDIV 0x47
#define VIA686A_REG_CONFIG 0x40

#define ROUND_INT(val) ((val)<0?((val)-0.5):((val)+0.5))

/* Conversions. Rounding and limit checking is only done on the TO_REG
   variants. */

/********* VOLTAGE CONVERSIONS (Bob Dougherty) ********/
// From HWMon.cpp (Copyright 1998-2000 Jonathan Teh Soon Yew):
// voltagefactor[0]=1.25/2628;		// Vccp
// voltagefactor[1]=1.25/2628;		// +2.5V
// voltagefactor[2]=1.67/2628;		// +3.3V
// voltagefactor[3]=2.6/2628;		// +5V
// voltagefactor[4]=6.3/2628;		// +12V
// in[i]=(data[i+2]*25.0+133)*voltagefactor[i];
// 
// These get us close, but they don't completely agree with what my BIOS says-
// they are all a bit low.
extern inline u8 IN_TO_REG(long val, int inNum)
{
  // there's an extra /100 hidden in the conversion factors here
  if (inNum<=1)
    return (u8)SENSORS_LIMIT(ROUND_INT(0.04*(val/0.047565-133)),0,255);
  else if (inNum==2)
    return (u8)SENSORS_LIMIT(ROUND_INT(0.04*(val/0.063546-133)),0,255);
  else if (inNum==3)
    return (u8)SENSORS_LIMIT(ROUND_INT(0.04*(val/0.098935-133)),0,255);
  else 
    return (u8)SENSORS_LIMIT(ROUND_INT(0.04*(val/0.23973-133)),0,255);
}
extern inline long IN_FROM_REG(u8 val, int inNum)
{
   // use 2500.0 instead of 25.0 because we nneed to multiply val by 100
  if (inNum<=1) 
    return (long)ROUND_INT((2500.0*val+133)*0.00047565);
  else if (inNum==2)
    return (long)ROUND_INT((2500.0*val+133)*0.00063546);
  else if (inNum==3)
    return (long)ROUND_INT((2500.0*val+133)*0.00098935);
  else 
    return (long)ROUND_INT((2500.0*val+133)*0.0023973);
}

extern inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

/******** TEMP CONVERSIONS (Bob Dougherty) *********/
// linear fits from HWMon.cpp (Copyright 1998-2000 Jonathan Teh Soon Yew)
//	if(temp<169)
//		return double(temp)*0.427-32.08;
//	else if(temp>=169 && temp<=202)
//		return double(temp)*0.582-58.16;
//	else
//		return double(temp)*0.924-127.33;
//
// A fifth-order polynomial fits the unofficial data (provided by Alex van Kaam <darkside@chello.nl>)
// a bit better.  It also give more reasonable numbers on my machine (ie. they agree with what my
// BIOS tells me).  Here's the fifth-order fit to the 10-bit data:
// temp = 6.500371171990e-09*val^5 +-4.006529810457e-06*val^4 +9.830610857874e-04*val^3 +
// -1.187047495730e-01*val^2 +8.700574403605e+00*val +-2.836026823804e+02
// 
// Alas, neither work well because of precision limits in our math (?!?), so
// I came up with a pretty good piecewise-2nd-order:
// 
// For 8-bit readings:
//   0-59: -0.00831398*val*val + 1.46882437*val - 63.66742250
//   60:179: 0.00056874*val*val + 0.29665581*val - 23.7538571
//   180-255: 0.01275451*val*val - 4.39034916*val + 428.14343750
// 
// For 10-bit readings:
//   0-283: -0.00051962*val*val + 0.36720609*val - 63.6674225
//   284:763: 0.00004141*val*val + 0.06853503*val + -22.48959243
//   764-1023: 0.00087626*val*val + -1.23656143*val + 488.97355989
// 
// To convert the other way (temp to 8-bit reg val): 
//   <0: 0.01864022*val*val + 2.12242751*val + 290.16637347
//   0-55: -0.00714790*val*val + 2.60844692*val + 70.67197155
//   >55: -0.00991559*val*val + 2.48774551*val + 85.21011594

// Converting temps to register values
extern inline u8 TEMP_TO_REG(long val)
{
  val /= 10;
  if (val<0x00) // val<0
    return (u8)SENSORS_LIMIT(ROUND_INT(0.01864022*val*val + 2.12242751*val + 72.54159337),0,255);
  else if (val<0x37) // val<55
    return (u8)SENSORS_LIMIT(ROUND_INT(-0.00714790*val*val + 2.60844692*val + 70.67197155),0,255);
  else
    return (u8)SENSORS_LIMIT(ROUND_INT(-0.00991559*val*val + 2.48774551*val + 85.21011594),0,255);

}

/* for 8-bit temperature hyst and over registers */
extern inline long TEMP_FROM_REG(u8 val)
{
  if (val<0x47) // val<71
    return (long)ROUND_INT(10.0*(-0.00831398*val*val + 1.46882437*val - 63.66742250));
  else if (val<0xbf) // val<191
    return (long)ROUND_INT(10.0*(0.00056874*val*val + 0.29665581*val - 23.7538571));
  else
    return (long)ROUND_INT(10.0*(0.01275451*val*val - 4.39034916*val + 428.14343750));
}

/* for 10-bit temperature readings */
extern inline long TEMP_FROM_REG10(u16 val)
{
  if (val<0x011c)  // val<284
    return (long)ROUND_INT(10.0*(-0.00051962*val*val + 0.36720609*val - 63.6674225));
  else if (val<0x02fc) //val<764
    return (long)ROUND_INT(10.0*(0.00004141*val*val + 0.06853503*val + -22.48959243));
  else
    return (long)ROUND_INT(10.0*(0.00087626*val*val + -1.23656143*val + 488.97355989));
 
}

#define ALARMS_FROM_REG(val) (val)

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)

/* Initial limits */
#define VIA686A_INIT_IN_0 200
#define VIA686A_INIT_IN_1 250
#define VIA686A_INIT_IN_2 330
#define VIA686A_INIT_IN_3 (((500)   * 100)/168)
#define VIA686A_INIT_IN_4 (((1200)  * 10)/38)
//#define VIA686A_INIT_IN_3 500
//#define VIA686A_INIT_IN_4 1200

#define VIA686A_INIT_IN_PERCENTAGE 10

#define VIA686A_INIT_IN_MIN_0 \
        (VIA686A_INIT_IN_0 - VIA686A_INIT_IN_0 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_0 \
        (VIA686A_INIT_IN_0 + VIA686A_INIT_IN_0 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_1 \
        (VIA686A_INIT_IN_1 - VIA686A_INIT_IN_1 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_1 \
        (VIA686A_INIT_IN_1 + VIA686A_INIT_IN_1 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_2 \
        (VIA686A_INIT_IN_2 - VIA686A_INIT_IN_2 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_2 \
        (VIA686A_INIT_IN_2 + VIA686A_INIT_IN_2 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_3 \
        (VIA686A_INIT_IN_3 - VIA686A_INIT_IN_3 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_3 \
        (VIA686A_INIT_IN_3 + VIA686A_INIT_IN_3 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MIN_4 \
        (VIA686A_INIT_IN_4 - VIA686A_INIT_IN_4 * VIA686A_INIT_IN_PERCENTAGE / 100)
#define VIA686A_INIT_IN_MAX_4 \
        (VIA686A_INIT_IN_4 + VIA686A_INIT_IN_4 * VIA686A_INIT_IN_PERCENTAGE / 100)

#define VIA686A_INIT_FAN_MIN	3000

#define VIA686A_INIT_TEMP_OVER 600
#define VIA686A_INIT_TEMP_HYST 500

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

/* This module may seem overly long and complicated. In fact, it is not so
   bad. Quite a lot of bookkeeping is done. A real driver can often cut
   some corners. */

/* For each registered VIA686A, we need to keep some data in memory. That
   data is pointed to by via686a_list[NR]->data. The structure itself is
   dynamically allocated, at the same time when a new via686a client is
   allocated. */
struct via686a_data {
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 in[5];		/* Register value */
	u8 in_max[5];		/* Register value */
	u8 in_min[5];		/* Register value */
	u8 fan[2];		/* Register value */
	u8 fan_min[2];		/* Register value */
	u16 temp[3];		/* Register value 10 bit */
	u8 temp_over[3];	/* Register value */
	u8 temp_hyst[3];	/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	u16 alarms;		/* Register encoding, combined */
};


#ifdef MODULE
static
#else
extern
#endif
int __init sensors_via686a_init(void);
static int __init via686a_cleanup(void);

static int via686a_attach_adapter(struct i2c_adapter *adapter);
static int via686a_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int via686a_detach_client(struct i2c_client *client);
static int via686a_command(struct i2c_client *client, unsigned int cmd,
			   void *arg);
static void via686a_inc_use(struct i2c_client *client);
static void via686a_dec_use(struct i2c_client *client);

static int via686a_read_value(struct i2c_client *client, u8 register);
static void via686a_write_value(struct i2c_client *client, u8 register,
			       u8 value);
static void via686a_update_client(struct i2c_client *client);
static void via686a_init_client(struct i2c_client *client);
static int via686a_find(int *address);


static void via686a_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void via686a_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void via686a_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void via686a_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void via686a_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);

static int via686a_id = 0;

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver via686a_driver = {
	/* name */ "VIA 686A",
	/* id */ I2C_DRIVERID_VIA686A,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &via686a_attach_adapter,
	/* detach_client */ &via686a_detach_client,
	/* command */ &via686a_command,
	/* inc_use */ &via686a_inc_use,
	/* dec_use */ &via686a_dec_use
};

/* Used by via686a_init/cleanup */
static int __initdata via686a_initialized = 0;

/* The /proc/sys entries */
/* These files are created for each detected VIA686A. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table via686a_dir_table_template[] = {
	{VIA686A_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_in},
	{VIA686A_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_fan},
	{VIA686A_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_fan},
	{VIA686A_SYSCTL_TEMP, "temp1", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_temp},
	{VIA686A_SYSCTL_TEMP2, "temp2", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_temp},
	{VIA686A_SYSCTL_TEMP3, "temp3", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_temp},
	{VIA686A_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_fan_div},
	{VIA686A_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &sensors_proc_real,
	 &sensors_sysctl_real, NULL, &via686a_alarms},
	{0}
};

static inline int via686a_read_value(struct i2c_client *client, u8 reg)
{
	return(inb_p(client->addr + reg));
}

static inline void via686a_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	outb_p(value, client->addr + reg);
}

/* This is called when the module is loaded */
int via686a_attach_adapter(struct i2c_adapter *adapter)
{
	return sensors_detect(adapter, &addr_data, via686a_detect);
}

/* Locate chip and get correct base address */
int via686a_find(int *address)
{
	struct pci_dev *s_bridge;
	u16 val;

	if (!pci_present())
		return -ENODEV;

	if (! (s_bridge = pci_find_device(PCI_VENDOR_ID_VIA,
	       PCI_DEVICE_ID_VIA_82C686_4, NULL)))
		return -ENODEV;

	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_word(s_bridge, VIA686A_BASE_REG, &val))
		return -ENODEV;
	*address = (val & 0xff80);

	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_word(s_bridge, VIA686A_ENABLE_REG, &val))
		return -ENODEV;
	if((*address == 0) || !(val & 0x01)) {
		printk("via686a.o: sensors not enabled - upgrade BIOS?\n");
		return -ENODEV;
	}
	return 0;
}

int via686a_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct via686a_data *data;
	int err = 0;
	const char *type_name = "via686a";

	/* Make sure we are probing the ISA bus!!  */
	if (!i2c_is_isa_adapter(adapter)) {
		printk
		    ("via686a.o: via686a_detect called for an I2C bus adapter?!?\n");
		return 0;
	}

	if (check_region(address, VIA686A_EXTENT)) {
		printk("via686a.o: region 0x%x already in use!\n", address);
		err = -ENODEV;
		goto ERROR0;
	}

	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct via686a_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct via686a_data *) (new_client + 1);
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &via686a_driver;
	new_client->flags = 0;

	/* Reserve the ISA region */
	request_region(address, VIA686A_EXTENT, type_name);

	/* Fill in the remaining client fields and put into the global list */
	strcpy(new_client->name, "Via 686A Integrated Sensors");

	new_client->id = via686a_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = sensors_register_entry((struct i2c_client *) new_client,
					type_name,
					via686a_dir_table_template,
					THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	/* Initialize the VIA686A chip */
	via686a_init_client(new_client);
	return 0;

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	release_region(address, VIA686A_EXTENT);
	kfree(new_client);
      ERROR0:
	return err;
}

int via686a_detach_client(struct i2c_client *client)
{
	int err;

	sensors_deregister_entry(((struct via686a_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("via686a.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	release_region(client->addr, VIA686A_EXTENT);
	kfree(client);

	return 0;
}

/* No commands defined yet */
int via686a_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void via686a_inc_use(struct i2c_client *client)
{
	MOD_INC_USE_COUNT;
}

void via686a_dec_use(struct i2c_client *client)
{
	MOD_DEC_USE_COUNT;
}

/* Called when we have found a new VIA686A. Set limits, etc. */
void via686a_init_client(struct i2c_client *client)
{
	int i;

	/* Reset the device */
	via686a_write_value(client, VIA686A_REG_CONFIG, 0x80);

	via686a_write_value(client, VIA686A_REG_IN_MIN(0),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_0, 0));
	via686a_write_value(client, VIA686A_REG_IN_MAX(0),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_0, 0));
	via686a_write_value(client, VIA686A_REG_IN_MIN(1),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_1, 1));
	via686a_write_value(client, VIA686A_REG_IN_MAX(1),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_1, 1));
	via686a_write_value(client, VIA686A_REG_IN_MIN(2),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_2, 2));
	via686a_write_value(client, VIA686A_REG_IN_MAX(2),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_2, 2));
	via686a_write_value(client, VIA686A_REG_IN_MIN(3),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_3, 3));
	via686a_write_value(client, VIA686A_REG_IN_MAX(3),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_3, 3));
	via686a_write_value(client, VIA686A_REG_IN_MIN(4),
			    IN_TO_REG(VIA686A_INIT_IN_MIN_4, 4));
	via686a_write_value(client, VIA686A_REG_IN_MAX(4),
			    IN_TO_REG(VIA686A_INIT_IN_MAX_4, 4));
	via686a_write_value(client, VIA686A_REG_FAN_MIN(1),
			    FAN_TO_REG(VIA686A_INIT_FAN_MIN, 2));
	via686a_write_value(client, VIA686A_REG_FAN_MIN(2),
			    FAN_TO_REG(VIA686A_INIT_FAN_MIN, 2));
	for(i = 1; i < 3; i++) {
		via686a_write_value(client, VIA686A_REG_TEMP_OVER(i),
				    TEMP_TO_REG(VIA686A_INIT_TEMP_OVER));
		via686a_write_value(client, VIA686A_REG_TEMP_HYST(i),
				    TEMP_TO_REG(VIA686A_INIT_TEMP_HYST));
	}

	/* Start monitoring */
	via686a_write_value(client, VIA686A_REG_CONFIG, 0x01);
}

void via686a_update_client(struct i2c_client *client)
{
	struct via686a_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

		for (i = 0; i <= 4; i++) {
			data->in[i] =
			    via686a_read_value(client, VIA686A_REG_IN(i));
			data->in_min[i] = via686a_read_value(client,
			                           VIA686A_REG_IN_MIN(i));
			data->in_max[i] = via686a_read_value(client,
			                           VIA686A_REG_IN_MAX(i));
		}
		for (i = 1; i <= 2; i++) {
			data->fan[i - 1] =
			    via686a_read_value(client, VIA686A_REG_FAN(i));
			data->fan_min[i - 1] = via686a_read_value(client,
			                           VIA686A_REG_FAN_MIN(i));
		}
		for (i = 1; i <= 3; i++) {
			data->temp[i - 1] = via686a_read_value(client,
			                    VIA686A_REG_TEMP(i)) << 2;
			data->temp_over[i - 1] = via686a_read_value(client,
			                         VIA686A_REG_TEMP_OVER(i));
			data->temp_hyst[i - 1] = via686a_read_value(client,
			                         VIA686A_REG_TEMP_HYST(i));
		}
		/* add in lower 2 bits */
		data->temp[0] |= (via686a_read_value(client,
			           VIA686A_REG_TEMP_LOW1) & 0xc0) >> 6;
		data->temp[1] |= (via686a_read_value(client,
			           VIA686A_REG_TEMP_LOW23) & 0x30) >> 4;
		data->temp[2] |= (via686a_read_value(client,
			           VIA686A_REG_TEMP_LOW23) & 0xc0) >> 6;
	       
		i = via686a_read_value(client, VIA686A_REG_FANDIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = i >> 6;
		data->alarms = via686a_read_value(client, VIA686A_REG_ALARM1) ||
		       (via686a_read_value(client, VIA686A_REG_ALARM2) << 8);
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
void via686a_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int nr = ctl_name - VIA686A_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = IN_FROM_REG(data->in_min[nr], nr);
		results[1] = IN_FROM_REG(data->in_max[nr], nr);
		results[2] = IN_FROM_REG(data->in[nr], nr);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->in_min[nr] = IN_TO_REG(results[0], nr);
			via686a_write_value(client, VIA686A_REG_IN_MIN(nr),
					    data->in_min[nr]);
		}
		if (*nrels_mag >= 2) {
			data->in_max[nr] = IN_TO_REG(results[1], nr);
			via686a_write_value(client, VIA686A_REG_IN_MAX(nr),
					    data->in_max[nr]);
		}
	}
}

void via686a_fan(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int nr = ctl_name - VIA686A_SYSCTL_FAN1 + 1;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
					  DIV_FROM_REG(data->
						       fan_div[nr - 1]));
		results[1] =
		    FAN_FROM_REG(data->fan[nr - 1],
				 DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0],
				       DIV_FROM_REG(data->fan_div[nr - 1]));
			via686a_write_value(client,
					    VIA686A_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}

void via686a_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int nr = ctl_name - VIA686A_SYSCTL_TEMP;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = TEMP_FROM_REG(data->temp_over[nr]);
		results[1] = TEMP_FROM_REG(data->temp_hyst[nr]);
		results[2] = TEMP_FROM_REG10(data->temp[nr]);
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->temp_over[nr] = TEMP_TO_REG(results[0]);
			via686a_write_value(client, VIA686A_REG_TEMP_OVER(nr),
					    data->temp_over[nr]);
		}
		if (*nrels_mag >= 2) {
			data->temp_hyst[nr] = TEMP_TO_REG(results[1]);
			via686a_write_value(client, VIA686A_REG_TEMP_HYST(nr),
					    data->temp_hyst[nr]);
		}
	}
}

void via686a_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = ALARMS_FROM_REG(data->alarms);
		*nrels_mag = 1;
	}
}

void via686a_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	struct via686a_data *data = client->data;
	int old;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		via686a_update_client(client);
		results[0] = DIV_FROM_REG(data->fan_div[0]);
		results[1] = DIV_FROM_REG(data->fan_div[1]);
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		old = via686a_read_value(client, VIA686A_REG_FANDIV);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			via686a_write_value(client, VIA686A_REG_FANDIV,
					    old);
		}
	}
}

int __init sensors_via686a_init(void)
{
	int res, addr;

	printk("via686a.o version %s (%s)\n", LM_VERSION, LM_DATE);
	via686a_initialized = 0;

	if (via686a_find(&addr)) {
		printk("via686a.o: No Via 686A sensors found.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	if ((res = i2c_add_driver(&via686a_driver))) {
		printk
		 ("via686a.o: Driver registration failed.\n");
		via686a_cleanup();
		return res;
	}
	via686a_initialized++;
	return 0;
}

int __init via686a_cleanup(void)
{
	int res;

	if (via686a_initialized >= 1) {
		if ((res = i2c_del_driver(&via686a_driver))) {
			printk("via686a.o: Driver deregistration failed.\n");
			return res;
		}
		via686a_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Kyösti Mälkki <kmalkki@cc.hut.fi>, Mark Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("VIA 686A Sensor device");

int init_module(void)
{
	return sensors_via686a_init();
}

int cleanup_module(void)
{
	return via686a_cleanup();
}

#endif				/* MODULE */
