/* ------------------------------------------------------------------------- */
/* i2c-dev.c - i2c-bus driver, char device interface			     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-97 Simon G. Vogl

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */
#define RCSID "$Id: i2c-dev.c,v 1.7 1998/12/30 08:36:08 i2c Exp i2c $"
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/malloc.h>

#if LINUX_VERSION_CODE >= 0x020100
#  include <asm/uaccess.h>
#else
#  include <asm/segment.h>
#endif

#include "i2c.h"
 
/* ----- global defines ---------------------------------------------------- */
#define DEB(x)		/* should be reasonable open, close &c. 	*/
#define DEBE(x)	x	/* error messages 			~	*/
#define DEBI(x) 	/* ioctl and its arguments 			*/

/* ----- global variables -------------------------------------------------- */

/* save the registered char devices.
 * there is one device per i2c adapter.
 */
#define I2C_DEV_MAX	I2C_ADAP_MAX
struct i2c_client *devs[I2C_DEV_MAX];
int dev_count;

struct i2c_driver driver;
struct i2c_client dev_template = {
	"I2C char device",		/* name				*/
	-1,				/* id				*/
	0,				/* flags			*/
	0,				/* address			*/
	NULL,				/* adapter			*/
	&driver,			/* driver			*/
};


/* ----- local functions --------------------------------------------------- */

#if LINUX_VERSION_CODE >= 0x020100
long long i2c_lseek(struct file * file, long long offset, int origin) 
#else
int i2c_lseek (struct inode * inode, struct file *file, off_t offset, int origin)
#endif
{
	return -ESPIPE;	
}


int i2c_open(struct inode * inode, struct file * file) 
{
	unsigned int minor = MINOR(inode->i_rdev);

	if (minor >= dev_count ) {
		DEBE(printk("i2c: minor exceeded ports\n"));
		return -ENODEV;
	}
	file->private_data = devs[minor];
	MOD_INC_USE_COUNT;
	i2c_attach_client(devs[minor]);	/* tell adapter it is in use.	*/

	DEB(printk("i2c_open: i2c%d\n",minor));
	return 0;
}

#if LINUX_VERSION_CODE >= 0x020100
int i2c_release (struct inode * inode, struct file * file)
#else
void i2c_release (struct inode * inode, struct file * file)
#endif
{
	unsigned int minor = MINOR(inode->i_rdev);

	file->private_data=NULL;
	DEB(printk("i2c_close: i2c%d\n",minor));
	i2c_detach_client(devs[minor]);
	MOD_DEC_USE_COUNT;
#if LINUX_VERSION_CODE >= 0x020100
	return 0;
#endif
}

#if LINUX_VERSION_CODE >= 0x020140	/* file operations changed again...*/
ssize_t i2c_write(struct file * file, 
	const char * buf, size_t count, loff_t *ppos)
#else
#  if LINUX_VERSION_CODE >= 0x020100
long i2c_write(struct inode * inode, struct file * file,
	const char * buf, unsigned long count)
#  else
int i2c_write (struct inode *inode, struct file *file, 
	const char *buf, int count)
#  endif
#endif
{
#if LINUX_VERSION_CODE >= 0x020140
	struct inode *inode = file->f_dentry->d_inode;
#endif
	int ret;
	char *tmp;
	unsigned int minor = MINOR(inode->i_rdev);
	struct i2c_client *client = 
		(struct i2c_client *)file->private_data;

	if (minor>=dev_count){
		DEBE(printk("i2c_write: minor %d invalid\n",minor));
		return -EINVAL;
	}

	/* copy user space data to kernel space. */
	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
#if LINUX_VERSION_CODE >= 0x020100
	copy_from_user(tmp,buf,count);
#else
	memcpy_fromfs(tmp,buf,count);
#endif

	DEB(printk("i2c_write: i2c%d writing %d bytes.\n",minor,count));

	ret = i2c_master_send(client,tmp,count);
	kfree(tmp);
	return ret;
}

#if LINUX_VERSION_CODE >= 0x020140	/* file operations changed again...*/
ssize_t i2c_read(struct file * file,char * buf, size_t count, loff_t *ppos) 
#else
#if LINUX_VERSION_CODE >= 0x020100
long i2c_read(struct inode * inode, struct file * file,
	char * buf, unsigned long count) 
#else
int i2c_read (struct inode *inode, struct file *file, char *buf, int count)
#endif
#endif
{
#if LINUX_VERSION_CODE >= 0x020140
	struct inode *inode = file->f_dentry->d_inode;
#endif
	unsigned int minor = MINOR(inode->i_rdev);
	struct i2c_client *client = 
		(struct i2c_client *)file->private_data;
	char *tmp;
	int ret;
	
	DEB(printk("i2c_read: i2c%d reading %d bytes from %d.\n",minor,count,client->addr));
       	if (minor>=dev_count) {
		DEBE(printk("i2c_write: minor %d invalid\n",minor));
		return -EINVAL;
	}

	/* copy user space data to kernel space. */
	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	ret = i2c_master_recv(client,tmp,count);
#if LINUX_VERSION_CODE >= 0x020100
	copy_to_user(buf,tmp,count);
#else
	memcpy_tofs(buf,tmp,count);
#endif
	kfree(tmp);
	return ret;
}


