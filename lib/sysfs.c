/*
    sysfs.c - Part of libsensors, a library for reading Linux sensor data
    Copyright (c) 2005 Mark M. Hoffman <mhoffman@lightlink.com>

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

/* this define needed for strndup() */
#define _GNU_SOURCE

#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sysfs/libsysfs.h>
#include <regex.h>
#include "data.h"
#include "error.h"
#include "access.h"
#include "general.h"
#include "sysfs.h"

int sensors_found_sysfs = 0;

char sensors_sysfs_mount[NAME_MAX];

#define DYNAMIC_CHIP_REGEX "\\([[:alpha:]]\\{1,\\}[[:digit:]]\\{0,\\}\\)\\(\\_[[:alpha:]]\\{1,\\}\\)\\{0,1\\}"

static int sensors_read_dynamic_chip_check_mapping(
	sensors_chip_feature *minor,
	sensors_chip_feature *major,
	regmatch_t *pmatch)
{
	if (pmatch[1].rm_so == -1 || pmatch[2].rm_so == -1 || 
			strncmp(minor->data.name, major->data.name, 
			pmatch[1].rm_eo - pmatch[1].rm_so)) {
		return 0;
	} else {
		minor->data.mapping = 
			minor->data.compute_mapping = 
			major->data.number;
			
		return 1;
	}
}

static 
sensors_chip_features sensors_read_dynamic_chip(struct sysfs_device *sysdir)
{
	int fnum = 1, i, last_major = -1;
	struct sysfs_attribute *attr;
	struct dlist *attrs;
	sensors_chip_features ret = {0, 0};
	sensors_chip_feature features[256], *dyn_features;
	regex_t preg;
	regmatch_t pmatch[3];
	char *name;
		
	attrs = sysfs_get_device_attributes(sysdir);
	
	if (attrs == NULL)
		return ret;
	
	regcomp(&preg, DYNAMIC_CHIP_REGEX, 0);
		
	dlist_for_each_data(attrs, attr, struct sysfs_attribute) {
		sensors_chip_feature feature = { { 0, }, 0, };	
		name = attr->name;
		
		if (!strcmp(name, "name")) {
			ret.prefix = strndup(attr->value, strlen(attr->value) - 1);
			continue;
		} else if (regexec(&preg, name, 3, pmatch, 0) != 0) {
			continue;
		}
		
		feature.data.number = fnum;
		feature.data.mode = (attr->method & (SYSFS_METHOD_SHOW|SYSFS_METHOD_STORE)) == 
											(SYSFS_METHOD_SHOW|SYSFS_METHOD_STORE) ?
											SENSORS_MODE_RW :
							(attr->method & SYSFS_METHOD_SHOW) ? SENSORS_MODE_R :
							(attr->method & SYSFS_METHOD_STORE) ? SENSORS_MODE_W :
							SENSORS_MODE_NO_RW;
		feature.data.mapping = SENSORS_NO_MAPPING;
		feature.data.compute_mapping = SENSORS_NO_MAPPING;
			
		if (pmatch[2].rm_so != -1) {
			if (!strcmp(name + pmatch[2].rm_so, "_input")) {
				int last_match;
				
				/* copy only the part before the _ */
				feature.data.name = strndup(name, 
					pmatch[1].rm_eo - pmatch[1].rm_so);
				
				/* check if the features previously read are sub devices of this major
				   and update their mapping accordingly */ 
				last_match = fnum - 1;
				for(i = fnum - 2; i >= 0; i--) {
					if (regexec(&preg, features[i].data.name, 3, pmatch, 0) == -1 ||
							!sensors_read_dynamic_chip_check_mapping(&features[i],
							&feature, pmatch)) {
						break;						
					} else {
						last_match = i;
					}
				}
				
				/* if so the major should be in the list before any minor feature,
					so swap with the oldest read */
				if (last_match < fnum - 1) {
					sensors_chip_feature tmp = features[last_match];
					features[last_match] = feature;
					features[fnum - 1] = tmp;
							
					last_major = last_match;
					
					goto NEXT_NUM; /* NOTE: goto used */
				} else {
					last_major = fnum - 1;
				}
				
			} else {
				feature.data.name = strdup(name);
			
				if (last_major != -1 && 
						!sensors_read_dynamic_chip_check_mapping(&feature,
								&features[last_major], pmatch)) {
					last_major = -1; /* no more features for current major */
				}
			}
		} else {
			feature.data.name = strdup(name);
		}
			
		features[fnum - 1] = feature;		
NEXT_NUM:
		fnum++;
	}
		
	regfree(&preg);
	
	dyn_features = malloc(sizeof(sensors_chip_feature) * fnum);
	if (dyn_features == NULL) {
		sensors_fatal_error(__FUNCTION__,"Out of memory");
	}
	
	for(i = 0; i < fnum - 1; i++) {
		dyn_features[i] = features[i];
	}
	dyn_features[fnum - 1].data.name = 0;
	
	ret.feature = dyn_features;
	
	return ret;
}

