/*
    i2c-keywest.c - Part of lm_sensors,  Linux kernel modules
                for hardware monitoring

    i2c Support for Apple Keywest I2C Bus Controller

    Copyright (c) 2000 Philip Edelbrock <phil@stimpy.netroedge.com>

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
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/types.h>

#include <linux/i2c.h>

#include <linux/init.h>
#include <linux/mm.h>

#include <asm/prom.h>
// #include <asm/dbdma.h>
// #include <asm/cuda.h>
// #include <asm/pmu.h>
#include <asm/feature.h>
#include <linux/nvram.h>
// #include <linux/vt_kern.h>


/* PCI device */
#define VENDOR		0x106b
#define DEVICE		0x22

/*****    Protos    ******/
s32 keywest_access(struct i2c_adapter * adap, u16 addr,
                  unsigned short flags, char read_write,
                  u8 command, int size, union i2c_smbus_data * data);
u32 keywest_func(struct i2c_adapter *adapter);
/***** End ofProtos ******/


struct keywest_iface {
	void *base;
	void *steps;
	void *control;
	void *status;
	void *ISR;
	void *IER;
	void *addr;
	void *subaddr;
	void *data;
	struct i2c_adapter *i2c_adapt;
	struct keywest_iface *next;
};

static struct i2c_algorithm smbus_algorithm = {
        /* name */ "Non-I2C SMBus adapter",
        /* id */ I2C_ALGO_SMBUS,
        /* master_xfer */ NULL,
        /* smbus_access */ keywest_access,
        /* slave_send */ NULL,
        /* slave_rcv */ NULL,
        /* algo_control */ NULL,
        /* functionality */ keywest_func,
};


struct keywest_iface *ifaces=NULL;

s32 keywest_access(struct i2c_adapter * adap, u16 addr,
                  unsigned short flags, char read_write,
                  u8 command, int size, union i2c_smbus_data * data) {
 struct keywest_iface *ifaceptr;

  ifaceptr=(struct keywest_iface *)adap->data;
  printk("Keywest access called with ref to iface: 0x%X\n",ifaceptr->base);
  return -1;
}

u32 keywest_func(struct i2c_adapter *adapter){
        return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
            I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
            I2C_FUNC_SMBUS_BLOCK_DATA | I2C_FUNC_SMBUS_PROC_CALL;
}

void keywest_inc(struct i2c_adapter *adapter)
{
        MOD_INC_USE_COUNT;
}

void keywest_dec(struct i2c_adapter *adapter)
{
        MOD_DEC_USE_COUNT;
}

int init_iface (void *base,void *steps,struct keywest_iface **ifaces) {
 struct i2c_adapter *keywest_adapter;
 char *name;
 struct keywest_iface **temp_hdl=ifaces;
 int res;

  if (ifaces == NULL) {
   printk("Ah!  Passed a null handle to init_iface");
   return -1;
  }
  while ((*temp_hdl) != NULL) {
   printk("found an entry, skipping.\n");
   temp_hdl=&(*temp_hdl)->next;
  }
  *temp_hdl=(struct keywest_iface *)kmalloc(sizeof(struct keywest_iface),GFP_KERNEL);
  if (*temp_hdl == NULL) { printk("kmalloc failed on temp_hdl!"); return -1; }
  (*temp_hdl)->next=NULL;
  base=ioremap((int)base,(int)steps*8);
  (*temp_hdl)->base=base;
  (*temp_hdl)->steps=steps;
  (*temp_hdl)->control=(void *)((__u32)base+(__u32)steps);
  (*temp_hdl)->status=(void *)((__u32)base+(__u32)steps*2);
  (*temp_hdl)->ISR=(void *)((__u32)base+(__u32)steps*3);
  (*temp_hdl)->IER=(void *)((__u32)base+(__u32)steps*4);
  (*temp_hdl)->addr=(void *)((__u32)base+(__u32)steps*5);
  (*temp_hdl)->subaddr=(void *)((__u32)base+(__u32)steps*6);
  (*temp_hdl)->data=(void *)((__u32)base+(__u32)steps*7);
  keywest_adapter=(struct i2c_adapter*)kmalloc(sizeof(struct i2c_adapter),GFP_KERNEL);
  memset((void *)keywest_adapter,0,(sizeof(struct i2c_adapter)));
  if (keywest_adapter == NULL) { printk("kmalloc failed on keywest_adapter!"); return -1; }
  strcpy(keywest_adapter->name,"keywest i2c\0");
  keywest_adapter->id=I2C_ALGO_SMBUS;
  keywest_adapter->algo=&smbus_algorithm;
  keywest_adapter->algo_data=NULL;
  keywest_adapter->inc_use=keywest_inc;
  keywest_adapter->dec_use=keywest_dec;
  keywest_adapter->client_register=NULL;
  keywest_adapter->client_unregister=NULL;
  keywest_adapter->data=(void *)(*temp_hdl);
  (*temp_hdl)->i2c_adapt=keywest_adapter;
  if (res=i2c_add_adapter((*temp_hdl)->i2c_adapt)) {
                printk
                    ("i2c-keywest.o: Adapter registration failed, module not inserted.\n");
                cleanup();
  }
  printk("Done creating iface entry\n");
  return res;
}

