/*
    bmcsensors.c - Part of lm_sensors, Linux kernel modules
                for hardware monitoring
                
    Copyright (c) 2002 Mark D. Studebaker <mdsxyz123@yahoo.com>

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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/ipmi.h>
#include "version.h"
#include "sensors.h"
#include <linux/init.h>

static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { 0, SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

SENSORS_INSMOD_1(bmcsensors);

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

struct bmcsensors_data {
	struct semaphore lock;
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u8 alarms;
};

#ifdef MODULE
static
#else
extern
#endif
int __init sensors_bmcsensors_init(void);
static int __init bmcsensors_cleanup(void);

static int bmcsensors_attach_adapter(struct i2c_adapter *adapter);
static int bmcsensors_detect(struct i2c_adapter *adapter, int address,
			  unsigned short flags, int kind);
static int bmcsensors_detach_client(struct i2c_client *client);
static int bmcsensors_command(struct i2c_client *client, unsigned int cmd,
			   void *arg);
static void bmcsensors_inc_use(struct i2c_client *client);
static void bmcsensors_dec_use(struct i2c_client *client);

static void bmcsensors_update_client(struct i2c_client *client);
static void bmcsensors_init_client(struct i2c_client *client);
static int bmcsensors_find(int *address);


static void bmcsensors_all(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
static void bmcsensors_alarms(struct i2c_client *client, int operation,
			   int ctl_name, int *nrels_mag, long *results);
static void bmcsensors_fan_div(struct i2c_client *client, int operation,
			    int ctl_name, int *nrels_mag, long *results);
static void bmcsensors_pwm(struct i2c_client *client, int operation,
			int ctl_name, int *nrels_mag, long *results);
void bmcsensors_get_sdr(u16 resid, u16 record, u8 offset);
void bmcsensors_get_reading(struct i2c_client *client, int i);

static int bmcsensors_id = 0;

static struct i2c_driver bmcsensors_driver = {
	/* name */ "BMC Sensors driver",
	/* id */ I2C_DRIVERID_BMCSENSORS,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &bmcsensors_attach_adapter,
	/* detach_client */ &bmcsensors_detach_client,
	/* command */ &bmcsensors_command,
	/* inc_use */ &bmcsensors_inc_use,
	/* dec_use */ &bmcsensors_dec_use
};

static struct bmcsensors_data bmc_data;
struct i2c_client bmc_client = {
	"BMC Sensors",
	1,                  /* fake should be 0 */
	0,
	0,
	NULL,   /* adapter */
	&bmcsensors_driver,
	& bmc_data,
	0
};

static bmcsensors_initialized;


#define MAX_SDR_ENTRIES 50
#define SDR_LIMITS 8
#define SDR_MAX_ID_LENGTH 16
#define SDR_MAX_UNPACKED_ID_LENGTH ((SDR_MAX_ID_LENGTH * 4 / 3) + 2)
struct sdrdata {
	/* reverse lookup from sysctl */
	int sysctl;
	/* retrieved from SDR, not expected to change */
	u8 stype;
	u8 number;
	u8 capab;
	u16 thresh_mask;
	u8 linear;
	s16 m;
	s16 b;
	u8 k;
	u8 nominal;
	u8 limits[SDR_LIMITS];
	int lim1, lim2;
	u8 lim1_write, lim2_write;
	u8 string_type;
	u8 id_length;
	u8 id[SDR_MAX_ID_LENGTH];
	/* retrieved from reading */
	u8 reading;
	u8 status;
	u8 thresholds;
};
static struct sdrdata sdrd[MAX_SDR_ENTRIES];
static int sdrd_count;

#define MAX_PROC_ENTRIES (MAX_SDR_ENTRIES + 5)
#define MAX_PROCNAME_SIZE 8
static ctl_table *bmcsensors_dir_table;
static char *bmcsensors_proc_name_pool;

