/*
    i2c-bus.c - A Linux module for reading sensor data.
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl> 

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
#include <linux/proc_fs.h>

#include "version.h"
#include "compat.h"

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */o

static int i2cbus_init(void);
static int i2cbus_cleanup(void);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,1,0))

static int read_bus_i2c(char *buf, char **start, off_t offset, int len,
                        int *eof , void *private);

static struct proc_dir_entry *proc_bus_i2c;

#else /* (LINUX_VERSION_CODE > KERNEL_VERSION(2,1,0)) */

static int read_bus_i2c(char *buf, char **start, off_t offset, int len,
                        int unused);

static struct proc_dir_entry proc_bus_dir =
  {
    /* low_ino */	0,     /* Set by proc_register_dynamic */
    /* namelen */	3, 
    /* name */		"bus",
    /* mode */		S_IRUGO | S_IXUGO | S_IFDIR,
    /* nlink */		1,     /* Corrected by proc_register[_dynamic] */
    /* uid */		0,
    /* gid */		0,
    /* size */		0,
    /* ops */		&proc_dir_inode_operations,
  };

static struct proc_dir_entry proc_bus_i2c_dir =
  {
    /* low_ino */	0,     /* Set by proc_register_dynamic */
    /* namelen */	3, 
    /* name */		"i2c",
    /* mode */		S_IRUGO | S_IFREG,
    /* nlink */		1,     
    /* uid */		0,
    /* gid */		0,
    /* size */		0,
    /* ops */		NULL,
    /* get_info */	&read_bus_i2c
  };

#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2,1,0)) */

static int i2cbus_initialized;

int i2cbus_init(void)
{
  int res;

  printk("i2c-bus.o version %s (%s)\n",LM_VERSION,LM_DATE);
  i2cbus_initialized = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,1,0))
  if (! proc_bus) {
    printk("i2c-bus.o: /proc/bus/ does not exist, module not inserted.\n");
    i2cbus_cleanup();
    return -ENOENT;
  }
  proc_bus_i2c = create_proc_entry("i2c",0,proc_bus);
  if (proc_bus_i2c)
    proc_bus_i2c->read_proc = &read_bus_i2c;
  else {
    printk("i2c-bus.o: Could not create /proc/bus/i2c, module not inserted.\n");
    i2cbus_cleanup();
    return -ENOENT;
  }
  i2cbus_initialized += 2;
#else
  /* In Linux 2.0.x, there is no /proc/bus! But I hope no other module
     introduced it, or we are fucked. */
  if ((res = proc_register_dynamic(&proc_root, &proc_bus_dir))) {
    printk("i2c-bus.o: Could not create /proc/bus/, module not inserted.\n");
    i2cbus_cleanup();
    return res;
  }
  i2cbus_initialized ++;
  if ((res = proc_register_dynamic(&proc_bus_dir, &proc_bus_i2c_dir))) {
    printk("i2c-bus.o: Could not create /proc/bus/i2c, module not inserted.\n");
    i2cbus_cleanup();
    return res;
  }
  i2cbus_initialized ++;
#endif
  return 0;
}

int i2cbus_cleanup(void)
{
  int res;

  if (i2cbus_initialized >= 1) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,1,0))
    if ((res = remove_proc_entry("i2c",proc_bus))) {
      printk("i2c-bus.o: could not delete /proc/bus/i2c, module not removed.");
      return res;
    }
    i2cbus_initialized -= 2;
#else
    if (i2cbus_initialized >= 2) {
      if ((res = proc_unregister(&proc_bus_dir,proc_bus_i2c_dir.low_ino))) {
         printk("i2c-bus.o: could not delete /proc/bus/i2c, "
                "module not removed.");
         return res;
      }    
      i2cbus_initialized --;
    }
    if ((res = proc_unregister(&proc_root,proc_bus_dir.low_ino))) {
       printk("i2c-bus.o: could not delete /proc/bus/, "
              "module not removed.");
       return res;
    }    
    i2cbus_initialized --;
#endif
  }
  return 0;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,1,0))
int read_bus_i2c(char *buf, char **start, off_t offset, int len, int *eof, 
                 void *private)
#else
int read_bus_i2c(char *buf, char **start, off_t offset, int len, int unused)
#endif
{
  len = 0;
  len += sprintf(buf,"A very nice test, indeed\n");
  return len;
}


#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("I2C /proc/bus entries driver");

int init_module(void)
{
  return i2cbus_init();
}

int cleanup_module(void)
{
  return i2cbus_cleanup();
}

#endif /* MODULE */