/* returns !0 if sysfs filesystem was found, 0 otherwise */
int sensors_init_sysfs(void)
{
	if (sysfs_get_mnt_path(sensors_sysfs_mount, NAME_MAX) == 0)
		sensors_found_sysfs = 1;

	return sensors_found_sysfs;
}

/* returns: 0 if successful, !0 otherwise */
static int sensors_read_one_sysfs_chip(struct sysfs_device *dev)
{
	static int total_dynamic = 0;
	int domain, bus, slot, fn, i;
	struct sysfs_attribute *attr, *bus_attr;
	char bus_path[SYSFS_PATH_MAX];
	sensors_proc_chips_entry entry;

	/* ignore any device without name attribute */
	if (!(attr = sysfs_get_device_attr(dev, "name")))
		return 0;

	/* ignore subclients */
	if (attr->len >= 11 && !strcmp(attr->value + attr->len - 11,
			" subclient\n"))
		return 0;

	/* also ignore eeproms */
	if (!strcmp(attr->value, "eeprom\n"))
		return 0;

	/* NB: attr->value[attr->len-1] == '\n'; chop that off */
	entry.name.prefix = strndup(attr->value, attr->len - 1);
	if (!entry.name.prefix)
		sensors_fatal_error(__FUNCTION__, "out of memory");

	entry.name.busname = strdup(dev->path);
	if (!entry.name.busname)
		sensors_fatal_error(__FUNCTION__, "out of memory");

	if (sscanf(dev->name, "%d-%x", &entry.name.bus, &entry.name.addr) == 2) {
		/* find out if legacy ISA or not */
		if (entry.name.bus == 9191)
			entry.name.bus = SENSORS_CHIP_NAME_BUS_ISA;
		else {
			snprintf(bus_path, sizeof(bus_path),
				"%s/class/i2c-adapter/i2c-%d/device/name",
				sensors_sysfs_mount, entry.name.bus);

			if ((bus_attr = sysfs_open_attribute(bus_path))) {
				if (sysfs_read_attribute(bus_attr))
					return -SENSORS_ERR_PARSE;

				if (bus_attr->value
				 && !strncmp(bus_attr->value, "ISA ", 4))
					entry.name.bus = SENSORS_CHIP_NAME_BUS_ISA;

				sysfs_close_attribute(bus_attr);
			}
		}
	} else if (sscanf(dev->name, "%*[a-z0-9_].%d", &entry.name.addr) == 1) {
		/* must be new ISA (platform driver) */
		entry.name.bus = SENSORS_CHIP_NAME_BUS_ISA;
	} else if (sscanf(dev->name, "%x:%x:%x.%x", &domain, &bus, &slot, &fn) == 4) {
		/* PCI */
		entry.name.addr = (domain << 16) + (bus << 8) + (slot << 3) + fn;
		entry.name.bus = SENSORS_CHIP_NAME_BUS_PCI;
	} else
		return -SENSORS_ERR_PARSE;
	
	/* check whether this chip is known in the static list */ 
	for (i = 0; sensors_chip_features_list[i].prefix; i++)
		if (!strcasecmp(sensors_chip_features_list[i].prefix, entry.name.prefix))
			break;

	/* if no chip definition matches */
	if (!sensors_chip_features_list[i].prefix && 
		total_dynamic < N_PLACEHOLDER_ELEMENTS) {
		sensors_chip_features n_entry = sensors_read_dynamic_chip(dev);

		/* skip to end of list */
		for(i = 0; sensors_chip_features_list[i].prefix; i++);

		sensors_chip_features_list[i] = n_entry;	

		total_dynamic++;
	}
		
	sensors_add_proc_chips(&entry);

	return 0;
}