#define IPMI_SDR_SIZE 67
static int ipmi_sdr_partial_size = IPMI_SDR_SIZE;
static struct ipmi_msg tx_message;	/* send message */
static unsigned char tx_msg_data[IPMI_MAX_MSG_LENGTH + 50];
static unsigned char rx_msg_data[IPMI_MAX_MSG_LENGTH + 50]; /* sloppy */
int rx_msg_data_offset;
static long msgid;		/* message ID */
static u16 resid;

enum states {STATE_INIT, STATE_RESERVE, STATE_SDR, STATE_SDRPARTIAL, STATE_READING, STATE_DONE};
int state;
static int receive_counter;


/* IPMI Message defs */
/* Network Function Codes */
#define IPMI_NETFN_SENSOR	0x04
#define IPMI_NETFN_STORAGE	0x0A
/* Commands */
#define IPMI_RESERVE_SDR		0x22
#define IPMI_GET_SDR		0x23
#define IPMI_GET_SENSOR_STATE_READING		0x2D

/* SDR defs */
#define STYPE_TEMP	0x01
#define STYPE_VOLT	0x02
#define STYPE_CURR	0x03
#define STYPE_FAN	0x04

/* get rid of this ***************/
#define STYPE_MAX	0x29		/* the last sensor type we are interested in */
static u8 bmcs_count[STYPE_MAX + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const u8 bmcs_max[STYPE_MAX + 1] = {
0, 10, 10, 10, 10, 0, 0, 0}; /* more zeros*/

/************************************/


/* unpack based on string type, convert to normal, null terminate */
static void ipmi_sprintf(u8 * to, u8 * from, u8 type, u8 length)
{
	static const u8 *bcdplus = "0123456789 -.:,_";
	int i;

	switch (type) {
		case 0: 	/* unicode */		
			for(i = 0; i < length; i++)
				*to++ = *from++ & 0x7f;
			*to = 0;
			break;
		case 1: 	/* BCD Plus */		
			for(i = 0; i < length; i++)
				*to++ = bcdplus[*from++ & 0x0f];
			*to = 0;
			break;
		case 2: 	/* packed ascii */ /* if not a mult. of 3 this will run over */     
			for(i = 0; i < length; i += 3) {
				*to++ = *from & 0x3f;
				*to++ = *from++ >> 6 | ((*from & 0xf)  << 2);
				*to++ = *from++ >> 4 | ((*from & 0x3)  << 4);
				*to++ = (*from++ >> 2) & 0x3f;
			}
			*to = 0;
			break;
		case 3: 	/* normal */		
			if(length > 1)
				memcpy(to, from, length);
			to[length] = 0;
			break;
	}
}

static const char * threshold_text[] = {
	"upper non-recoverable threshold",
	"upper critical threshold",
	"upper non-critical threshold",
	"lower non-recoverable threshold",
	"lower critical threshold",
	"lower non-critical threshold",
	"positive-going hysteresis", /* not used */
	"negative-going hysteresis"
};

/* select two out of the 8 possible readable thresholds, and place indexes into the limits
   array into lim1 and lim2. Set writable flags */
static void bmcsensors_select_thresholds(int i)
{
	u8 capab = sdrd[i].capab;
	u16 mask = sdrd[i].thresh_mask;

	sdrd[i].lim1 = -1;
	sdrd[i].lim2 = -1;
	sdrd[i].lim1_write = 0;
	sdrd[i].lim2_write = 0;

	if(((capab & 0x0c) == 0x04) ||	/* readable thresholds ? */
	   ((capab & 0x0c) == 0x08)) {
		/* select upper threshold */
		if(mask & 0x10) {			/* upper crit */
			sdrd[i].lim1 = 1;
			if((capab & 0x0c) == 0x08 && (mask & 0x1000))
				sdrd[i].lim1_write = 1;
		}
		else if(mask & 0x20) {		/* upper non-recov */
			sdrd[i].lim1 = 0;
			if((capab & 0x0c) == 0x08 && (mask & 0x2000))
				sdrd[i].lim1_write = 1;
		}
		else if(mask & 0x08) {		/* upper non-crit */
			sdrd[i].lim1 = 2;
			if((capab & 0x0c) == 0x08 && (mask & 0x0800))
				sdrd[i].lim1_write = 1;
		}

		/* select lower threshold */
		if(((capab & 0x30) == 0x10) ||	/* readable hysteresis ? */
		   ((capab & 0x30) == 0x20))	/* neg hyst */
			sdrd[i].lim2 = 7;
		else if(mask & 0x02) {		/* lower crit */
			sdrd[i].lim2 = 4;
			if((capab & 0x0c) == 0x08 && (mask & 0x0200))
				sdrd[i].lim2_write = 1;
		}
		else if(mask & 0x04) {		/* lower non-recov */
			sdrd[i].lim2 = 3;
			if((capab & 0x0c) == 0x08 && (mask & 0x0400))
				sdrd[i].lim2_write = 1;
		}
		else if(mask & 0x01) {		/* lower non-crit */
			sdrd[i].lim2 = 5;
			if((capab & 0x0c) == 0x08 && (mask & 0x0100))
				sdrd[i].lim2_write = 1;
		}
	}

	if(sdrd[i].lim1 >= 0)
		printk(KERN_INFO "bmcsensors.o: sensor %d: using %s for upper limit\n",
			i, threshold_text[sdrd[i].lim1]);
/*
	else
		printk(KERN_INFO "bmcsensors.o: sensor %d: no readable upper limit\n", i);
*/
	if(sdrd[i].lim2 >= 0)
		printk(KERN_INFO "bmcsensors.o: sensor %d: using %s for lower limit\n",
			i, threshold_text[sdrd[i].lim2]);
/*
	else
		printk(KERN_INFO "bmcsensors.o: sensor %d: no readable lower limit\n", i);
*/
}

/* After we have received all the SDR entries and picked out the ones
   we are interested in, build a table of the /proc entries and register.
*/
static void bmcsensors_build_proc_table()
{
	int i;
	int temps = 0, volts = 0, currs = 0, fans = 0;
	u8 id[SDR_MAX_UNPACKED_ID_LENGTH];

	if(!(bmcsensors_dir_table = kmalloc((sdrd_count + 1) * sizeof(struct ctl_table), GFP_KERNEL))) {
		printk(KERN_ERR "bmcsensors.o: no memory\n");
		return; /* do more than this */	/* ^^ add 1 or more for alarms, etc. */
	}
	if(!(bmcsensors_proc_name_pool = kmalloc((sdrd_count + 0) * MAX_PROCNAME_SIZE, GFP_KERNEL))) {
		kfree(bmcsensors_dir_table);
		printk(KERN_ERR "bmcsensors.o: no memory\n");
		return; /* do more than this */	/* ^^ add 1 or more for alarms, etc. */
	}

	for(i = 0; i < sdrd_count; i++) {
		bmcsensors_dir_table[i].procname = bmcsensors_proc_name_pool + (i * MAX_PROCNAME_SIZE);
		bmcsensors_dir_table[i].data = NULL;
		bmcsensors_dir_table[i].maxlen = 0;
		bmcsensors_dir_table[i].child = NULL;
		bmcsensors_dir_table[i].proc_handler = &i2c_proc_real;
		bmcsensors_dir_table[i].strategy = &i2c_sysctl_real;
		bmcsensors_dir_table[i].de = NULL;

		switch(sdrd[i].stype) {
			case(STYPE_TEMP) :
				bmcsensors_dir_table[i].ctl_name = BMC_SYSCTL_TEMP1 + temps;
				sprintf((char *)bmcsensors_dir_table[i].procname, "temp%d", ++temps);
				bmcsensors_dir_table[i].extra1 = &bmcsensors_all;
				break;
			case(STYPE_VOLT) :
				bmcsensors_dir_table[i].ctl_name = BMC_SYSCTL_IN1 + volts;
				sprintf((char *)bmcsensors_dir_table[i].procname, "in%d", ++volts);
				bmcsensors_dir_table[i].extra1 = &bmcsensors_all;
				break;
			case(STYPE_CURR) :
				bmcsensors_dir_table[i].ctl_name = BMC_SYSCTL_CURR1 + currs;
				sprintf((char *)bmcsensors_dir_table[i].procname, "curr%d", ++currs);
				bmcsensors_dir_table[i].extra1 = &bmcsensors_all;
				break;
			case(STYPE_FAN) :
				bmcsensors_dir_table[i].ctl_name = BMC_SYSCTL_FAN1 + fans;
				sprintf((char *)bmcsensors_dir_table[i].procname, "fan%d", ++fans);
				bmcsensors_dir_table[i].extra1 = &bmcsensors_all;
				break;
			default: /* ?? */
				printk(KERN_INFO "bmcsensors.o: unk stype\n");
				continue;
		}
		sdrd[i].sysctl = bmcsensors_dir_table[i].ctl_name;
		if(sdrd[i].linear != 0) {
			printk(KERN_INFO "bmcsensors.o: sensor %d: (type 0x%x) nonlinear function 0x%.2x unsupported\n",
				i, sdrd[i].stype, sdrd[i].linear);
		}
		ipmi_sprintf(id, sdrd[i].id, sdrd[i].string_type, sdrd[i].id_length);
		printk(KERN_INFO "bmcsensors.o: registering sensor %d: '%s' (type 0x%.2x) as %s "
			"(m=%d; b=%d; k1=%d; k2=%d; cap=0x%.2x; mask=0x%.4x)\n",
			i, id, sdrd[i].stype, bmcsensors_dir_table[i].procname,
			sdrd[i].m, sdrd[i].b,sdrd[i].k & 0xf, sdrd[i].k >> 4,
			sdrd[i].capab, sdrd[i].thresh_mask);
		bmcsensors_select_thresholds(i);
		if(sdrd[i].lim1_write || sdrd[i].lim2_write)
			bmcsensors_dir_table[i].mode = 0644;
		else
			bmcsensors_dir_table[i].mode = 0444;
	}
	bmcsensors_dir_table[sdrd_count].ctl_name = 0;

	if ((i = i2c_register_entry(&bmc_client, "bmc",
					bmcsensors_dir_table,
					THIS_MODULE)) < 0) {
		printk(KERN_INFO "bmcsensors.o: i2c registration failed.\n");
		kfree(bmcsensors_dir_table);
		kfree(bmcsensors_proc_name_pool);
		return;
	}
	bmcsensors_initialized = 3;
	bmc_data.sysctl_id = i;

	bmcsensors_init_client(&bmc_client);
	printk(KERN_INFO "bmcsensors.o: registered %d temp, %d volt, %d current, %d fan sensors\n",
			temps, volts, currs, fans);
	bmcsensors_update_client(&bmc_client);
}


/* Process a sensor reading response */
static int bmcsensors_rcv_reading_msg(struct ipmi_msg *msg)
{
	if(receive_counter >= sdrd_count) {
		/* shouldn't happen */
		receive_counter = 0;
		return STATE_DONE;
	}
	sdrd[receive_counter].reading = msg->data[1];
	sdrd[receive_counter].status = msg->data[2];
	sdrd[receive_counter].thresholds = msg->data[3];
#ifdef DEBUG
	printk(KERN_DEBUG "bmcsensors.o: sensor %d (type %d) reading %d\n",
		receive_counter, sdrd[receive_counter].stype, msg->data[1]);
#endif
	if(++receive_counter >= sdrd_count) {
		receive_counter = 0;
		return STATE_DONE;
	}
	bmcsensors_get_reading(&bmc_client, receive_counter); /* don't really need to pass client */
	return STATE_READING; 
}

/* Process a SDR response */
static int bmcsensors_rcv_sdr_msg(struct ipmi_msg *msg, int state)
{
	u16 record, nextrecord;
	int version, type, length, owner, lun, number, entity, instance, init;
	int stype, code;
	int id_length;
	int i;
	int rstate = STATE_SDR;
	struct ipmi_msg txmsg;
	unsigned char * data;


	if(msg->data[0] != 0) {
		/* cut request in half and try again */
		ipmi_sdr_partial_size /= 2;
		if(ipmi_sdr_partial_size < 8) {
			printk(KERN_INFO "bmcsensors.o: IPMI buffers too small, giving up\n");
			return STATE_DONE;
		}
		printk(KERN_INFO "bmcsensors.o: Reducing SDR request size to %d\n", ipmi_sdr_partial_size);
		bmcsensors_get_sdr(resid, 0, 0);
		return STATE_SDR;
	}
	if(ipmi_sdr_partial_size < IPMI_SDR_SIZE) {
		if(rx_msg_data_offset == 0) {
			memcpy(rx_msg_data, msg->data, ipmi_sdr_partial_size + 3);
			rx_msg_data_offset = ipmi_sdr_partial_size + 3;
		} else {
			memcpy(rx_msg_data + rx_msg_data_offset, msg->data + 3, ipmi_sdr_partial_size);
			rx_msg_data_offset += ipmi_sdr_partial_size;
		}
		if(rx_msg_data_offset > IPMI_SDR_SIZE + 3) {
			/* got last chunk */
			rx_msg_data_offset =  0;
			data = rx_msg_data;
		} else {
			/* get more */
			record = (rx_msg_data[4] << 8) | rx_msg_data[3];
			bmcsensors_get_sdr(resid, record, rx_msg_data_offset - 3);
			return STATE_SDR;
		}
	} else {
		data = msg->data;	/* got it in one chunk */
	}

	nextrecord = (data[2] << 8) | data[1];
	version = data[5];
	type = data[6];
	if(type == 1 || type == 2) {		/* known SDR type */
/*
		owner = data[8];
		lun = data[9];
		entity = data[11];
		init = data[13];
*/
		stype = data[15];

		if(stype <= STYPE_MAX) {	/* known sensor type */
			if(bmcs_count[stype] >= bmcs_max[stype]) {
				if(bmcs_max[stype] > 0) {
					printk(KERN_INFO "bmcsensors.o: Limit of %d exceeded for sensor type 0x%x\n", bmcs_max[stype], stype);
#ifdef DEBUG
				} else {
					printk(KERN_INFO "bmcsensors.o: Ignoring unsupported sensor type 0x%x\n", stype);
#endif
				}
			} else {
				if(sdrd_count >= MAX_SDR_ENTRIES) {
					printk(KERN_INFO "bmcsensors.o: Limit of %d exceeded for total sensors\n", MAX_SDR_ENTRIES);
				} else {
					/* add SDR database entry */
					sdrd[sdrd_count].stype = stype;
					sdrd[sdrd_count].number = data[10];
					sdrd[sdrd_count].capab = data[14];
					sdrd[sdrd_count].thresh_mask = (data[22] << 8) | data[21];
					if(type == 1) {
						sdrd[sdrd_count].linear = data[26] & 0x7f;
						sdrd[sdrd_count].m = data[27];
						sdrd[sdrd_count].m |= ((u16) (data[28] & 0xc0)) << 2;
						if(sdrd[sdrd_count].m & 0x0200)
							sdrd[sdrd_count].m |= 0xfc00;	/* sign extend */
						sdrd[sdrd_count].b = data[29];
						sdrd[sdrd_count].b |= ((u16) (data[30] & 0xc0)) << 2;
						if(sdrd[sdrd_count].b & 0x0200)
							sdrd[sdrd_count].b |= 0xfc00;	/* sign extend */
						sdrd[sdrd_count].k = data[32];
						sdrd[sdrd_count].nominal = data[34];
						for(i = 0; i < SDR_LIMITS; i++)		/* assume readable */
							sdrd[sdrd_count].limits[i] = data[39 + i];
						sdrd[sdrd_count].string_type = data[50] >> 6;
						id_length = data[50] & 0x1f;
						if(id_length > 0)
							memcpy(sdrd[sdrd_count].id, &data[51], id_length);
						sdrd[sdrd_count].id_length = id_length;
					} else {
						sdrd[sdrd_count].m = 1;
						sdrd[sdrd_count].b = 0;
						sdrd[sdrd_count].k = 0;
						sdrd[sdrd_count].string_type = data[34] >> 6;
						id_length = data[34] & 0x1f;
						if(id_length > 0)
							memcpy(sdrd[sdrd_count].id, &data[35], id_length);
						sdrd[sdrd_count].id_length = id_length;
						/* limits?? */
					}
					bmcs_count[stype]++;
					sdrd_count++;
				}
			}
		}
/*
				} else {
					printk(KERN_INFO "bmcsensors.o: Ignoring sensor type 0x%x\n", stype);
				}
*/
			/*
				printk(KERN_INFO "bmcsensors.o: STATE_SDR: record = 0x%x; version = 0x%x, type = 0x%x; length=0x%x\n",
				record, version, type, length);
				printk(KERN_INFO "bmcsensors.o: STATE_SDR: owner = 0x%x; lun = 0x%x, number = 0x%x; entity=0x%x\n",
				owner, lun, number, entity);
				printk(KERN_INFO "bmcsensors.o: STATE_SDR: instance = 0x%x; init = 0x%x, capab = 0x%x; stype=0x%x\n",
				instance, init, capab, stype);
				printk(KERN_INFO "bmcsensors.o: STATE_SDR: code = 0x%x\n",
				code);
				printk(KERN_INFO "bmcsensors.o: STATE_SDR: next = 0x%x\n", nextrecord);
			} else {
				printk(KERN_INFO "bmcsensors.o: unknown STATE_SDR type 0x%x\n", type);
			*/
	}
			
	if(nextrecord == 0xFFFF) {
		rstate = STATE_READING;
		if(sdrd_count == 0) {
			printk(KERN_INFO "bmcsensors.o: No recognized sensors found.\n");
		} else {
			printk(KERN_INFO "bmcsensors.o: found %d temp, %d volt, %d current, %d fan sensors\n",
bmcs_count[1], bmcs_count[2], bmcs_count[3], bmcs_count[4]);
			bmcsensors_build_proc_table();
		}
	} else {
		bmcsensors_get_sdr(resid, nextrecord, 0);
	}
	return rstate;
}

/* Process incoming messages based on internal state */
static void bmcsensors_rcv_msg(struct ipmi_msg *msg)
{

	switch(state) {
		case STATE_INIT:
		case STATE_RESERVE:
			resid = (msg->data[2] << 8) || msg->data[1];
			bmcsensors_get_sdr(resid, 0, 0);
			state = STATE_SDR;
			break;

		case STATE_SDR:
		case STATE_SDRPARTIAL:
			state = bmcsensors_rcv_sdr_msg(msg, state);
			break;

		case STATE_READING:
			state = bmcsensors_rcv_reading_msg(msg);
			break;

		case STATE_DONE:
			break;

		default:
			state = STATE_INIT;
	}
}


/* Incoming message handler */
static void bmcsensors_msg_handler(struct ipmi_recv_msg *msg,
				   void * handler_data)
{
	if (msg->msg.data[0] != 0 && msg->msg.data[0] != 0xca) {
		printk(KERN_ERR "BMCsensors response: Error %x on cmd %x; state = %d; probably fatal.\n",
		       msg->msg.data[0], msg->msg.cmd, state);
	} else {
		bmcsensors_rcv_msg(&(msg->msg));
	}       
	ipmi_free_recv_msg(msg);
}

/************** Message Sending **************/

/* Send an IPMI message */
static void bmcsensors_send_message(struct ipmi_msg * msg)
{
#ifdef DEBUG
	printk(KERN_INFO "bmcsensors.o: Send BMC msg, cmd: 0x%x\n",
		       msg->cmd);
#endif
	bmcclient_i2c_send_message(&bmc_client, msgid++, msg);

}

/* Compose and send a "reserve SDR" message */
void bmcsensors_reserve_sdr(void)
{
	tx_message.netfn = IPMI_NETFN_STORAGE;
	tx_message.cmd = IPMI_RESERVE_SDR;
	tx_message.data_len = 0;
	tx_message.data = NULL;
	bmcsensors_send_message(&tx_message);
}

/* Componse and send a "get SDR" message */
void bmcsensors_get_sdr(u16 resid, u16 record, u8 offset)
{
	tx_message.netfn = IPMI_NETFN_STORAGE;
	tx_message.cmd = IPMI_GET_SDR;
	tx_message.data_len = 6;
	tx_message.data = tx_msg_data;
	tx_msg_data[0] = resid & 0xff;
	tx_msg_data[1] = resid >> 8;
	tx_msg_data[2] = record & 0xff;
	tx_msg_data[3] = record >> 8;
	tx_msg_data[4] = offset;
	tx_msg_data[5] = ipmi_sdr_partial_size;
	bmcsensors_send_message(&tx_message);
}

/* Compose and send a "get sensor reading" message */
void bmcsensors_get_reading(struct i2c_client *client, int i)
{
	tx_message.netfn = IPMI_NETFN_SENSOR;
	tx_message.cmd = IPMI_GET_SENSOR_STATE_READING;
	tx_message.data_len = 1;
	tx_message.data = tx_msg_data;
	tx_msg_data[0] = sdrd[i].number;
	bmcsensors_send_message(&tx_message);
}

/**************** Initialization ****************/

int bmcsensors_attach_adapter(struct i2c_adapter *adapter)
{
/*
  not really i2c or isa so i2c_detect not required
*/
	printk(KERN_INFO "bmcsensors.o: in bmcsensors_detect() for ID %x\n",
		adapter->algo->id);

	if(adapter->algo->id != I2C_ALGO_IPMI) {
		return 0;
	}

	if(bmcsensors_initialized >= 2) {
		printk(KERN_INFO "bmcsensors.o: limit of 1\n");
		return 0;
	}

	return bmcsensors_detect(adapter, 0, 0, 0);
}

int bmcsensors_detect(struct i2c_adapter *adapter, int address,
		   unsigned short flags, int kind)
{
	int err, i;

	bmc_client.id = 0;
	bmc_client.adapter = adapter;
	bmc_data.valid = 0;
/*
	init_MUTEX(bmc_data.update_lock);
*/

	if ((err = i2c_attach_client(&bmc_client))) {
		printk(KERN_ERR "attach client error in bmcsensors_detect()\n");
		return err;
	}
	bmcsensors_initialized = 2;

	/* initialize some key data */
	state = STATE_INIT;
	sdrd_count = 0;
	receive_counter = 0;
	rx_msg_data_offset = 0;
	ipmi_sdr_partial_size = IPMI_SDR_SIZE;
	for(i = 0; i <= STYPE_MAX; i++)
		bmcs_count[i] = 0;

	/* send our first message, which kicks things off */
	bmcsensors_reserve_sdr();
	/* don't call i2c_register_entry until we scan the SDR's */
	return 0;
}

int bmcsensors_detach_client(struct i2c_client *client)
{
	int err;

	if(bmcsensors_initialized >= 3) {
		kfree(bmcsensors_dir_table);
		kfree(bmcsensors_proc_name_pool);
		i2c_deregister_entry(((struct bmcsensors_data *) (client->data))->
				 sysctl_id);
	}

	if ((err = i2c_detach_client(client))) {
/*
		printk
		    ("bmcsensors.o: Client deregistration failed, client not detached.\n");
*/
		return err;
	}

	return 0;
}

int bmcsensors_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	bmcsensors_msg_handler((struct ipmi_recv_msg *) arg, NULL);
	return 0;
}

