/*
    sensors.c - Part of lm_sensors, Linux kernel modules for hardware
                monitoring
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
#include <linux/malloc.h>
#include <linux/ctype.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>

#include "version.h"
#include "compat.h"
#include "i2c.h"
#include "isa.h"
#include "sensors.h"


#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif /* MODULE */

static int sensors_create_name(char **name, const char *prefix,
                               struct i2c_adapter * adapter, int addr);
static void sensors_parse_reals(int *nrels, void *buffer, int bufsize,
                                long *results, int magnitude);
static void sensors_write_reals(int nrels,void *buffer,int *bufsize,
                                long *results, int magnitude);
static int sensors_proc_chips(ctl_table *ctl, int write, struct file * filp,
                              void *buffer, size_t *lenp);
static int sensors_sysctl_chips (ctl_table *table, int *name, int nlen, 
                                 void *oldval, size_t *oldlenp, void *newval,
                                 size_t newlen, void **context);

static int sensors_init(void);
static int sensors_cleanup(void);

#define SENSORS_ENTRY_MAX 20
static struct ctl_table_header *sensors_entries[SENSORS_ENTRY_MAX];

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
static struct i2c_client *sensors_clients[SENSORS_ENTRY_MAX];
static unsigned short sensors_inodes[SENSORS_ENTRY_MAX];
static void sensors_fill_inode(struct inode *inode, int fill);
static void sensors_dir_fill_inode(struct inode *inode, int fill);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */

static ctl_table sysctl_table[] = {
  { CTL_DEV, "dev", NULL, 0, 0555 },
  { 0 },
  { DEV_SENSORS, "sensors", NULL, 0, 0555 },
  { 0 },
  { 0, NULL, NULL, 0, 0555 },
  { 0 }
};

static ctl_table sensors_proc_dev_sensors[] = {
  { SENSORS_CHIPS, "chips", NULL, 0, 0644, NULL, &sensors_proc_chips, 
    &sensors_sysctl_chips },
  { 0 }
};

static ctl_table sensors_proc_dev[] = {
  { DEV_SENSORS, "sensors", NULL, 0, 0555, sensors_proc_dev_sensors },
  { 0 },
};


static ctl_table sensors_proc[] = {
  { CTL_DEV, "dev", NULL, 0, 0555, sensors_proc_dev },
  { 0 }
};


static struct ctl_table_header *sensors_proc_header;
static int sensors_initialized;

/* This returns a nice name for a new directory; for example lm78-isa-0310
   (for a LM78 chip on the ISA bus at port 0x310), or lm75-i2c-3-4e (for
   a LM75 chip on the third i2c bus at address 0x4e).  
   name is allocated first. */
int sensors_create_name(char **name, const char *prefix, 
                        struct i2c_adapter * adapter, int addr)
{
  char name_buffer[50]; 
  int id;
  if (i2c_is_isa_adapter(adapter)) 
    sprintf(name_buffer,"%s-isa-%04x",prefix,addr);
  else {
    if ((id = i2c_adapter_id(adapter)) < 0)
      return -ENOENT;
    sprintf(name_buffer,"%s-i2c-%d-%02x",prefix,id,addr);
  }
  *name = kmalloc(strlen(name_buffer)+1,GFP_KERNEL);
  strcpy(*name,name_buffer);
  return 0;
}

/* This rather complex function must be called when you want to add an entry
   to /proc/sys/dev/sensors/chips (not yet implemented). It also creates
   a new directory within /proc/sys/dev/sensors/.
   ctl_template should be a template of the newly created directory. It is
   copied in memory. The extra2 field of each file is set to point to client.
   If any driver wants subdirectories within the newly created directory,
   this function must be updated! */
