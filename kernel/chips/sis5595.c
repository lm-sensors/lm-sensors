/*
    sis5595.c - Part of lm_sensors, Linux kernel modules
                for hardware monitoring
                
    Copyright (c) 1998 - 2001 Frodo Looijaard <frodol@dds.nl>,
                        Kyösti Mälkki <kmalkki@cc.hut.fi>, and
			Mark D. Studebaker <mdsxyz123@yahoo.com>

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

/* 
   SiS southbridge has a LM78-like chip integrated on the same IC.
   This driver is a customized copy of lm78.c

    Supports following revisions:
	Version		PCI ID		PCI Revision
	1		1039/0008	AF or less
	2		1039/0008	B0 or greater

   Note: these chips contain a 0008 device which is incompatible with the
         5595. We recognize these by the presence of the listed
         "blacklist" PCI ID and refuse to load.

   NOT SUPPORTED	PCI ID		BLACKLIST PCI ID	
	 540		0008		0540
	 550		0008		0550
	5513		0008		5511
	5581		0008		5597
	5582		0008		5597
	5597		0008		5597
	5598		0008		5597/5598
	 630		0008		0630
	 645		0008		0645
	 730		0008		0730
	 735		0008		0735
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/i2c-proc.h>
#include <linux/init.h>
#include <asm/io.h>
#include "version.h"

MODULE_LICENSE("GPL");

/* If force_addr is set to anything different from 0, we forcibly enable
   the device at the given address. */
static int force_addr = 0;
MODULE_PARM(force_addr, "i");
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the sensors");

/* Addresses to scan.
   Note that we can't determine the ISA address until we have initialized
   our module */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0x0000, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(sis5595);

static int blacklist[] = {
			PCI_DEVICE_ID_SI_540,
			PCI_DEVICE_ID_SI_550,
			PCI_DEVICE_ID_SI_630,
			PCI_DEVICE_ID_SI_645,
			PCI_DEVICE_ID_SI_730,
			PCI_DEVICE_ID_SI_735,
			PCI_DEVICE_ID_SI_5511, /* 5513 chip has the 0008 device but
						  that ID shows up in other chips so we
						  use the 5511 ID for recognition */
			PCI_DEVICE_ID_SI_5597,
			PCI_DEVICE_ID_SI_5598,
                          0 };

/* Many SIS5595 constants specified below */

/* Length of ISA address segment */
#define SIS5595_EXTENT 8
/* PCI Config Registers */
#define SIS5595_BASE_REG 0x68
#define SIS5595_PIN_REG 0x7A
#define SIS5595_ENABLE_REG 0x7B

/* Where are the ISA address/data registers relative to the base address */
#define SIS5595_ADDR_REG_OFFSET 5
#define SIS5595_DATA_REG_OFFSET 6

/* The SIS5595 registers */
#define SIS5595_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define SIS5595_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define SIS5595_REG_IN(nr) (0x20 + (nr))

#define SIS5595_REG_FAN_MIN(nr) (0x3a + (nr))
#define SIS5595_REG_FAN(nr) (0x27 + (nr))

/* On the first version of the chip, the temp registers are separate.
   On the second version,
   TEMP pin is shared with IN4, configured in PCI register 0x7A.
   The registers are the same as well.
   OVER and HYST are really MAX and MIN. */

#define REV2MIN	0xb0
#define SIS5595_REG_TEMP 	(( data->revision) >= REV2MIN) ? \
					SIS5595_REG_IN(4) : 0x27
#define SIS5595_REG_TEMP_OVER	(( data->revision) >= REV2MIN) ? \
					SIS5595_REG_IN_MAX(4) : 0x39
#define SIS5595_REG_TEMP_HYST	(( data->revision) >= REV2MIN) ? \
					SIS5595_REG_IN_MIN(4) : 0x3a

#define SIS5595_REG_CONFIG 0x40
#define SIS5595_REG_ALARM1 0x41
#define SIS5595_REG_ALARM2 0x42
#define SIS5595_REG_FANDIV 0x47

/* Conversions. Limit checking is only done on the TO_REG
   variants. Note that you should be a bit careful with which arguments
   these macros are called: arguments may be evaluated more than once.
   Fixing this is just not worth it. */

