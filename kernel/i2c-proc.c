/*
    i2c-proc.c - Part of lm_sensors, Linux kernel modules for hardware
                 monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> 

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

#include "i2c.h"
#include "smbus.h"
#include "i2c-isa.h"
#include "version.h"
#include "compat.h"
#include "sensors.h"

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* def MODULE */

static int i2cproc_init(void);
static int i2cproc_cleanup(void);
static int i2cproc_attach_adapter(struct i2c_adapter *adapter);
static int i2cproc_detach_client(struct i2c_client *client);
static int i2cproc_command(struct i2c_client *client, unsigned int cmd,
                           void *arg);
static void i2cproc_inc_use(struct i2c_client *client);
static void i2cproc_dec_use(struct i2c_client *client);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
static void monitor_bus_i2c(struct inode *inode, int fill);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))

static ssize_t i2cproc_bus_read(struct file * file, char * buf,size_t count, 
                                loff_t *ppos);
static int read_bus_i2c(char *buf, char **start, off_t offset, int len,
                           int *eof , void *private);

#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,29)) */

static int i2cproc_bus_read(struct inode * inode, struct file * file,
                            char * buf, int count);
static int read_bus_i2c(char *buf, char **start, off_t offset, int len,
                        int unused);

static struct proc_dir_entry proc_bus_dir =
  {
    /* low_ino */	0,     /* Set by proc_register_dynamic */
    /* namelen */	3, 
    /* name */		"bus",
    /* mode */		S_IRUGO | S_IXUGO | S_IFDIR,
    /* nlink */		2,     /* Corrected by proc_register[_dynamic] */
    /* uid */		0,
    /* gid */		0,
    /* size */		0,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,0,36))
    /* ops */		&proc_dir_inode_operations, 
#endif
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

/* List of registered entries in /proc/bus */
static struct proc_dir_entry *i2cproc_proc_entries[I2C_ADAP_MAX];

#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */

/* To implement the dynamic /proc/bus/i2c-? files, we need our own 
   implementation of the read hook */
static struct file_operations i2cproc_operations = {
        NULL,
        i2cproc_bus_read,
};

static struct inode_operations i2cproc_inode_operations = {
        &i2cproc_operations
};


/* Used by init/cleanup */
static int i2cproc_initialized;

/* This is a sorted list of all adapters that will have entries in /proc/bus */
static struct i2c_adapter *i2cproc_adapters[I2C_ADAP_MAX];

/* Inodes of /dev/bus/i2c-? files */
static int i2cproc_inodes[I2C_ADAP_MAX];

/* We will use a nasty trick: we register a driver, that will be notified
   for each adapter. Then, we register a dummy client on the adapter, that
   will get notified if the adapter is removed. This is the same trick as
   used in i2c/i2c-dev.c */
static struct i2c_driver i2cproc_driver = {
  /* name */		"i2c-proc dummy driver",
  /* id */ 		I2C_DRIVERID_I2CPROC,
  /* flags */		DF_NOTIFY,
  /* attach_adapter */	&i2cproc_attach_adapter,
  /* detach_client */   &i2cproc_detach_client,
  /* command */		&i2cproc_command,
  /* inc_use */		&i2cproc_inc_use,
  /* dec_use */		&i2cproc_dec_use
};

static struct i2c_client i2cproc_client_template = {
  /* name */		"i2c-proc dummy client",
  /* id */		1,
  /* flags */		0,
  /* addr */		-1,
  /* adapter */		NULL,
  /* driver */		&i2cproc_driver,
  /* data */		NULL
};


int i2cproc_init(void)
{
  int res;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
  struct proc_dir_entry *proc_bus_i2c;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */

  printk("i2c-proc.o version %s (%s)\n",LM_VERSION,LM_DATE);
  i2cproc_initialized = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
  if (! proc_bus) {
    printk("i2c-proc.o: /proc/bus/ does not exist, module not inserted.\n");
    i2cproc_cleanup();
    return -ENOENT;
  }
  proc_bus_i2c = create_proc_entry("i2c",0,proc_bus);
  if (!proc_bus_i2c) {
    printk("i2c-proc.o: Could not create /proc/bus/i2c, "
           "module not inserted.\n");
    i2cproc_cleanup();
    return -ENOENT;
  }
  proc_bus_i2c->read_proc = &read_bus_i2c;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
  proc_bus_i2c->fill_inode = &monitor_bus_i2c;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */
  i2cproc_initialized += 2;
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,29)) */
  /* In Linux 2.0.x, there is no /proc/bus! But I hope no other module
     introduced it, or we are fucked. And 2.0.35 and earlier does not
     export proc_dir_inode_operations, so we grab it from proc_net,
     which also uses it. Not nice. */
/* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,0,36) */
  proc_bus_dir.ops = proc_net.ops;
