/*
	Taken from arch/i386/kernel/dmi_scan.c.
	Changes dmi_ident to be non-static so we can access.
*/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/apm_bios.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/pm.h>
#include <asm/keyboard.h>
#include <asm/system.h>
#include <linux/bootmem.h>
#include "version.h"
#include "dmi_scan.h"

struct dmi_header
{
	u8	type;
	u8	length;
	u16	handle;
};

#define dmi_printk(x)
//#define dmi_printk(x) printk x

static char * __init dmi_string(struct dmi_header *dm, u8 s)
{
	u8 *bp=(u8 *)dm;
	bp+=dm->length;
	if(!s)
		return "";
	s--;
	while(s>0)
	{
		bp+=strlen(bp);
		bp++;
		s--;
	}
	return bp;
}

/*
 *	We have to be cautious here. We have seen BIOSes with DMI pointers
 *	pointing to completely the wrong place for example
 */
 
static int __init dmi_table(u32 base, int len, int num, void (*decode)(struct dmi_header *))
{
	u8 *buf;
	struct dmi_header *dm;
	u8 *data;
	int i=1;
		
	buf = ioremap(base, len);
	if(buf==NULL)
		return -1;

	data = buf;

	/*
 	 *	Stop when we see al the items the table claimed to have
 	 *	OR we run off the end of the table (also happens)
 	 */
 
	while(i<num && (data - buf) < len)
	{
		dm=(struct dmi_header *)data;
	
		/*
		 *	Avoid misparsing crud if the length of the last
	 	 *	record is crap 
		 */
		if((data-buf+dm->length) >= len)
			break;
		decode(dm);		
		data+=dm->length;
		/*
		 *	Don't go off the end of the data if there is
	 	 *	stuff looking like string fill past the end
	 	 */
		while((data-buf) < len && (*data || data[1]))
			data++;
		data+=2;
		i++;
	}
	iounmap(buf);
	return 0;
}


static int __init dmi_iterate(void (*decode)(struct dmi_header *))
{
	unsigned char buf[20];
	long fp=0xE0000L;
	fp -= 16;

#ifdef CONFIG_SIMNOW
	/*
 	 *	Skip on x86/64 with simnow. Will eventually go away
 	 *	If you see this ifdef in 2.6pre mail me !
 	 */
	return -1;
#endif
 	
	while( fp < 0xFFFFF)
	{
		fp+=16;
		isa_memcpy_fromio(buf, fp, 20);
		if(memcmp(buf, "_DMI_", 5)==0)
		{
			u16 num=buf[13]<<8|buf[12];
			u16 len=buf[7]<<8|buf[6];
			u32 base=buf[11]<<24|buf[10]<<16|buf[9]<<8|buf[8];

			dmi_printk((KERN_INFO "DMI %d.%d present.\n",
				buf[14]>>4, buf[14]&0x0F));
			dmi_printk((KERN_INFO "%d structures occupying %d bytes.\n",
				buf[13]<<8|buf[12],
				buf[7]<<8|buf[6]));
			dmi_printk((KERN_INFO "DMI table at 0x%08X.\n",
				buf[11]<<24|buf[10]<<16|buf[9]<<8|buf[8]));
			if(dmi_table(base,len, num, decode)==0)
				return 0;
		}
	}
	return -1;
}


char *dmi_ident[DMI_STRING_MAX];

/*
 *	Save a DMI string
 */
 
static void __init dmi_save_ident(struct dmi_header *dm, int slot, int string)
{
	char *d = (char*)dm;
	char *p = dmi_string(dm, d[string]);
	if(p==NULL || *p == 0)
		return;
	if (dmi_ident[slot])
		return;
	dmi_ident[slot] = kmalloc(strlen(p)+1, GFP_KERNEL);
	if(dmi_ident[slot])
		strcpy(dmi_ident[slot], p);
	else
		printk(KERN_ERR "dmi_save_ident: out of memory.\n");
}

/*
 *	Process a DMI table entry. Right now all we care about are the BIOS
 *	and machine entries. For 2.5 we should pull the smbus controller info
 *	out of here.
 */

static void __init dmi_decode(struct dmi_header *dm)
{
	u8 *data = (u8 *)dm;
	char *p;
	
	switch(dm->type)
	{
		case  0:
			p=dmi_string(dm,data[4]);
			if(*p)
			{
				dmi_printk(("BIOS Vendor: %s\n", p));
				dmi_save_ident(dm, DMI_BIOS_VENDOR, 4);
				dmi_printk(("BIOS Version: %s\n", 
					dmi_string(dm, data[5])));
				dmi_save_ident(dm, DMI_BIOS_VERSION, 5);
				dmi_printk(("BIOS Release: %s\n",
					dmi_string(dm, data[8])));
				dmi_save_ident(dm, DMI_BIOS_DATE, 8);
			}
			break;
			
		case 1:
			p=dmi_string(dm,data[4]);
			if(*p)
			{
				dmi_printk(("System Vendor: %s.\n",p));
				dmi_save_ident(dm, DMI_SYS_VENDOR, 4);
				dmi_printk(("Product Name: %s.\n",
					dmi_string(dm, data[5])));
				dmi_save_ident(dm, DMI_PRODUCT_NAME, 5);
				dmi_printk(("Version %s.\n",
					dmi_string(dm, data[6])));
				dmi_save_ident(dm, DMI_PRODUCT_VERSION, 6);
				dmi_printk(("Serial Number %s.\n",
					dmi_string(dm, data[7])));
			}
			break;
		case 2:
			p=dmi_string(dm,data[4]);
			if(*p)
			{
				dmi_printk(("Board Vendor: %s.\n",p));
				dmi_save_ident(dm, DMI_BOARD_VENDOR, 4);
				dmi_printk(("Board Name: %s.\n",
					dmi_string(dm, data[5])));
				dmi_save_ident(dm, DMI_BOARD_NAME, 5);
				dmi_printk(("Board Version: %s.\n",
					dmi_string(dm, data[6])));
				dmi_save_ident(dm, DMI_BOARD_VERSION, 6);
			}
			break;
		case 3:
			p=dmi_string(dm,data[8]);
			if(*p && *p!=' ')
				dmi_printk(("Asset Tag: %s.\n", p));
			break;
	}
}

void __init dmi_scan_mach(void)
{
	int err;
	printk("dmi_scan.o version %s (%s)\n", LM_VERSION, LM_DATE);
	err = dmi_iterate(dmi_decode);
	if(err)
		printk("dmi_scan.o: SM BIOS not found\n");
	else
		printk("dmi_scan.o: SM BIOS found\n");
}

#ifdef MODULE
MODULE_DESCRIPTION("SM BIOS DMI Scanner");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
EXPORT_SYMBOL(dmi_ident);
EXPORT_SYMBOL(dmi_scan_mach);
int init_module(void)
{
	return 0;
}

int cleanup_module(void)
{
	return 0;
}

#endif