int sensors_register_entry(struct i2c_client *client ,const char *prefix, 
                           ctl_table *ctl_template)
{
  int i,res,len,id;
  ctl_table *new_table;
  char *name;
  struct ctl_table_header *new_header;

  if ((res = sensors_create_name(&name,prefix,client->adapter,
                                 i2c_is_isa_client(client)?
                                 ((struct isa_client *) client)->isa_addr:
                                 client->addr)))
    return res;

  for (id = 0; id < SENSORS_ENTRY_MAX; id++)
    if (! sensors_entries[id]) {
      break;
    }
  if (id == SENSORS_ENTRY_MAX) {
    kfree(name);
    return -ENOMEM;
  }
  id += 256;

  len = 0;
  while (ctl_template[len].procname)
    len++;
  len += 7;
  if (! (new_table = kmalloc(sizeof(ctl_table) * len,GFP_KERNEL))) {
    kfree(name);
    return -ENOMEM;
  }
    
  memcpy(new_table,sysctl_table,6 * sizeof(ctl_table));
  new_table[0].child = &new_table[2];
  new_table[2].child = &new_table[4];
  new_table[4].child = &new_table[6];
  new_table[4].procname = name;
  new_table[4].ctl_name = id;
  memcpy(new_table+6,ctl_template,(len-6) * sizeof(ctl_table));
  for (i = 6; i < len; i++)
    new_table[i].extra2 = client;

  if (! (new_header = register_sysctl_table(new_table,0))) {
    kfree(new_table);
    kfree(name);
    return -ENOMEM;
  }

  sensors_entries[id-256] = new_header;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
  sensors_clients[id-256] = client;
#ifdef DEBUG
  if (!new_header || !new_header->ctl_table || 
      !new_header->ctl_table->child || 
      !new_header->ctl_table->child->child ||
      !new_header->ctl_table->child->child->de) {
    printk("sensors.o: NULL pointer when trying to install fill_inode fix!\n");
    return id;  
  }
#endif /* DEBUG */
  sensors_inodes[id-256] = new_header->ctl_table->child->child->de->low_ino;
  new_header->ctl_table->child->child->de->fill_inode = &sensors_dir_fill_inode;
#endif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))

  return id;
}

