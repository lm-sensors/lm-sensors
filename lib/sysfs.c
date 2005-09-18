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