void bmcsensors_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void bmcsensors_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

void bmcsensors_init_client(struct i2c_client *client)
{
/*******************************************/



}

void bmcsensors_update_client(struct i2c_client *client)
{
	struct bmcsensors_data *data = client->data;
	int i;

/*
	down(&data->update_lock);
*/

	if ((jiffies - data->last_updated > 3 * HZ) ||
	    (jiffies < data->last_updated) || !data->valid) {
		/* don't start an update cycle if one already in progress */
		if(state != STATE_READING) {
			state = STATE_READING;
			printk(KERN_DEBUG "bmcsensors.o: Starting update\n");
			bmcsensors_get_reading(client, 0);
		}
	}

/*
	up(&data->update_lock);
*/
}


/************* /proc callback helper functions *********/

/* need better way to map from sysctl to sdrd record number */
int find_sdrd(int sysctl)
{
	int i;

	for(i = 0; i < sdrd_count; i++)
		if(sdrd[i].sysctl == sysctl)
			return i;
	return -1;
}

/* IPMI V1.5 Section 30 */
static const int exps[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};

static int decplaces(int i)
{
	u8 k2;

	k2 = sdrd[i].k >> 4;
	if(k2 < 8)
		return 0;
	else
		return 16 - k2;
}

static long convert_value(u8 value, int i)
{
	u8 k1, k2;
	long r;

	k1 = sdrd[i].k & 0x0f;
	k2 = sdrd[i].k >> 4;

	r = value * sdrd[i].m;
	if(k1 < 8)
		r += sdrd[i].b * exps[k1];
	else
		r += sdrd[i].b / exps[16 - k1];
	if(k2 < 8)
		r *= exps[k2];
/*
	taken care of by nrels_mag
	else
		r /= exps[16 - k2];
*/
	return r;
}


