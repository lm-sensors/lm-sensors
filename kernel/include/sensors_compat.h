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

#include <linux/config.h>

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


/* The part below, taken from linux/init.h, is required for compatibility with
   kernels 2.4.16 and older, which don't know about __devexit_p. */

/* Functions marked as __devexit may be discarded at kernel link time, depending
   on config options.  Newer versions of binutils detect references from
   retained sections to discarded sections and flag an error.  Pointers to
   __devexit functions must use __devexit_p(function_name), the wrapper will
   insert either the function_name or NULL, depending on the config options.
 */
#ifndef __devexit_p
#if defined(MODULE) || defined(CONFIG_HOTPLUG)
#define __devexit_p(x) x
#else
#define __devexit_p(x) NULL
#endif
#endif /* __devexit_p */

#endif /* _SENSORS_COMPAT_H_ */