/* #endif */
  if ((res = proc_register_dynamic(&proc_root, &proc_bus_dir))) {
    printk("i2c-proc.o: Could not create /proc/bus/, module not inserted.\n");
    i2cproc_cleanup();
    return res;
  }
  i2cproc_initialized ++;
  if ((res = proc_register_dynamic(&proc_bus_dir, &proc_bus_i2c_dir))) {
    printk("i2c-proc.o: Could not create /proc/bus/i2c, "
           "module not inserted.\n");
    i2cproc_cleanup();
    return res;
  }
  i2cproc_initialized ++;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */
  if ((res = i2c_add_driver(&i2cproc_driver))) {
    printk("i2c-proc.o: Driver registration failed, module not inserted.\n");
    i2cproc_cleanup();
    return res;
  }
  i2cproc_initialized ++;
  return 0;
}

int i2cproc_cleanup(void)
{
  int res;

  if (i2cproc_initialized >= 3) {
    if ((res = i2c_del_driver(&i2cproc_driver))) {
      printk("i2c-proc.o: Driver deregistration failed, "
             "module not removed.\n");
      return res;
    }
    i2cproc_initialized--;
  }
  if (i2cproc_initialized >= 1) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
    remove_proc_entry("i2c",proc_bus);
    i2cproc_initialized -= 2;
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,29)) */
    if (i2cproc_initialized >= 2) {
      if ((res = proc_unregister(&proc_bus_dir,proc_bus_i2c_dir.low_ino))) {
         printk("i2c-proc.o: could not delete /proc/bus/i2c, "
                "module not removed.");
         return res;
      }    
      i2cproc_initialized --;
    }
    if ((res = proc_unregister(&proc_root,proc_bus_dir.low_ino))) {
       printk("i2c-proc.o: could not delete /proc/bus/, "
              "module not removed.");
       return res;
    }    
    i2cproc_initialized --;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */
  }
  return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
/* Monitor access to /proc/bus/i2c*; make unloading i2c-proc.o impossible
   if some process still uses it or some file in it */
void monitor_bus_i2c(struct inode *inode, int fill)
{
  if (fill)
    MOD_INC_USE_COUNT;
  else
    MOD_DEC_USE_COUNT;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */


/* This function generates the output for /proc/bus/i2c */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
int read_bus_i2c(char *buf, char **start, off_t offset, int len, int *eof, 
                 void *private)
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,29)) */
int read_bus_i2c(char *buf, char **start, off_t offset, int len, int unused)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */
{
  int i;
  len = 0;
  for (i = 0; i < I2C_ADAP_MAX; i++)
    if (i2cproc_adapters[i])
      len += sprintf(buf+len, "i2c-%d\t%s\t%-32s\t%-32s\n",
                     i2c_adapter_id(i2cproc_adapters[i]),
                     i2c_is_smbus_adapter(i2cproc_adapters[i])?"smbus":
#ifdef DEBUG
                       i2c_is_isa_adapter(i2cproc_adapters[i])?"isa":
#endif /* def DEBUG */
                       "i2c",
                     i2cproc_adapters[i]->name,
                     i2cproc_adapters[i]->algo->name);
  return len;
}