#define IN_TO_REG(val)  (SENSORS_LIMIT((((val) * 10 + 8)/16),0,255))
#define IN_FROM_REG(val) (((val) *  16 + 5) / 10)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm <= 0)
		return 255;
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1,
			     254);
}

#define FAN_FROM_REG(val,div) ((val)==0?-1:(val)==255?0:1350000/((val)*(div)))

/* Version 1 datasheet temp=.83*reg + 52.12 */
#define TEMP_FROM_REG(val) (((((val)>=0x80?(val)-0x100:(val))*83)+5212)/10)
/* inverse 1.20*val - 62.77 */
#define TEMP_TO_REG(val) (SENSORS_LIMIT(((val)<0?\
				((((val)*12)-6327)/100):\
                                ((((val)*12)-6227)/100)),0,255))

#define DIV_FROM_REG(val) (1 << (val))
#define DIV_TO_REG(val) ((val)==8?3:(val)==4?2:(val)==1?0:1)

/* For the SIS5595, we need to keep some data in memory. It is
   dynamically allocated, at the time when the new sis5595 client is
   allocated. */
struct sis5595_data {
	struct i2c_client client;
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */
	char maxins;		/* == 3 if temp enabled, otherwise == 4 */
	u8 revision;		/* Reg. value */

	u8 in[5];		/* Register value */
	u8 in_max[5];		/* Register value */
	u8 in_min[5];		/* Register value */
	u8 fan[2];		/* Register value */
	u8 fan_min[2];		/* Register value */
	u8 temp;		/* Register value */
	u8 temp_over;		/* Register value */
	u8 temp_hyst;		/* Register value */
	u8 fan_div[2];		/* Register encoding, shifted right */
	u16 alarms;		/* Register encoding, combined */
};

static struct pci_dev *s_bridge;	/* pointer to the (only) sis5595 */

static int sis5595_attach_adapter(struct i2c_adapter *adapter);
static int sis5595_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int sis5595_detach_client(struct i2c_client *client);

static int sis5595_read_value(struct i2c_client *client, u8 reg);
static int sis5595_write_value(struct i2c_client *client, u8 reg,
			       u8 value);
static void sis5595_update_client(struct i2c_client *client);
static void sis5595_init_client(struct i2c_client *client);


static void sis5595_in(struct i2c_client *client, int operation,
		       int ctl_name, int *nrels_mag, long *results);
static void sis5595_fan(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void sis5595_temp(struct i2c_client *client, int operation,
			 int ctl_name, int *nrels_mag, long *results);
static void sis5595_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void sis5595_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);

/* The driver. I choose to use type i2c_driver, as at is identical to both
   smbus_driver and isa_driver, and clients could be of either kind */
static struct i2c_driver sis5595_driver = {
	.name		= "SiS 5595",
	.id		= I2C_DRIVERID_SIS5595,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= sis5595_attach_adapter,
	.detach_client	= sis5595_detach_client,
};

/* The /proc/sys entries */

/* -- SENSORS SYSCTL START -- */
#define SIS5595_SYSCTL_IN0 1000	/* Volts * 100 */
#define SIS5595_SYSCTL_IN1 1001
#define SIS5595_SYSCTL_IN2 1002
#define SIS5595_SYSCTL_IN3 1003
#define SIS5595_SYSCTL_IN4 1004
#define SIS5595_SYSCTL_FAN1 1101	/* Rotations/min */
#define SIS5595_SYSCTL_FAN2 1102
#define SIS5595_SYSCTL_TEMP 1200	/* Degrees Celsius * 10 */
#define SIS5595_SYSCTL_FAN_DIV 2000	/* 1, 2, 4 or 8 */
#define SIS5595_SYSCTL_ALARMS 2001	/* bitvector */

#define SIS5595_ALARM_IN0 0x01
#define SIS5595_ALARM_IN1 0x02
#define SIS5595_ALARM_IN2 0x04
#define SIS5595_ALARM_IN3 0x08
#define SIS5595_ALARM_BTI 0x20
#define SIS5595_ALARM_FAN1 0x40
#define SIS5595_ALARM_FAN2 0x80
#define SIS5595_ALARM_IN4  0x8000
#define SIS5595_ALARM_TEMP 0x8000