void dump_ifaces (struct keywest_iface **ifaces) {
  if (ifaces == NULL) { printk("Ah!  Passed null handle to dump!\n"); return; }
  if (*ifaces != NULL) {
   printk("Interface @%X,%X  Locations:\n",(*ifaces)->base,(*ifaces)->steps);
   printk("  control:%X status:%X ISR:%X IER:%X addr:%X subaddr:%X data:%X\n",
     (*ifaces)->control,(*ifaces)->status,(*ifaces)->ISR,
     (*ifaces)->IER,(*ifaces)->addr,(*ifaces)->subaddr,(*ifaces)->data);
   printk("Contents:\n");
   printk("  control: 0x%02X status:0x%02X ISR:0x%02X IER:0x%02X addr:0x%02X subaddr:0x%02X data:0x%02X\n",
     readb((*ifaces)->control),readb((*ifaces)->status),readb((*ifaces)->ISR),
     readb((*ifaces)->IER),readb((*ifaces)->addr),readb((*ifaces)->subaddr),
     readb((*ifaces)->data));
   printk("I2C-Adapter:\n");
   printk("  name:%s\n",(*ifaces)->i2c_adapt->name);
   dump_ifaces(&(*ifaces)->next);
  } else { printk("End of ifaces.\n"); }
}

int cleanup (struct keywest_iface **ifaces) {
 int res=0;

  if (ifaces == NULL) { printk("Ah!  Passed null handle to cleanup!\n"); return; }
  if (*ifaces != NULL) {
   printk("Cleaning up interface @%X,%X\n",(*ifaces)->base,(*ifaces)->steps);
   printk("  control:%X status:%X ISR:%X IER:%X addr:%X subaddr:%X data:%X\n",
     (*ifaces)->control,(*ifaces)->status,(*ifaces)->ISR,(*ifaces)->ISR,
     (*ifaces)->IER,(*ifaces)->addr,(*ifaces)->subaddr,(*ifaces)->data);
   cleanup(&(*ifaces)->next);
   if (res = i2c_del_adapter((*ifaces)->i2c_adapt)) {
     printk ("i2c-keywest.o: i2c_del_adapter failed, module not removed\n");
   }
   kfree(*ifaces);
   iounmap((*ifaces)->base);
   (*ifaces)=NULL;
  }
  return res;
}


static void scan_of(char *dev_type) {
 struct device_node *np=NULL;
 struct property *dp=NULL;
 int i;

 np = find_devices(dev_type);
 if (np == 0) { printk("No %s devices found.\n",dev_type); }
 while (np != 0 ) {
  printk("%s found: %s, with properties:\n",dev_type,np->full_name);
  dp=np->properties;
  while (dp != 0) {
   printk("     %s = %s [ ",dp->name,dp->value);
   for (i=0; i < dp->length; i++) {
    printk("%02X",(char)dp->value[i]);
    if (((i+1) % 4) == 0) { printk(" "); }
   }
   printk("] (length=%d)\n",dp->length);
   dp=dp->next;
  }
  np=np->next;
 }
}

static int find_keywest(void)
{
	struct pci_dev *s_bridge;
	u8 rev;
	struct device_node *i2c_device;
	struct device_node *np;
	struct property *dp;
	void **temp;
	int i;
	void *base=NULL;
	void *steps=NULL;

#ifdef DEBUG
		scan_of("i2c");
#endif
//		scan_of("backlight");
//		scan_of("sound");
//		scan_of("ethernet");
		scan_of("i2c");
                i2c_device = find_compatible_devices("i2c","keywest");
		if (i2c_device == 0) { printk("No Keywest i2c devices found.\n");  return -1; }
                while (i2c_device != 0) {
                        printk("Keywest device found: %s\n",i2c_device->full_name);
                        temp=(void **)get_property(i2c_device, "AAPL,address", NULL);
			if (temp != NULL) {base=*temp;} else {printk("no 'address' prop!\n");}
//			printk("address= [%X]\n",base);
                        temp=(void **)get_property(i2c_device, "AAPL,address-step", NULL);
			if (temp != NULL) {steps=*temp;} else {printk("no 'address-step' prop!\n");}
			printk("Device found. base: %X steps: %X\n",base,steps);
			if (init_iface(base,steps,&ifaces) !=0 ) {
				cleanup(&ifaces); return -ENODEV;
			}
			i2c_device=i2c_device->next;
		}
	if (ifaces == NULL) { printk("ifaces null! :'("); }
	dump_ifaces(&ifaces);
	return 0;
}

#ifdef MODULE
static
#else
extern
#endif
int __init i2c_keywest_init(void)
{
	return find_keywest();
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Philip Edelbrock <phil@netroedge.com");
MODULE_DESCRIPTION("I2C driver for Apple's Keywest");

int init_module(void)
{
	return i2c_keywest_init();
}

void cleanup_module(void)
{
	cleanup(&ifaces);
}
#endif
