#ifndef ALGO_BIT_H
#define ALGO_BIT_H 1

/* --- Defines for bit-adapters ---------------------------------------	*/
#include "i2c.h"
/*
 * This struct contains the hw-dependent functions of bit-style adapters to 
 * manipulate the line states, and to init any hw-specific features. This is
 * only used if you have more than one hw-type of adapter running. 
 */
struct bit_adapter {
        char name[32];		/* give it a nice name 			*/
	unsigned int id;	/* not used yet, maybe later		*/
	void *data;		/* private data for lolevel routines	*/
	void (*setsda) (void *data, int state);
	void (*setscl) (void *data, int state);
	int  (*getsda) (void *data);
	int  (*getscl) (void *data);

	/* administrative calls */
	int (*client_register)(struct i2c_client *);
	int (*client_unregister)(struct i2c_client *);

	/* local settings */
	int udelay;
	int mdelay;
	int timeout;

};

extern struct bit_adapter *bit_adaps[];

#define BIT_ADAP_MAX	16

int i2c_bit_add_bus(struct bit_adapter *);
int i2c_bit_del_bus(struct bit_adapter *);

#endif /* ALGO_BIT_H */