/* This function generates the output for /proc/bus/i2c-? */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
ssize_t i2cproc_bus_read(struct file * file, char * buf,size_t count, 
                         loff_t *ppos)
{
  struct inode * inode = file->f_dentry->d_inode;
#else (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,29))
int i2cproc_bus_read(struct inode * inode, struct file * file,char * buf,
                     int count)
{
#endif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
  char *kbuf;
  struct i2c_client *client;
  int i,j,len=0;

  if (count < 0)
    return -EINVAL;
  if (count > 4000)
    count = 4000;
  for (i = 0; i < I2C_ADAP_MAX; i++)
    if (i2cproc_inodes[i] == inode->i_ino) {
      if (! (kbuf = kmalloc(count,GFP_KERNEL)))
        return -ENOMEM;
      for (j = 0; j < I2C_CLIENT_MAX; j++)
        if ((client = i2cproc_adapters[i]->clients[j]))
          /* Filter out dummy clients */
#ifndef DEBUG
          if ((client->driver->id != I2C_DRIVERID_I2CPROC) &&
              (client->driver->id != I2C_DRIVERID_I2CDEV))
#endif /* ndef DEBUG */
            len += sprintf(kbuf+len,"%x\t%-32s\t%-32s\n",
#ifdef DEBUG
                           i2c_is_isa_client(client)?
                             ((struct isa_client *) client)->isa_addr&0xffffff:
#endif /* def DEBUG */
                             client->addr,
                           client->name,client->driver->name);
      if (file->f_pos+len > count)
        len = count - file->f_pos;
      len = len - file->f_pos;
      if (len < 0) 
        len = 0;
      copy_to_user (buf,kbuf+file->f_pos,len);
      file->f_pos += len;
      kfree(kbuf);
      return len;
    }
  return -ENOENT;
}


/* We need to add the adapter to i2cproc_adapters, if it is interesting
   enough */
int i2cproc_attach_adapter(struct i2c_adapter *adapter)
{
  struct i2c_client *client;
  int i,res;
  char name[8];

  struct proc_dir_entry *proc_entry;

#ifndef DEBUG
  if (i2c_is_isa_adapter(adapter))
    return 0;
#endif /* ndef DEBUG */

  for (i = 0; i < I2C_ADAP_MAX; i++)
    if(!i2cproc_adapters[i])
      break;
  if (i == I2C_ADAP_MAX) {
    printk("i2c-proc.o: Too many adapters!\n");
    return -ENOMEM;
  }

#ifndef DEBUG
  if (! (client = kmalloc(sizeof(struct i2c_client),GFP_KERNEL))) {
#else /* def DEBUG */
  if (! (client = kmalloc(sizeof(struct isa_client),GFP_KERNEL))) {
#endif 
    printk("i2c-proc.o: Out of memory!\n");
    return -ENOMEM;
  }
  memcpy(client,&i2cproc_client_template,sizeof(struct i2c_client));
#ifdef DEBUG
  ((struct isa_client *) client) -> isa_addr = -1;
#endif /* def DEBUG */
  client->adapter = adapter;
  if ((res = i2c_attach_client(client))) {
    printk("i2c-proc.o: Attaching client failed.\n");
    kfree(client);
    return res;
  }
  i2cproc_adapters[i] = adapter;

  sprintf(name,"i2c-%d",i2c_adapter_id(adapter));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
  proc_entry = create_proc_entry(name,0,proc_bus);
  if (! proc_entry) {
    printk("i2c-proc.o: Could not create /proc/bus/%s\n",name);
    kfree(client);
    return -ENOENT;
  }
  proc_entry->ops = &i2cproc_inode_operations;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
  proc_entry->fill_inode = &monitor_bus_i2c;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,29)) */
  if (!(proc_entry = kmalloc(sizeof(struct proc_dir_entry)+strlen(name)+1,
                             GFP_KERNEL))) {
    printk("i2c-proc.o: Out of memory!\n");
    return -ENOMEM;
  }

  memset(proc_entry,0,sizeof(struct proc_dir_entry));
  proc_entry->namelen = strlen(name);
  proc_entry->name = (char *) (proc_entry + 1);
  proc_entry->mode = S_IRUGO | S_IFREG;
  proc_entry->nlink = 1;
  proc_entry->ops = &i2cproc_inode_operations;
  strcpy((char *) proc_entry->name,name);

  if ((res = proc_register_dynamic(&proc_bus_dir, proc_entry))) {
    printk("i2c-proc.o: Could not create %s.\n",name);
    kfree(proc_entry);
    kfree(client);
    return res;
  }

  i2cproc_proc_entries[i] = proc_entry;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */

  i2cproc_inodes[i] = proc_entry->low_ino;
  return 0;
}
  
int i2cproc_detach_client(struct i2c_client *client)
{
  int i,res;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
  char name[8];
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */

#ifndef DEBUG
  if (i2c_is_isa_client(client))
    return 0;
#endif /* ndef DEBUG */

  for (i = 0; i < I2C_ADAP_MAX; i++) 
    if (client->adapter == i2cproc_adapters[i]) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29))
      sprintf(name,"i2c-%d",i2c_adapter_id(i2cproc_adapters[i]));
      remove_proc_entry(name,proc_bus);
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,29)) */
      if ((res = proc_unregister(&proc_bus_dir,
                                 i2cproc_proc_entries[i]->low_ino))) {
        printk("i2c-proc.o: Deregistration of /proc entry failed, "
               "client not detached.\n");
        return res;
      }
      kfree(i2cproc_proc_entries[i]);
      i2cproc_proc_entries[i] = NULL;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,29)) */
      if ((res = i2c_detach_client(client))) {
        printk("i2c-proc.o: Client deregistration failed, "
               "client not detached.\n");
        return res;
      }
      i2cproc_adapters[i] = NULL;
      i2cproc_inodes[i] = 0;
      kfree(client);
      return 0;
    }
  return -ENOENT;
}

/* Nothing here yet */
int i2cproc_command(struct i2c_client *client, unsigned int cmd,
                    void *arg)
{
  return -1;
}

/* Nothing here yet */
void i2cproc_inc_use(struct i2c_client *client)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

/* Nothing here yet */
void i2cproc_dec_use(struct i2c_client *client)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("I2C /proc/bus entries driver");

int init_module(void)
{
  return i2cproc_init();
}

int cleanup_module(void)
{
  return i2cproc_cleanup();
}

#endif /* def MODULE */

