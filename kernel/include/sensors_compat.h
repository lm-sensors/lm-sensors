/*
 * Stolen from kernel 2.5.69
 * device.h - generic, centralized driver model
 * To make it easier to backport from 2.5
 *
 * Copyright (c) 2001-2003 Patrick Mochel <mochel@osdl.org>
 *
 */

#ifndef _SENSORS_COMPAT_H_
#define _SENSORS_COMPAT_H_

/* debugging and troubleshooting/diagnostic helpers. */
#define dev_printk(level, dev, format, arg...)	\
	printk(level "%s: " format , (dev)->name , ## arg)

#ifdef DEBUG
#define dev_dbg(dev, format, arg...)		\
	dev_printk(KERN_DEBUG , dev , format , ## arg)
#else
#define dev_dbg(dev, format, arg...) do {} while (0)
#endif

#define dev_err(dev, format, arg...)		\
	dev_printk(KERN_ERR , dev , format , ## arg)
#define dev_info(dev, format, arg...)		\
	dev_printk(KERN_INFO , dev , format , ## arg)
#define dev_warn(dev, format, arg...)		\
	dev_printk(KERN_WARNING , dev , format , ## arg)

#endif /* _SENSORS_COMPAT_H_ */
