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

    Changes:

    2001/7/14  Paul Harrison:
    		- got write working sufficient to bring Tumbler audio up, 
		  read still untested
*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <linux/i2c.h>

#include <linux/init.h>
#include <linux/mm.h>

#include <asm/prom.h>
#include <asm/feature.h>
#include <linux/nvram.h>

/* The Tumbler audio equalizer can be really slow sometimes */
#define POLL_SANITY 10000

/* PCI device */
#define VENDOR		0x106b
#define DEVICE		0x22

/*****    Protos    ******/

s32 keywest_access(struct i2c_adapter *adap, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int size, union i2c_smbus_data *data);
u32 keywest_func(struct i2c_adapter *adapter);

 /**/ 

struct keywest_iface {
	void *base;
	void *steps;
	void *mode;
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

void dump_ifaces(struct keywest_iface **ifaces);
int cleanup(struct keywest_iface **ifaces);

/***** End of Protos ******/


/** Vars **/

struct keywest_iface *ifaces = NULL;


/** Functions **/

/* keywest needs a small delay to defuddle itself after changing a setting */
void writeb_wait(int value, void *addr)
{
	writeb(value, addr);
	udelay(10);
}

int poll_interrupt(void *ISR)
{
	int i, res;
	for (i = 0; i < POLL_SANITY; i++) {
		udelay(100);

		res = readb(ISR) & 0x0F;
		if (res > 0) {
			/* printk("i2c-keywest: received interrupt: 0x%02X\n",res); */
			return res;
		}
	}

	if (i == POLL_SANITY) {
		printk("i2c-keywest: Sanity check failed!  Expected interrupt never happened.\n");
		return -1;
	}

	return -1;		/* Should never get here */
}


void keywest_reset(struct keywest_iface *ifaceptr)
{
	int interrupt_state;

	/* Clear all past interrupts */
	interrupt_state = readb(ifaceptr->ISR) & 0x0F;
	if (interrupt_state)
		writeb(interrupt_state,ifaceptr->ISR);
}

s32 keywest_access(struct i2c_adapter *adap, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int size, union i2c_smbus_data *data)
{

	struct keywest_iface *ifaceptr;
	int interrupt_state = 1;
	int ack;
	int error_state = 0;
	int len, i;		/* for block transfers */

	ifaceptr = (struct keywest_iface *) adap->data;

	keywest_reset(ifaceptr);

	/* Set up address and r/w bit */
	writeb_wait(((addr << 1) | (read_write == I2C_SMBUS_READ ?1:0)),
		    (void *) ifaceptr->addr);

	/* Set up 'sub address' which I'm guessing is the command field? */
	writeb_wait(command, (void *) ifaceptr->subaddr);
	
	/* Start sending address */
	writeb_wait(readb(ifaceptr->control) | 2, ifaceptr->control);
	interrupt_state = poll_interrupt(ifaceptr->ISR);
	ack = readb(ifaceptr->status) & 0x0F;

	if ((ack & 0x02) == 0) {
		printk("i2c-keywest: Ack Status on addr expected but got: 0x%02X on addr: 0x%02X\n",
		     ack, addr);
		return -1;
	} 

	/* Set ACK if reading */
	if (read_write == I2C_SMBUS_READ)
		writeb_wait(1 | readb(ifaceptr->control), ifaceptr->control);	
		
	switch (size) {
	    case I2C_SMBUS_BYTE_DATA:
		if (read_write == I2C_SMBUS_WRITE) {
			writeb_wait(data->byte, ifaceptr->data);
				    
			/* Clear interrupt and go */
			writeb_wait(interrupt_state, ifaceptr->ISR);	
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			if (interrupt_state < 0) 
				error_state = -1;

			if ((readb(ifaceptr->status) & 0x02) == 0) {
				printk("i2c-keywest: Ack Expected by not received(2)!\n");
				error_state = -1;
			}
			
			/* Send stop */
			writeb_wait(readb(ifaceptr->control) | 4, ifaceptr->control);

			writeb_wait(interrupt_state, ifaceptr->control);
			
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			if (interrupt_state < 0) 
				error_state = -1;
			writeb_wait(interrupt_state, ifaceptr->ISR);	
		} else {
			/* Clear interrupt and go */
			writeb_wait(interrupt_state, ifaceptr->ISR);	
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			data->byte = readb(ifaceptr->data);
			
			/* End read: clear ack */
			writeb_wait(0, ifaceptr->control);	
		}
		break;

	    case I2C_SMBUS_WORD_DATA:
		if (read_write == I2C_SMBUS_WRITE) {
			writeb_wait(data->word & 0x0ff, ifaceptr->data);

			/* Clear interrupt and go */
			writeb_wait(interrupt_state, ifaceptr->ISR);	
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			if (interrupt_state < 0)
				error_state = -1;

			if ((readb(ifaceptr->status) & 0x02) == 0) {
				printk("i2c-keywest: Ack Expected by not received(2)!\n");
				error_state = -1;
			}

			writeb_wait((data->word & 0x0ff00) >> 8,
				    ifaceptr->data);
	
			/* Clear interrupt and go */
			writeb_wait(interrupt_state, ifaceptr->ISR);
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			if (interrupt_state < 0) 
				error_state = -1;

			if ((readb(ifaceptr->status) & 0x02) == 0) {
				printk("i2c-keywest: Ack Expected by not received(3)!\n");
				error_state = -1;
			}

			/* Send stop */
			writeb_wait(readb(ifaceptr->control) | 4, ifaceptr->control);

			writeb_wait(interrupt_state, ifaceptr->control);
			
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			if (interrupt_state < 0) 
				error_state = -1;
			writeb_wait(interrupt_state, ifaceptr->ISR);	
		} else {
			/* Clear interrupt and go */
			writeb_wait(interrupt_state, ifaceptr->ISR);	
			interrupt_state =
			    poll_interrupt(ifaceptr->ISR);
			data->word =
			    (readb(ifaceptr->data) << 8);

			/* Send ack */
			writeb_wait(1, ifaceptr->control);	

			/* Clear interrupt and go */	
			writeb_wait(interrupt_state, ifaceptr->ISR);	
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			data->word |= (readb(ifaceptr->data));
		}
		break;

	case I2C_SMBUS_BLOCK_DATA:
		if (read_write == I2C_SMBUS_WRITE) {
			len = data->block[0];
			if (len < 0)
				len = 0;
			if (len > 32)
				len = 32;

			for(i=1; i<=len; i++) {
				writeb_wait(data->block[i],
					    ifaceptr->data);

				/* Clear interrupt and go */
				writeb_wait(interrupt_state,ifaceptr->ISR); 

				interrupt_state = poll_interrupt(ifaceptr->ISR);
				if ((readb(ifaceptr->status) & 0x02) == 0) {
					printk("i2c-keywest: Ack Expected by not received(block)!\n");
					error_state = -1;
				}
			}

			/* Send stop */
			writeb_wait(readb(ifaceptr->control) | 4, ifaceptr->control);

			writeb_wait(interrupt_state, ifaceptr->control);
			
			interrupt_state = poll_interrupt(ifaceptr->ISR);
			if (interrupt_state < 0) 
				error_state = -1;
			writeb_wait(interrupt_state, ifaceptr->ISR);	
		} else {
			for(i=1; i<=data->block[0]; i++) {
				/* Send ack */
				writeb_wait(1, ifaceptr->control);

				/* Clear interrupt and go */
				writeb_wait(interrupt_state, ifaceptr->ISR);
				interrupt_state = poll_interrupt(ifaceptr->ISR);
				data->block[i] = readb(ifaceptr->data);
			}
		}
		break;

	    default:
		printk("i2c-keywest: operation not supported\n");
		error_state = -1;
	}

	/* End read: clear ack */
	if (read_write == I2C_SMBUS_READ)
		writeb_wait(0, ifaceptr->control);

	return error_state;
}

u32 keywest_func(struct i2c_adapter * adapter)
{
	return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |
	       I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |
	       I2C_FUNC_SMBUS_BLOCK_DATA;
}

void keywest_inc(struct i2c_adapter *adapter)
{
	MOD_INC_USE_COUNT;
}

void keywest_dec(struct i2c_adapter *adapter)
{
	MOD_DEC_USE_COUNT;
}

/**** Internal Functions ****/

int init_iface(void *base, void *steps, struct keywest_iface **ifaces)
{
	struct i2c_adapter *keywest_adapter;
	struct keywest_iface **temp_hdl = ifaces;
	int res;

	if (ifaces == NULL) {
		printk("Ah!  Passed a null handle to init_iface");
		return -1;
	}

	while ((*temp_hdl) != NULL) {
		temp_hdl = &(*temp_hdl)->next;
	}

	*temp_hdl =
	    (struct keywest_iface *) kmalloc(sizeof(struct keywest_iface),
					     GFP_KERNEL);
	if (*temp_hdl == NULL) {
		printk("kmalloc failed on temp_hdl!");
		return -1;
	}

	(*temp_hdl)->next = NULL;
	base = ioremap((int) base, (int) steps * 8);
	(*temp_hdl)->base = base;
	(*temp_hdl)->steps = steps;
	(*temp_hdl)->mode = (void *) base;
	(*temp_hdl)->control = (void *) ((__u32) base + (__u32) steps);
	(*temp_hdl)->status = (void *) ((__u32) base + (__u32) steps * 2);
	(*temp_hdl)->ISR = (void *) ((__u32) base + (__u32) steps * 3);
	(*temp_hdl)->IER = (void *) ((__u32) base + (__u32) steps * 4);
	(*temp_hdl)->addr = (void *) ((__u32) base + (__u32) steps * 5);
	(*temp_hdl)->subaddr = (void *) ((__u32) base + (__u32) steps * 6);
	(*temp_hdl)->data = (void *) ((__u32) base + (__u32) steps * 7);
	
	keywest_adapter =
	    (struct i2c_adapter *) kmalloc(sizeof(struct i2c_adapter),
					   GFP_KERNEL);
	if (keywest_adapter == NULL) {
		printk("kmalloc failed on keywest_adapter!");
		return -1;
	}
	memset((void *) keywest_adapter, 0, (sizeof(struct i2c_adapter)));
	
	strcpy(keywest_adapter->name, "keywest i2c\0");
	keywest_adapter->id = I2C_ALGO_SMBUS;
	keywest_adapter->algo = &smbus_algorithm;
	keywest_adapter->algo_data = NULL;
	keywest_adapter->inc_use = keywest_inc;
	keywest_adapter->dec_use = keywest_dec;
	keywest_adapter->client_register = NULL;
	keywest_adapter->client_unregister = NULL;
	keywest_adapter->data = (void *) (*temp_hdl);
	
	(*temp_hdl)->i2c_adapt = keywest_adapter;
	
	if ((res = i2c_add_adapter((*temp_hdl)->i2c_adapt)) != 0) {
		printk("i2c-keywest.o: Adapter registration failed, module not inserted.\n");
		/* cleanup(); */
	}

	/* Now actually do the initialization of the device */

	/* Select standard sub mode 
	 
	   ie for <Address><Ack><Command><Ack><data><Ack>... style transactions
	 */
	writeb_wait(0x08, (*temp_hdl)->mode);

        /* Enable interrupts */
	writeb_wait(1 + 2 + 4 + 8, (*temp_hdl)->IER);

	keywest_reset(*temp_hdl);

	return res;
}

void dump_ifaces(struct keywest_iface **ifaces)
{
	if (ifaces == NULL) {
		printk("Ah!  Passed null handle to dump!\n");
		return;
	}
	if (*ifaces != NULL) {
		printk("Interface @%X,%X  Locations:\n", (u32) (*ifaces)->base,
		       (u32) (*ifaces)->steps);
		printk("  mode:%X control:%X status:%X ISR:%X IER:%X addr:%X subaddr:%X data:%X\n",
		     (u32) (*ifaces)->mode, (u32) (*ifaces)->control,
		     (u32) (*ifaces)->status, (u32) (*ifaces)->ISR, (u32) (*ifaces)->IER,
		     (u32) (*ifaces)->addr, (u32) (*ifaces)->subaddr, (u32) (*ifaces)->data);
		printk("Contents:\n");
		printk("  mode:0x%02X control: 0x%02X status:0x%02X ISR:0x%02X IER:0x%02X addr:0x%02X subaddr:0x%02X data:0x%02X\n",
		     readb((*ifaces)->mode), readb((*ifaces)->control),
		     readb((*ifaces)->status), readb((*ifaces)->ISR),
		     readb((*ifaces)->IER), readb((*ifaces)->addr),
		     readb((*ifaces)->subaddr), readb((*ifaces)->data));
		printk("I2C-Adapter:\n");
		printk("  name:%s\n", (*ifaces)->i2c_adapt->name);
		dump_ifaces(&(*ifaces)->next);
	} else {
		printk("End of ifaces.\n");
	}
}

int cleanup(struct keywest_iface **ifaces)
{
	int res = 0;

	if (ifaces == NULL) {
		printk("Ah!  Passed null handle to cleanup!\n");
		return 0;
	}
	
	if (*ifaces != NULL) {
		if (cleanup(&(*ifaces)->next) != 0)
			res = -1;

		printk("Cleaning up interface @%X,%X\n", (u32) (*ifaces)->base,
		       (u32) (*ifaces)->steps);
		       
		if (i2c_del_adapter((*ifaces)->i2c_adapt) != 0) {
			printk("i2c-keywest.o: i2c_del_adapter failed, module not removed\n");
			res = -1;
		}

		kfree(*ifaces);
		iounmap((*ifaces)->base);
		(*ifaces) = NULL;
	}

	return res;
}

#ifdef DEBUG
static void scan_of(char *dev_type)
{
	struct device_node *np = NULL;
	struct property *dp = NULL;
	int i;

	np = find_devices(dev_type);

	if (np == 0) {
		printk("No %s devices found.\n", dev_type);
	}
	
	while (np != 0) {
		printk("%s found: %s, with properties:\n", dev_type,
		       np->full_name);
		dp = np->properties;
		while (dp != 0) {
			printk("     %s = %s [ ", dp->name, dp->value);
			for (i = 0; i < dp->length; i++) {
				printk("%02X", (char) dp->value[i]);
				if (((i + 1) % 4) == 0) {
					printk(" ");
				}
			}
			printk("] (length=%d)\n", dp->length);
			dp = dp->next;
		}
		np = np->next;
	}
}
#endif

static int find_keywest(void)
{
	struct device_node *i2c_device;
	void **temp;
	void *base = NULL;
	void *steps = NULL;

#ifdef DEBUG
	scan_of("i2c");
#endif

	i2c_device = find_compatible_devices("i2c", "keywest");
	
	if (i2c_device == 0) {
		printk("No Keywest i2c devices found.\n");
		return -1;
	}
	
	while (i2c_device != 0) {
		printk("Keywest device found: %s\n",
		       i2c_device->full_name);
		temp =
		    (void **) get_property(i2c_device, "AAPL,address",
					   NULL);
		if (temp != NULL) {
			base = *temp;
		} else {
			printk("no 'address' prop!\n");
		}

		temp =
		    (void **) get_property(i2c_device, "AAPL,address-step",
					   NULL);
		if (temp != NULL) {
			steps = *temp;
		} else {
			printk("no 'address-step' prop!\n");
		}
		
		/* printk("Device found. base: %X steps: %X\n", base, steps); */

		if (init_iface(base, steps, &ifaces) != 0) {
			cleanup(&ifaces);
			return -ENODEV;
		}

		i2c_device = i2c_device->next;
	}
	
	if (ifaces == NULL) {
		printk("ifaces null! :'(");
	}
	
	/* dump_ifaces(&ifaces); */
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