/************** /proc callbacks *****************/

void bmcsensors_all(struct i2c_client *client, int operation, int ctl_name,
		 int *nrels_mag, long *results)
{
	int i;
	struct bmcsensors_data *data = client->data;
	int nr = ctl_name - BMC_SYSCTL_TEMP1 + 1;

	if((i = find_sdrd(ctl_name)) < 0) {
		*nrels_mag = 0;
		return;		
	}
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = decplaces(i);
	else if (operation == SENSORS_PROC_REAL_READ) {
		bmcsensors_update_client(client);
		if(sdrd[i].stype == STYPE_FAN) { /* lower limit only */
			if(sdrd[i].lim2 >= 0)
				results[0] = convert_value(sdrd[i].limits[sdrd[i].lim2], i);
			else
				results[0] = 0;
			results[1] = convert_value(sdrd[i].reading, i);
			*nrels_mag = 2;
		} else {
			if(sdrd[i].lim1 >= 0)
				results[0] = convert_value(sdrd[i].limits[sdrd[i].lim1], i);
			else
				results[0] = 0;
			if(sdrd[i].lim2 >= 0)
				results[1] = convert_value(sdrd[i].limits[sdrd[i].lim2], i);
			else
				results[1] = 0;
			results[2] = convert_value(sdrd[i].reading, i);
			*nrels_mag = 3;
		}
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		if (*nrels_mag >= 1) {
		}
	}
}