/* -- SENSORS SYSCTL END -- */

/* These files are created for each detected SIS5595. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized 
   when a new copy is allocated. */
static ctl_table sis5595_dir_table_template[] = {
	{SIS5595_SYSCTL_IN0, "in0", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_in},
	{SIS5595_SYSCTL_IN1, "in1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_in},
	{SIS5595_SYSCTL_IN2, "in2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_in},
	{SIS5595_SYSCTL_IN3, "in3", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_in},
	{SIS5595_SYSCTL_IN4, "in4", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_in},
	{SIS5595_SYSCTL_FAN1, "fan1", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_fan},
	{SIS5595_SYSCTL_FAN2, "fan2", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_fan},
	{SIS5595_SYSCTL_TEMP, "temp", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_temp},
	{SIS5595_SYSCTL_FAN_DIV, "fan_div", NULL, 0, 0644, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_fan_div},
	{SIS5595_SYSCTL_ALARMS, "alarms", NULL, 0, 0444, NULL, &i2c_proc_real,
	 &i2c_sysctl_real, NULL, &sis5595_alarms},
	{0}
};

/* This is called when the module is loaded */
static int sis5595_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, sis5595_detect);
}

/* Locate SiS bridge and correct base address for SIS5595 */
static int __init sis5595_find_sis(int *address)
{
	u16 val;
	int *i;

	if (!pci_present())
		return -ENODEV;

	if (!(s_bridge =
	      pci_find_device(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_503,
			     NULL)))
		return -ENODEV;

	/* Look for imposters */
	for(i = blacklist; *i != 0; i++) {
		if (pci_find_device(PCI_VENDOR_ID_SI, *i, NULL)) {
			printk("sis5595.o: Error: Looked for SIS5595 but found unsupported device %.4X\n", *i);
			return -ENODEV;
		}
	}

	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_word(s_bridge, SIS5595_BASE_REG, &val))
		return -ENODEV;

	*address = val & ~(SIS5595_EXTENT - 1);
	if (*address == 0 && force_addr == 0) {
		printk("sis5595.o: base address not set - upgrade BIOS or use force_addr=0xaddr\n");
		return -ENODEV;
	}
	if (force_addr)
		*address = force_addr;	/* so detect will get called */

	return 0;
}

int sis5595_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct sis5595_data *data;
	int err = 0;
	const char *type_name = "sis5595";
	const char *client_name = "SIS5595 chip";
	char val;
	u16 a;

	/* Make sure we are probing the ISA bus!!  */
	if (!i2c_is_isa_adapter(adapter)) {
		printk
		    ("sis5595.o: sis5595_detect called for an I2C bus adapter?!?\n");
		return 0;
	}

	if(force_addr)
		address = force_addr & ~(SIS5595_EXTENT - 1);
	if (check_region(address, SIS5595_EXTENT)) {
		printk("sis5595.o: region 0x%x already in use!\n", address);
		return -ENODEV;
	}
	if(force_addr) {
		printk("sis5595.o: forcing ISA address 0x%04X\n", address);
		if (PCIBIOS_SUCCESSFUL !=
		    pci_write_config_word(s_bridge, SIS5595_BASE_REG, address))
			return -ENODEV;
		if (PCIBIOS_SUCCESSFUL !=
		    pci_read_config_word(s_bridge, SIS5595_BASE_REG, &a))
			return -ENODEV;
		if ((a & ~(SIS5595_EXTENT - 1)) != address) {
			/* doesn't work for some chips? */
			printk("sis5595.o: force address failed\n");
			return -ENODEV;
		}
	}

	if (PCIBIOS_SUCCESSFUL !=
	    pci_read_config_byte(s_bridge, SIS5595_ENABLE_REG, &val))
		return -ENODEV;
	if((val & 0x80) == 0) {
		printk("sis5595.o: enabling sensors\n");
		if (PCIBIOS_SUCCESSFUL !=
		    pci_write_config_byte(s_bridge, SIS5595_ENABLE_REG,
		                      val | 0x80))
			return -ENODEV;
		if (PCIBIOS_SUCCESSFUL !=
		    pci_read_config_byte(s_bridge, SIS5595_ENABLE_REG, &val))
			return -ENODEV;
		if((val & 0x80) == 0) {	/* doesn't work for some chips! */
			printk("sis5595.o: sensors enable failed - not supported?\n");
			return -ENODEV;
		}
	}

	if (!(data = kmalloc(sizeof(struct sis5595_data), GFP_KERNEL))) {
		return -ENOMEM;
	}

	new_client = &data->client;
	new_client->addr = address;
	init_MUTEX(&data->lock);
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &sis5595_driver;
	new_client->flags = 0;

	/* Reserve the ISA region */
	request_region(address, SIS5595_EXTENT, type_name);

	/* Check revision and pin registers to determine whether 4 or 5 voltages */
	pci_read_config_byte(s_bridge, PCI_REVISION_ID, &(data->revision));
	/* 4 voltages, 1 temp */
	data->maxins = 3;
	if (data->revision >= REV2MIN) {
		pci_read_config_byte(s_bridge, SIS5595_PIN_REG, &val);
		if (!(val & 0x80))
			/* 5 voltages, no temp */
			data->maxins = 4;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry((struct i2c_client *) new_client,
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

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
	release_region(address, SIS5595_EXTENT);
	kfree(data);
	return err;
}

static int sis5595_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct sis5595_data *) (client->data))->
				 sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    ("sis5595.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	release_region(client->addr, SIS5595_EXTENT);
	kfree(client->data);

	return 0;
}


