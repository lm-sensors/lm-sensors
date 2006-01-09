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
#include <sysfs/libsysfs.h>
#include "data.h"
#include "error.h"
#include "access.h"
#include "general.h"
#include "sysfs.h"

int sensors_found_sysfs = 0;

char sensors_sysfs_mount[NAME_MAX];

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
		snprintf(bus_path, sizeof(bus_path),
				"%s/class/i2c-adapter/i2c-%d/device/name",
				sensors_sysfs_mount, entry.name.bus);

		if ((bus_attr = sysfs_open_attribute(bus_path))) {
			if (sysfs_read_attribute(bus_attr))
				return -SENSORS_ERR_PARSE;

			if (bus_attr->value && !strncmp(bus_attr->value, "ISA ", 4))
				entry.name.bus = SENSORS_CHIP_NAME_BUS_ISA;

			sysfs_close_attribute(bus_attr);
		}
	} else if (sscanf(dev->name, "%*[a-z0-9_].%d", &entry.name.addr) == 1) {
		/* must be new ISA (platform driver) */
		entry.name.bus = SENSORS_CHIP_NAME_BUS_ISA;
	} else
		return -SENSORS_ERR_PARSE;

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
		ret = -SENSORS_ERR_PROC;
		goto exit0;
	}

	if (!(devs = sysfs_get_bus_devices(bus))) {
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
		ret = -SENSORS_ERR_PROC;
		goto exit0;
	}

	if (!(clsdevs = sysfs_get_class_devices(cls))) {
		ret = -SENSORS_ERR_PROC;
		goto exit1;
	}

	dlist_for_each_data(clsdevs, clsdev, struct sysfs_class_device) {
		struct sysfs_device *dev;
		struct sysfs_attribute *attr;

		if (!(dev = sysfs_get_classdev_device(clsdev)))
			continue;
		if (!(attr = sysfs_get_device_attr(dev, "name")))
			continue;

		entry.adapter = strdup(attr->value);
		if (!entry.adapter)
			sensors_fatal_error(__FUNCTION__, "out of memory");

		if (!strncmp(entry.adapter, "ISA ", 4)) {
			entry.number = SENSORS_CHIP_NAME_BUS_ISA;
			entry.algorithm = strdup("ISA bus algorithm");
		} else if (sscanf(clsdev->name, "i2c-%d", &entry.number) != 1) {
			entry.number = SENSORS_CHIP_NAME_BUS_DUMMY;
			entry.algorithm = strdup("Dummy bus algorithm");
		} else
			entry.algorithm = strdup("Unavailable from sysfs");

		if (!entry.algorithm)
			sensors_fatal_error(__FUNCTION__, "out of memory");

		sensors_add_proc_bus(&entry);
	}

exit1:
	/* this frees *cls _and_ *clsdevs */
	sysfs_close_class(cls);

exit0:
	return ret;
}