void sensors_deregister_entry(int id)
{
  ctl_table *table;
  id -= 256;
  if (sensors_entries[id]) {
    table = sensors_entries[id]->ctl_table;
    unregister_sysctl_table(sensors_entries[id]);
    kfree((void *) (table[4].procname));
    kfree(table);
    sensors_entries[id] = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
    sensors_clients[id] = NULL;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */
  }
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
/* Monitor access for /proc/sys/dev/sensors; make unloading sensors.o 
   impossible if some process still uses it or some file in it */
void sensors_fill_inode(struct inode *inode, int fill)
{
  if (fill)
    MOD_INC_USE_COUNT;
  else
    MOD_DEC_USE_COUNT;
}

/* Monitor access for /proc/sys/dev/sensors/ directories; make unloading
   the corresponding module impossible if some process still uses it or
   some file in it */
void sensors_dir_fill_inode(struct inode *inode, int fill)
{
  int i;
  struct i2c_client *client;

#ifdef DEBUG
  if (! inode) {
    printk("sensors.o: Warning: inode NULL in fill_inode()\n");
    return;
  }
#endif /* def DEBUG */
  
  for (i = 0; i < SENSORS_ENTRY_MAX; i++) 
    if (sensors_clients[i] && (sensors_inodes[i] == inode->i_ino))
      break;
#ifdef DEBUG
  if (i == SENSORS_ENTRY_MAX) {
    printk("sensors.o: Warning: inode (%ld) not found in fill_inode()\n",
           inode->i_ino);
    return;
  }
#endif /* def DEBUG */
  client = sensors_clients[i];
  if (fill)
    client->driver->inc_use(client);
  else
    client->driver->dec_use(client);
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */

int sensors_proc_chips(ctl_table *ctl, int write, struct file * filp,
                       void *buffer, size_t *lenp)
{
  char BUF[SENSORS_PREFIX_MAX + 30];
  int buflen,curbufsize,i;
  struct ctl_table *client_tbl;

  if (write)
    return 0;

  /* If buffer is size 0, or we try to read when not at the start, we
     return nothing. Note that I think writing when not at the start
     does not work either, but anyway, this is straight from the kernel
     sources. */
  if (!*lenp || (filp->f_pos && !write)) {
    *lenp = 0;
    return 0;
  }
  curbufsize = 0;
  for (i = 0; i < SENSORS_ENTRY_MAX; i ++)
    if (sensors_entries[i]) {
      client_tbl = sensors_entries[i]->ctl_table->child->child;
      buflen = sprintf(BUF,"%d\t%s\n",client_tbl->ctl_name,
                       client_tbl->procname);
      if (buflen + curbufsize > *lenp)
        buflen=*lenp-curbufsize;
      copy_to_user(buffer,BUF,buflen);
      curbufsize += buflen;
      (char *) buffer += buflen;
    }
  *lenp = curbufsize;
  filp->f_pos += curbufsize;
  return 0;
}

int sensors_sysctl_chips (ctl_table *table, int *name, int nlen, void *oldval,
                          size_t *oldlenp, void *newval, size_t newlen,
                          void **context)
{
  struct sensors_chips_data data;
  int i,oldlen,nrels,maxels;
  struct ctl_table *client_tbl;

  if (oldval && oldlenp && ! get_user_data(oldlen,oldlenp) && oldlen) {
    maxels = oldlen / sizeof(struct sensors_chips_data);
    nrels = 0;
    for (i = 0; (i < SENSORS_ENTRY_MAX) && (nrels < maxels); i++)
      if (sensors_entries[i]) {
        client_tbl = sensors_entries[i]->ctl_table->child->child;
        data.sysctl_id = client_tbl->ctl_name;
        strcpy(data.name,client_tbl->procname);
        copy_to_user(oldval,&data,sizeof(struct sensors_chips_data));
        (char *) oldval += sizeof(struct sensors_chips_data);
        nrels++;
      }
    oldlen = nrels * sizeof(struct sensors_chips_data);
    put_user(oldlen,oldlenp);
  }
  return 0;
}


/* This funcion reads or writes a 'real' value (encoded by the combination
   of an integer and a magnitude, the last is the power of ten the value
   should be divided with) to a /proc/sys directory. To use this function,
   you must (before registering the ctl_table) set the extra2 field to the
   client, and the extra1 field to a function of the form:
      void func(struct i2c_client *client, int operation, int ctl_name,
                int *nrels_mag, long *results)
   This function can be called for three values of operation. If operation
   equals SENSORS_PROC_REAL_INFO, the magnitude should be returned in 
   nrels_mag. If operation equals SENSORS_PROC_REAL_READ, values should
   be read into results. nrels_mag should return the number of elements
   read; the maximum number is put in it on entry. Finally, if operation
   equals SENSORS_PROC_REAL_WRITE, the values in results should be
   written to the chip. nrels_mag contains on entry the number of elements
   found.
   In all cases, client points to the client we wish to interact with,
   and ctl_name is the SYSCTL id of the file we are accessing. */
int sensors_proc_real(ctl_table *ctl, int write, struct file * filp,
                      void *buffer, size_t *lenp)
{
#define MAX_RESULTS 20
  int mag,nrels=MAX_RESULTS;
  long results[MAX_RESULTS];
  sensors_real_callback callback = ctl -> extra1;
  struct i2c_client *client = ctl -> extra2;

  /* If buffer is size 0, or we try to read when not at the start, we
     return nothing. Note that I think writing when not at the start
     does not work either, but anyway, this is straight from the kernel
     sources. */
  if (!*lenp || (filp->f_pos && !write)) {
    *lenp = 0;
    return 0;
  }

  /* Get the magnitude */
  callback(client,SENSORS_PROC_REAL_INFO,ctl->ctl_name,&mag,NULL);

  if (write) {
    /* Read the complete input into results, converting to longs */
    sensors_parse_reals(&nrels,buffer,*lenp,results,mag);

    if (! nrels)
      return 0;

    /* Now feed this information back to the client */
    callback(client,SENSORS_PROC_REAL_WRITE,ctl->ctl_name,&nrels,results);
    
    filp->f_pos += *lenp;
    return 0;
  } else { /* read */
    /* Get the information from the client into results */
    callback(client,SENSORS_PROC_REAL_READ,ctl->ctl_name,&nrels,results);

    /* And write them to buffer, converting to reals */
    sensors_write_reals(nrels,buffer,lenp,results,mag);
    filp->f_pos += *lenp;
    return 0;
  }
}

/* This function is equivalent to sensors_proc_real, only it interacts with
   the sysctl(2) syscall, and returns no reals, but integers */
int sensors_sysctl_real (ctl_table *table, int *name, int nlen, void *oldval,
               size_t *oldlenp, void *newval, size_t newlen,
               void **context)
{
  long results[MAX_RESULTS];
  int oldlen,nrels=MAX_RESULTS;
  sensors_real_callback callback = table -> extra1;
  struct i2c_client *client = table -> extra2;

  /* Check if we need to output the old values */
  if (oldval && oldlenp && ! get_user_data(oldlen,oldlenp) && oldlen) {
    callback(client,SENSORS_PROC_REAL_READ,table->ctl_name,&nrels,results);

    /* Note the rounding factor! */
    if (nrels * sizeof(long) < oldlen)
      oldlen = nrels * sizeof(long);
    oldlen = (oldlen / sizeof(long)) * sizeof(long);
    copy_to_user(oldval,results,oldlen);
    put_user(oldlen,oldlenp);
  }

  if (newval && newlen) {
    /* Note the rounding factor! */
    newlen -= newlen % sizeof(long);
    nrels = newlen / sizeof(long);
    copy_from_user(results,newval,newlen);
    
    /* Get the new values back to the client */
    callback(client,SENSORS_PROC_REAL_WRITE,table->ctl_name,&nrels,results);
  }
  return 0;
}
    

/* nrels contains initially the maximum number of elements which can be
   put in results, and finally the number of elements actually put there.
   A magnitude of 1 will multiply everything with 10; etc.
   buffer, bufsize is the character buffer we read from and its length.
   results will finally contain the parsed integers. 

   Buffer should contain several reals, separated by whitespace. A real
   has the following syntax:
     [ Minus ] Digit* [ Dot Digit* ] 
   (everything between [] is optional; * means zero or more).
   When the next character is unparsable, everything is skipped until the
   next whitespace.

   WARNING! This is tricky code. I have tested it, but there may still be
            hidden bugs in it, even leading to crashes and things!
*/
void sensors_parse_reals(int *nrels, void *buffer, int bufsize, 
                         long *results, int magnitude)
{
  int maxels,min,mag;
  long res;
  char nextchar=0;

  maxels = *nrels;
  *nrels = 0;

  while (bufsize && (*nrels < maxels)) {

    /* Skip spaces at the start */
    while (bufsize && ! get_user_data(nextchar,(char *) buffer) && 
           isspace((int) nextchar)) {
      bufsize --;
      ((char *) buffer)++;
    }

    /* Well, we may be done now */
    if (! bufsize)
      return;

    /* New defaults for our result */
    min = 0;
    res = 0;
    mag = magnitude;

    /* Check for a minus */
    if (! get_user_data(nextchar,(char *) buffer) && (nextchar == '-')) {
      min=1;
      bufsize--;
      ((char *) buffer)++;
    }

    /* Digits before a decimal dot */
    while (bufsize && !get_user_data(nextchar,(char *) buffer) && 
           isdigit((int) nextchar)) {
      res = res * 10 + nextchar - '0';
      bufsize--;
      ((char *) buffer)++;
    }

    /* If mag < 0, we must actually divide here! */
    while (mag < 0) {
      res = res / 10;
      mag++;
    }

    if (bufsize && (nextchar == '.')) {
      /* Skip the dot */
      bufsize--;
      ((char *) buffer)++;
  
      /* Read digits while they are significant */
      while(bufsize && (mag > 0) && 
            !get_user_data(nextchar,(char *) buffer) &&
            isdigit((int) nextchar)) {
        res = res * 10 + nextchar - '0';
        mag--;
        bufsize--;
        ((char *) buffer)++;
      }
    }
    /* If we are out of data, but mag > 0, we need to scale here */
    while (mag > 0) {
      res = res * 10;
      mag --;
    }

    /* Skip everything until we hit whitespace */
    while(bufsize && !get_user_data(nextchar,(char *) buffer) &&
          isspace ((int) nextchar)) {
      bufsize --;
      ((char *) buffer) ++;
    }

    /* Put res in results */
    results[*nrels] = (min?-1:1)*res;
    (*nrels)++;
  }    
  
  /* Well, there may be more in the buffer, but we need no more data. 
     Ignore anything that is left. */
  return;
}
    
void sensors_write_reals(int nrels,void *buffer,int *bufsize,long *results,
                         int magnitude)
{
  #define BUFLEN 20
  char BUF[BUFLEN+1]; /* An individual representation should fit in here! */
  char printfstr[10];
  int nr=0;
  int buflen,mag,times;
  int curbufsize=0;

  while ((nr < nrels) && (curbufsize < *bufsize)) {
    mag=magnitude;

    if (nr != 0) {
      put_user(' ', (char *) buffer);
      curbufsize ++;
      ((char *) buffer) ++;
    }

    /* Fill BUF with the representation of the next string */
    if (mag <= 0) {

      buflen=sprintf(BUF,"%ld",results[nr]);
      if (buflen < 0) { /* Oops, a sprintf error! */
        *bufsize=0;
        return;
      }
      while ((mag < 0) && (buflen < BUFLEN)) {
        BUF[buflen++]='0';
        mag++;
      }
      BUF[buflen]=0;
    } else {
      times=1;
      for (times=1; mag-- > 0; times *= 10);
      if (results[nr] < 0) {
        BUF[0] = '-';
        buflen = 1;
      } else
        buflen=0;
      strcpy(printfstr,"%ld.%0Xld");
      printfstr[6]=magnitude+'0';
      buflen+=sprintf(BUF+buflen,printfstr,abs(results[nr])/times,
                      abs(results[nr])%times);
      if (buflen < 0) { /* Oops, a sprintf error! */
        *bufsize=0;
        return;
      }
    }

    /* Now copy it to the user-space buffer */
    if (buflen + curbufsize > *bufsize)
      buflen=*bufsize-curbufsize;
    copy_to_user(buffer,BUF,buflen);
    curbufsize += buflen;
    (char *) buffer += buflen;

    nr ++;
  }
  if (curbufsize < *bufsize) {
    put_user('\n', (char *) buffer);
    curbufsize ++;
  }
  *bufsize=curbufsize;
}

int sensors_init(void) 
{
  printk("sensors.o version %s (%s)\n",LM_VERSION,LM_DATE);
  sensors_initialized = 0;
  if (! (sensors_proc_header = register_sysctl_table(sensors_proc,0)))
    return -ENOMEM;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58))
  sensors_proc_header->ctl_table->child->de->fill_inode = &sensors_fill_inode;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,58)) */
  sensors_initialized ++;
  return 0;
}

int sensors_cleanup(void)
{
  if (sensors_initialized >= 1) {
    unregister_sysctl_table(sensors_proc_header);
    sensors_initialized --;
  }
  return 0;
}

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("LM78 driver");

int init_module(void)
{
  return sensors_init();
}

int cleanup_module(void)
{
  return sensors_cleanup();
}

#endif /* MODULE */

