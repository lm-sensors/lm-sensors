/* experimental eeprom driver
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/malloc.h>

#include "i2c.h"			/* +++ simon */
#include "algo-bit.h"


int test=0;
/*
 *	Read the configuration EEPROM
 */
#define EEPROM_ADDR	0x50

#define EEPROM_FROM 0x50
#define EEPROM_TO   0x58 /*exclusive.*/

#define MEMSIZE 128      /* default eeprom memory size */
/* i2c_client identifies a single device that is connected to an 
 * 	i2c bus.
 */
struct i2c_driver eepromDriver;

struct {
	int size;  /* eeprom memory size*/
	unsigned int flags;
	unsigned char *memory;
} eepromData;

struct i2c_client eepromTmpl = {
	"Winnov EEPROM",
	0,
	0,
	EEPROM_ADDR,
	NULL,				/* the adapter we sit on	*/
	&eepromDriver,			/* and our access routines	*/
	NULL,				/* for the clients		*/
};


int eeprom_attach(struct i2c_adapter *adap)
{
	struct i2c_client *eeprom;
	unsigned char buf[256];
	struct i2c_msg msgs[] = {
		{EEPROM_ADDR, 0, 1, "\0", },
		{EEPROM_ADDR, I2C_M_RD, 256, buf, },
	};
	__u8 c=0;
	int i;
	int ret=0;

	printk("wnv-eeprom: Trying to attach to adapter %s \n",adap->name);
	printk("Looking for EEPROMs\n");
	eepromTmpl.adapter = adap;
	for (i=EEPROM_FROM;i<EEPROM_TO;i++) {
		/* we misuse the template as a "generic" eeprom client...*/
		eepromTmpl.addr = i;
		ret = i2c_master_send(&eepromTmpl,(unsigned char*)&c,1);
		if (ret!=1) {
			/* no device at this address. */
		  /*	printk("eeprom: failed for %x (%d)\n",i,ret);*/
			continue;
		}
		printk("eeprom: success with %x\n",i);
		if (test) {
			msgs[0].addr = i;
			msgs[1].addr = i;
			ret = i2c_transfer(adap, msgs, 2);
			if (ret != 2 ) {
				printk("eeprom: test read error with %d!\n",
				       i);
			} else {
				int j;
				for (j=0;j<256;j+=8)
					printk("%2d: %#02x %#02x %#02x %#02x  -  %#02x %#02x %#02x %#02x \n",
					j,buf[j+0],buf[j+1],buf[j+2],buf[j+3],
					buf[j+4],buf[j+5],buf[j+6],buf[j+7]);
			}
		}
		eeprom = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
		if (eeprom == NULL)
			return 0;
		memcpy(eeprom, &eepromTmpl, sizeof(struct i2c_client));
		i2c_attach_client(eeprom);
	}
	return 0;
}

int eeprom_detach(struct i2c_client *client)
{
	printk("wnv-eeprom: Client %s wants to detach\n",client->name);

	kfree(client);
	
	return 0;
}

#define LOAD_EEPROM 1
#if 0
#define SET_SIZE 0xee01     /* set memory size */
#define GET_SIZE 0xee02     /* get memory size */
#define GET_MEM  0xee03     /* load data from eeprom & pass memory pointer */
#define SET_MEM  0xee04     /* dump memory to eeprom */
#define OPT
#endif
int eeprom_command(struct i2c_client *client,unsigned int cmd, void *arg)
{
	switch(cmd) {
		case LOAD_EEPROM: {
#if 0
			__u8 c=0;
			int i, ret,d;
			i2c_master_send(client,(unsigned char*)&c,1);
			i2c_master_recv(client, (char*)&c,1);
			i2c_master_send(client,(unsigned char*)&c,1);
			ret = i2c_master_recv(client, (char*)&           ,
				sizeof(struct EEPROM));
			if (ret != sizeof(struct EEPROM)) {
				err_msg("wnv-eeprom: could only read %d bytes!\n"
					,ret);
				return -1;
			}
			for (d=0,i=0;i<sizeof(struct EEPROM); i++) {
				d+=((char *)(&dev->eeprom))[i];
			}
			if ((__u8)d != 0xff) {
				err_msg("wnv-eeprom: Checksum error in configuration EEPROM\n");
				return -1;
			}
			info_msg("wnv-eeprom: Identified: %s\n", dev->eeprom.szProduct);
#endif
		} break;
		default:
			printk("eeprom: unknown command %x!\n",cmd);
	}
	return 0;
}

struct i2c_driver eepromDriver = {
	"Videum EEPROM",
	-1,
	DF_NOTIFY,
	eeprom_attach,
	eeprom_detach,
	eeprom_command,
};

#ifdef MODULE
MODULE_PARM(test,"i");

/*EXPORT_NO_SYMBOLS;*/

int init_module(void)
{
	/* register drivers to the i2c bus admin. system.
	 * These get notified upon attachment of adapters &
	 * in turn create new clients that are connected to 
	 * these adapters...
	 */
	i2c_add_driver(&eepromDriver);
	return 0;
}

void cleanup_module(void)
{
	i2c_del_driver(&eepromDriver);
}
#endif /* MODULE*/