int i2c_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	unsigned int minor = MINOR(inode->i_rdev);
	struct i2c_client *client = 
		(struct i2c_client *)file->private_data;
	int ret = 0;

	DEBI(printk("i2c ioctl, cmd: 0x%x, arg: %#lx\n", cmd, arg));
	switch ( cmd ) {
		case I2C_SLAVE:
			client->addr = arg;
			break;
		case I2C_TENBIT:
			client->flags &= !I2C_M_TENMASK;
			if (arg<0)
				return 0;
			switch(arg) {
			case 0:
				client->flags|=I2C_M_TEN0;
				break;
			case 1:
				client->flags|=I2C_M_TEN1;
				break;
			case 2:
				client->flags|=I2C_M_TEN2;
				break;
			case 3:
				client->flags|=I2C_M_TEN3;
				break;
			default:
				printk("i2c-dev(%d): illegal arg to I2C_TEN: %ld\n"
					,minor,arg);
			};
			break;
#if 0
		case I2C_WRITE_SIZE:
			if ( arg >= I2C_BUFFER_SIZE ) {
				printk("i2c%d: write size too big (%ld)",minor,arg);
				return -E2BIG;
			}
			data->writelength = arg;
			if ( arg > 0 )
				i2c_table[minor].flags|= P_REG;
			else
				i2c_table[minor].flags&=!P_REG;
			break;
		case I2C_WRITE_BUF:
#if LINUX_VERSION_CODE >= 0x020100
			copy_from_user(data->buf,(char*)arg,data->writelength);
#else
			memcpy_fromfs(data->buf,(char*)arg,data->writelength);
#endif
			break;
#endif
		default:
			ret = i2c_control(client,cmd,arg);
	}
	DEB(printk("i2c(dev): handled ioctl (%d,%ld)\n",cmd,arg));
	return ret;
}


/* ----- module functions ---------------------------------------------	*/
struct file_operations i2c_fops = {
	i2c_lseek,
	i2c_read,
	i2c_write,
	NULL,  			/* i2c_readdir	*/
	NULL,			/* i2c_select 	*/
	i2c_ioctl,
	NULL,			/* i2c_mmap 	*/
	i2c_open,
#if LINUX_VERSION_CODE >= 0x020178
	NULL,			/* i2c_flush	*/
#endif
	i2c_release,
};

int attach_adapter(struct i2c_adapter *adap)
{
	int i;
	struct i2c_client *client;

	for (i = 0; i < I2C_DEV_MAX; i++)
		if (NULL == devs[i])
			break;
	if (I2C_DEV_MAX == i)
		return -ENOMEM;

	client = (struct i2c_client *)kmalloc(sizeof(struct i2c_client),
		GFP_KERNEL);
	if (client==NULL)
		return -ENOMEM;
	memcpy(client,&dev_template,sizeof(struct i2c_client));
	client->adapter = adap;
/*	for combined r/w:
	client->data = kmalloc(sizeof(char)*I2C_BUFFER_SIZE,GFP_KERNEL);
*/
	devs[i] = client;
	dev_count++;

	DEB(printk("i2c(char): registered '%s' as minor %d\n",adap->name,i));
	return 0;
}

int detach_adapter(struct i2c_client *client)
{
	struct i2c_adapter *adap = client->adapter;
	int i;
	for (i = 0; i < I2C_DEV_MAX; i++)
		if (devs[i]->adapter == adap)
			break;
	if (I2C_DEV_MAX == i) 
	{
		printk(KERN_WARNING "i2c(char): detach adapter %s not found\n",
			adap->name);
		return -ENODEV;
	}
	if (devs[i]->data)
		kfree(devs[i]->data);
	kfree(devs[i]);
	devs[i] = NULL;
	dev_count--;

	DEB(printk("i2c(char): adapter unregistered: %s\n",adap->name));
	return 0;
}

int command(struct i2c_client *client,unsigned int cmd, void *arg)
{
	return -1;
}



struct i2c_driver driver = {
	"i2c character device",
	2,				/* id 				*/
	DF_NOTIFY,			/* flags			*/
	attach_adapter,
	detach_adapter,
	command,
};

#ifdef MODULE
MODULE_AUTHOR("Simon G. Vogl <simon@tk.uni-linz.ac.at>");
MODULE_DESCRIPTION("I2C-Bus character device interface");

#ifndef LM_SENSORS
EXPORT_NO_SYMBOLS;
#endif

int init_module(void) 
{
	if (register_chrdev(I2C_MAJOR,"i2c",&i2c_fops)) {
		printk("i2c: unable to get major %d for i2c bus\n",I2C_MAJOR);
		return -EIO;
	}

	memset(&devs,0,sizeof(devs));
	dev_count = 0;

	i2c_add_driver(&driver);
	printk("i2c char device interface initialized.\n");
	return 0;
}


void cleanup_module(void) 
{
	i2c_del_driver(&driver);
	unregister_chrdev(I2C_MAJOR,"i2c");
}

#endif
