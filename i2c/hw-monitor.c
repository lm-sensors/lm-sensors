 /*
    EISCA Fan SMBus (or i2c) driver
    Based on various char-device sources.

    Copyright (c) 1998 Kyösti Mälkki 

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
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/malloc.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>

#include "i2c.h"
#include "algo-bit.h"

/* Fan defines */
#define I2C_GL518SM	0x2d      /* 7-bit i2c slave address */
#define REGMASK 0x1f
#define PROCNAME        "temperature"

/* i2c */
static struct i2c_driver driver;
static struct i2c_client client_template;
static struct i2c_client *client = &client_template;

/* ---------------------------------------------------------------------- */

static unsigned int gl_sysmon_read(int reg, int count)
{
	struct i2c_msg msgs[2];
	char write[1]={0};
	char read[2]={0,0};
	int ret;
	unsigned int val;
	
	msgs[0].addr = msgs[1].addr = client->addr;
	msgs[0].flags = msgs[1].flags =
		(client->flags & (I2C_M_TEN|I2C_M_TENMASK));
	
	msgs[0].len = 1;
	msgs[0].buf = write;
	write[0] = reg & REGMASK;
	
	msgs[1].flags |= I2C_M_RD;
	msgs[1].len = count;
	msgs[1].buf = read;

	ret = i2c_transfer(client->adapter, msgs, 2);
	
	if ( 2 != ret ) return ret;
	if (count==1)
		val = read[0] & 0xff;
	else
		val = ( (read[0]<<8)|(read[1] & 0xff) ) & 0xffff;

	return val;
}

static int gl_sysmon_write(int reg, unsigned int val, int count)
{
	unsigned char buffer[5];

	buffer[0] = reg & REGMASK;
	if (count==2) {
		buffer[1] = (val >> 8) & 0xff;
		buffer[2] = val & 0xff;
		count = 3;
	} else {
		buffer[1] = val & 0xff;
		count = 2;
	}
	
	if (count != i2c_master_send(client, buffer, count))
		return -1;
	return 0;
}

static int gl_sysmon_info(char *buf, char **start, off_t fpos, int length, int dummy)
{
	char * p=buf;
	int i;
	unsigned int regs[32];

	for (i=0; i<32; i++) {
		regs[i] = 0;
		regs[i] = gl_sysmon_read(i, (i>6 && i<13) ? 2 : 1);
	}
	
	p += sprintf(buf, "CPU temperature %d Celsius\n", (regs[4]-119)); 

	return p - buf;
}

/* ----------------------------------------------------------------------- */

static struct proc_dir_entry *ent;

static int gl_sysmon_attach(struct i2c_adapter *adap)
{
	client->adapter = adap;

	i2c_attach_client(client);
	gl_sysmon_write(3, 64, 1);	
	
	ent = create_proc_entry(PROCNAME, 0, 0);
	ent->get_info = gl_sysmon_info;

	printk("EISCA Fan connected\n");
	return 0;
}

static int gl_sysmon_detach(struct i2c_client *client)
{
	gl_sysmon_write(3, 128, 1);
	i2c_detach_client(client);
	remove_proc_entry(PROCNAME, 0);
	
	printk("EISCA Fan disconnected\n");
	return 0;
}

static int
gl_sysmon_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return -1;
}

/* ----------------------------------------------------------------------- */

static struct i2c_driver driver = {
	"GL518SM",
	-1,
	DF_NOTIFY,
	gl_sysmon_attach,
	gl_sysmon_detach,
	gl_sysmon_command,
};

static struct i2c_client client_template = {
	"EISCA Cooler",
	I2C_DRIVERID_GL518SM,
	0,
	I2C_GL518SM,
	NULL,
	&driver
};

#ifdef MODULE
MODULE_AUTHOR("Kyosti Malkki <kmalkki@cc.hut.fi>");
MODULE_DESCRIPTION("i2c driver for GL518SM");

int init_module(void)
{
	i2c_add_driver(&driver);
	return 0;
}

void cleanup_module(void)
{
	i2c_del_driver(&driver);
}
#endif