void bmcsensors_alarms(struct i2c_client *client, int operation, int ctl_name,
		    int *nrels_mag, long *results)
{
	struct bmcsensors_data *data = client->data;
#if 0
	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		bmcsensors_update_client(client);
		results[0] = data->alarms;
		*nrels_mag = 1;
	}
#endif
}

int __init sensors_bmcsensors_init(void)
{
	int res, addr;

	printk(KERN_INFO "bmcsensors.o version %s (%s)\n", LM_VERSION, LM_DATE);
	bmcsensors_initialized = 0;

	if ((res = i2c_add_driver(&bmcsensors_driver))) {
		printk(KERN_ERR
		   "bmcsensors.o: Driver registration failed, module not inserted.\n");
		bmcsensors_cleanup();
		return res;
	}
	bmcsensors_initialized = 1;
	return 0;
}

int __init bmcsensors_cleanup(void)
{
	int res;

	if (bmcsensors_initialized >= 1) {
		if ((res = i2c_del_driver(&bmcsensors_driver))) {
			printk(KERN_WARNING
			    "bmcsensors.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
		bmcsensors_initialized--;
	}
	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Mark D. Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("IPMI BMC sensors");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

int init_module(void)
{
	return sensors_bmcsensors_init();
}

int cleanup_module(void)
{
	return bmcsensors_cleanup();
}

#endif				/* MODULE */
