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
	struct keywest_iface *next;
};

struct keywest_iface *ifaces=NULL;

int init_iface (void *base,void *steps,struct keywest_iface **ifaces) {
 struct keywest_iface **temp_hdl=ifaces;

  if (ifaces == NULL) {
   printk("Ah!  Passed a null handle to init_iface");
   return -1;
  }
  while ((*temp_hdl) != NULL) {
   printk("found an entry, skipping.\n");
   temp_hdl=&(*temp_hdl)->next;
  }
  *temp_hdl=(struct keywest_iface *)kmalloc(sizeof(struct keywest_iface),GFP_KERNEL);
  if (*temp_hdl == NULL) { printk("kmalloc failed!"); return -1; }
  (*temp_hdl)->next=NULL;
  base=ioremap(base,(int)steps*8);
  (*temp_hdl)->base=base;
  (*temp_hdl)->steps=steps;
  (*temp_hdl)->control=(void *)((__u32)base+(__u32)steps);
  (*temp_hdl)->status=(void *)((__u32)base+(__u32)steps*2);
  (*temp_hdl)->ISR=(void *)((__u32)base+(__u32)steps*3);
  (*temp_hdl)->IER=(void *)((__u32)base+(__u32)steps*4);
  (*temp_hdl)->addr=(void *)((__u32)base+(__u32)steps*5);
  (*temp_hdl)->subaddr=(void *)((__u32)base+(__u32)steps*6);
  (*temp_hdl)->data=(void *)((__u32)base+(__u32)steps*7);
  printk("Done creating iface entry\n");
  return 0;
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
   dump_ifaces(&(*ifaces)->next);
  } else { printk("End of ifaces.\n"); }
}

void cleanup (struct keywest_iface **ifaces) {
  if (ifaces == NULL) { printk("Ah!  Passed null handle to cleanup!\n"); return; }
  if (*ifaces != NULL) {
   printk("Cleaning up interface @%X,%X\n",(*ifaces)->base,(*ifaces)->steps);
   printk("  control:%X status:%X ISR:%X IER:%X addr:%X subaddr:%X data:%X\n",
     (*ifaces)->control,(*ifaces)->status,(*ifaces)->ISR,(*ifaces)->ISR,
     (*ifaces)->IER,(*ifaces)->addr,(*ifaces)->subaddr,(*ifaces)->data);
   cleanup(&(*ifaces)->next);
   kfree(*ifaces);
   iounmap((*ifaces)->base);
   (*ifaces)=NULL;
  }
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
	printk("Cleaning up\n");
	cleanup(&ifaces);
}
#endif