/* ISA access must be locked explicitly.
   There are some ugly typecasts here, but the good news is - they should
   nowhere else be necessary! */
static int sis5595_read_value(struct i2c_client *client, u8 reg)
{
	int res;

	down(&(((struct sis5595_data *) (client->data))->lock));
	outb_p(reg, client->addr + SIS5595_ADDR_REG_OFFSET);
	res = inb_p(client->addr + SIS5595_DATA_REG_OFFSET);
	up(&(((struct sis5595_data *) (client->data))->lock));
	return res;
}

static int sis5595_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	down(&(((struct sis5595_data *) (client->data))->lock));
	outb_p(reg, client->addr + SIS5595_ADDR_REG_OFFSET);
	outb_p(value, client->addr + SIS5595_DATA_REG_OFFSET);
	up(&(((struct sis5595_data *) (client->data))->lock));
	return 0;
}

/* Called when we have found a new SIS5595. */
static void sis5595_init_client(struct i2c_client *client)
{
	u8 reg;

	/* Start monitoring */
	reg = i2c_smbus_read_byte_data(client, SIS5595_REG_CONFIG);
	sis5595_write_value(client, SIS5595_REG_CONFIG, (reg|0x01)&0x7F);
}

static void sis5595_update_client(struct i2c_client *client)
{
	struct sis5595_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

		for (i = 0; i <= data->maxins; i++) {
			data->in[i] =
			    sis5595_read_value(client, SIS5595_REG_IN(i));
			data->in_min[i] =
			    sis5595_read_value(client,
					       SIS5595_REG_IN_MIN(i));
			data->in_max[i] =
			    sis5595_read_value(client,
					       SIS5595_REG_IN_MAX(i));
		}
		for (i = 1; i <= 2; i++) {
			data->fan[i - 1] =
			    sis5595_read_value(client, SIS5595_REG_FAN(i));
			data->fan_min[i - 1] =
			    sis5595_read_value(client,
					       SIS5595_REG_FAN_MIN(i));
		}
		if(data->maxins == 3) {
			data->temp =
			    sis5595_read_value(client, SIS5595_REG_TEMP);
			data->temp_over =
			    sis5595_read_value(client, SIS5595_REG_TEMP_OVER);
			data->temp_hyst =
			    sis5595_read_value(client, SIS5595_REG_TEMP_HYST);
		}
		i = sis5595_read_value(client, SIS5595_REG_FANDIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = i >> 6;
		data->alarms =
		    sis5595_read_value(client, SIS5595_REG_ALARM1) |
		    (sis5595_read_value(client, SIS5595_REG_ALARM2) << 8);
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

/* Return 0 for in4 and disallow writes if pin used for temp */
void sis5595_in(struct i2c_client *client, int operation, int ctl_name,
		int *nrels_mag, long *results)
{
	struct sis5595_data *data = client->data;
	int nr = ctl_name - SIS5595_SYSCTL_IN0;

	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 2;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if(nr <= 3 || data->maxins == 4) {
			sis5595_update_client(client);
			results[0] = IN_FROM_REG(data->in_min[nr]);
			results[1] = IN_FROM_REG(data->in_max[nr]);
			results[2] = IN_FROM_REG(data->in[nr]);
		} else {
			results[0] = 0;
			results[1] = 0;
			results[2] = 0;
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if(nr <= 3 || data->maxins == 4) {
			if (*nrels_mag >= 1) {
				data->in_min[nr] = IN_TO_REG(results[0]);
				sis5595_write_value(client,
				    SIS5595_REG_IN_MIN(nr), data->in_min[nr]);
			}
			if (*nrels_mag >= 2) {
				data->in_max[nr] = IN_TO_REG(results[1]);
				sis5595_write_value(client,
				    SIS5595_REG_IN_MAX(nr), data->in_max[nr]);
			}
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
		results[0] = FAN_FROM_REG(data->fan_min[nr - 1],
					  DIV_FROM_REG(data->fan_div[nr - 1]));
		results[1] = FAN_FROM_REG(data->fan[nr - 1],
					  DIV_FROM_REG(data->fan_div[nr - 1]));
		*nrels_mag = 2;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
			data->fan_min[nr - 1] = FAN_TO_REG(results[0],
							   DIV_FROM_REG
							   (data->
							    fan_div[nr-1]));
			sis5595_write_value(client,
					    SIS5595_REG_FAN_MIN(nr),
					    data->fan_min[nr - 1]);
		}
	}
}


/* Return 0 for temp and disallow writes if pin used for in4 */
void sis5595_temp(struct i2c_client *client, int operation, int ctl_name,
		  int *nrels_mag, long *results)
{
	struct sis5595_data *data = client->data;
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 1;
	else if (operation == SENSORS_PROC_REAL_READ) {
		if(data->maxins == 3) {
			sis5595_update_client(client);
			results[0] = TEMP_FROM_REG(data->temp_over);
			results[1] = TEMP_FROM_REG(data->temp_hyst);
			results[2] = TEMP_FROM_REG(data->temp);
		} else {
			results[0] = 0;
			results[1] = 0;
			results[2] = 0;
		}
		*nrels_mag = 3;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if(data->maxins == 3) {
			if (*nrels_mag >= 1) {
				data->temp_over = TEMP_TO_REG(results[0]);
				sis5595_write_value(client,
				    SIS5595_REG_TEMP_OVER, data->temp_over);
			}
			if (*nrels_mag >= 2) {
				data->temp_hyst = TEMP_TO_REG(results[1]);
				sis5595_write_value(client,
				    SIS5595_REG_TEMP_HYST, data->temp_hyst);
			}
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
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
}

void sis5595_fan_div(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
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
		old = sis5595_read_value(client, SIS5595_REG_FANDIV);
		if (*nrels_mag >= 2) {
			data->fan_div[1] = DIV_TO_REG(results[1]);
			old = (old & 0x3f) | (data->fan_div[1] << 6);
		}
		if (*nrels_mag >= 1) {
			data->fan_div[0] = DIV_TO_REG(results[0]);
			old = (old & 0xcf) | (data->fan_div[0] << 4);
			sis5595_write_value(client, SIS5595_REG_FANDIV, old);
		}
	}
}

static int __init sm_sis5595_init(void)
{
	int addr;

	printk("sis5595.o version %s (%s)\n", LM_VERSION, LM_DATE);

	if (sis5595_find_sis(&addr)) {
		printk("sis5595.o: SIS5595 not detected, module not inserted.\n");
		return -ENODEV;
	}
	normal_isa[0] = addr;

	return i2c_add_driver(&sis5595_driver);
}

static void __exit sm_sis5595_exit(void)
{
	i2c_del_driver(&sis5595_driver);
}



MODULE_AUTHOR("Kyösti Mälkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("SiS 5595 Sensor device");

module_init(sm_sis5595_init);
module_exit(sm_sis5595_exit);