/* returns 0 if successful, !0 otherwise */
static int sensors_read_sysfs_chips_compat(void)
{
	struct sysfs_bus *bus;
	struct dlist *devs;
	struct sysfs_device *dev;
	int ret = 0;

	if (!(bus = sysfs_open_bus("i2c"))) {
		if (errno && errno != ENOENT)
			ret = -SENSORS_ERR_PROC;
		goto exit0;
	}

	if (!(devs = sysfs_get_bus_devices(bus))) {
		if (errno && errno != ENOENT)
			ret = -SENSORS_ERR_PROC;
		goto exit1;
	}

	dlist_for_each_data(devs, dev, struct sysfs_device)
		if ((ret = sensors_read_one_sysfs_chip(dev)))
			goto exit1;

exit1:
	/* this frees bus and devs */
	sysfs_close_bus(bus);

exit0:
	return ret;
}

/* returns 0 if successful, !0 otherwise */
int sensors_read_sysfs_chips(void)
{
	struct sysfs_class *cls;
	struct dlist *clsdevs;
	struct sysfs_class_device *clsdev;
	int ret = 0;

	if (!(cls = sysfs_open_class("hwmon"))) {
		/* compatibility function for kernel 2.6.n where n <= 13 */
		return sensors_read_sysfs_chips_compat();
	}

	if (!(clsdevs = sysfs_get_class_devices(cls))) {
		if (errno && errno != ENOENT)
			ret = -SENSORS_ERR_PROC;
		goto exit;
	}

	dlist_for_each_data(clsdevs, clsdev, struct sysfs_class_device) {
		struct sysfs_device *dev;
		if (!(dev = sysfs_get_classdev_device(clsdev))) {
			ret = -SENSORS_ERR_PROC;
			goto exit;
		}
		if ((ret = sensors_read_one_sysfs_chip(dev)))
			goto exit;
	}

exit:
	/* this frees cls and clsdevs */
	sysfs_close_class(cls);
	return ret;
}

/* returns 0 if successful, !0 otherwise */
int sensors_read_sysfs_bus(void)
{
	struct sysfs_class *cls;
	struct dlist *clsdevs;
	struct sysfs_class_device *clsdev;
	sensors_bus entry;
	int ret = 0;

	if (!(cls = sysfs_open_class("i2c-adapter"))) {
		if (errno && errno != ENOENT)
			ret = -SENSORS_ERR_PROC;
		goto exit0;
	}

	if (!(clsdevs = sysfs_get_class_devices(cls))) {
		if (errno && errno != ENOENT)
			ret = -SENSORS_ERR_PROC;
		goto exit1;
	}

	dlist_for_each_data(clsdevs, clsdev, struct sysfs_class_device) {
		struct sysfs_device *dev;
		struct sysfs_attribute *attr;

		/* Get the adapter name from the classdev "name" attribute
		 * (Linux 2.6.20 and later). If it fails, fall back to
		 * the device "name" attribute (for older kernels). */
		if (!(attr = sysfs_get_classdev_attr(clsdev, "name"))
		 && !((dev = sysfs_get_classdev_device(clsdev)) &&
		      (attr = sysfs_get_device_attr(dev, "name"))))
			continue;

		/* NB: attr->value[attr->len-1] == '\n'; chop that off */
		entry.adapter = strndup(attr->value, attr->len - 1);
		if (!entry.adapter)
			sensors_fatal_error(__FUNCTION__, "out of memory");

		if (!strncmp(entry.adapter, "ISA ", 4)) {
			entry.number = SENSORS_CHIP_NAME_BUS_ISA;
		} else if (sscanf(clsdev->name, "i2c-%d", &entry.number) != 1) {
			entry.number = SENSORS_CHIP_NAME_BUS_DUMMY;
		}

		sensors_add_proc_bus(&entry);
	}

exit1:
	/* this frees *cls _and_ *clsdevs */
	sysfs_close_class(cls);

exit0:
	return ret;
}

