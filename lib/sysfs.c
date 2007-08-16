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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sysfs/libsysfs.h>
#include "data.h"
#include "error.h"
#include "access.h"
#include "general.h"
#include "sysfs.h"

char sensors_sysfs_mount[NAME_MAX];

#define MAX_SENSORS_PER_TYPE 16

static
int get_type_scaling(int type)
{
	switch (type & 0xFF10) {
	case SENSORS_FEATURE_IN:
	case SENSORS_FEATURE_TEMP:
		return 3;
	case SENSORS_FEATURE_FAN:
		return 0;
	}

	switch (type) {
	case SENSORS_FEATURE_VID:
		return 3;
	default:
		return 0;
	}

	return 0;
}

static int sensors_read_dynamic_chip(sensors_chip_features *chip,
				     struct sysfs_device *sysdir)
{
	int i, type, fnum = 1;
	struct sysfs_attribute *attr;
	struct dlist *attrs;
	/* room for all 3  (in, fan, temp) types, with all their subfeatures
	   + VID. We use a large sparse table at first to store all
	   found features, so that we can store them sorted at type and index
	   and then later create a dense sorted table */
	sensors_chip_feature features[MAX_SENSORS_PER_TYPE *
		SENSORS_FEATURE_MAX_SUB_FEATURES * 3 +
		MAX_SENSORS_PER_TYPE];
	sensors_chip_feature *dyn_features;
	char *name;
		
	attrs = sysfs_get_device_attributes(sysdir);
	
	if (attrs == NULL)
		return -ENOENT;
		
	memset(features, 0, sizeof(features));
	
	dlist_for_each_data(attrs, attr, struct sysfs_attribute) {
		sensors_chip_feature feature;
		name = attr->name;
		int nr;
		
		type = sensors_feature_get_type(name, &nr);
		if (type == SENSORS_FEATURE_UNKNOWN)
			continue;

		memset(&feature, 0, sizeof(sensors_chip_feature));
		/* check for _input extension and remove */
		i = strlen(name);
		if (i > 6 && !strcmp(name + i - 6, "_input"))
			feature.data.name = strndup(name, i-6);
		else
			feature.data.name = strdup(name);

		/* Adjust the channel number */
		switch (type & 0xFF00) {
			case SENSORS_FEATURE_FAN:
			case SENSORS_FEATURE_TEMP:
				if (nr)
					nr--;
				break;
		}
		
		if (nr >= MAX_SENSORS_PER_TYPE) {
			fprintf(stderr, "libsensors error, more sensors of one"
				" type then MAX_SENSORS_PER_TYPE, ignoring "
				"feature: %s\n", name);
			free(feature.data.name);
			continue;
		}
		
		/* "calculate" a place to store the feature in our sparse,
		   sorted table */
		if (type == SENSORS_FEATURE_VID) {
			i = nr + MAX_SENSORS_PER_TYPE *
			     SENSORS_FEATURE_MAX_SUB_FEATURES * 3;
		} else {
			i = (type >> 8) * MAX_SENSORS_PER_TYPE *
				SENSORS_FEATURE_MAX_SUB_FEATURES +
				nr * SENSORS_FEATURE_MAX_SUB_FEATURES +
				(type & 0xFF);
		}
		
		if (features[i].data.name) {			
			fprintf(stderr, "libsensors error, trying to add dupli"
				"cate feature: %s to dynamic feature table\n",
				name);
			free(feature.data.name);
			continue;
		}
		
		/* fill in the other feature members */
		feature.data.number = i + 1;
		feature.data.type = type;
			
		if ((type & 0x00FF) == 0) {
			/* main feature */
			feature.data.mapping = SENSORS_NO_MAPPING;
			feature.data.compute_mapping = SENSORS_NO_MAPPING;
		} else if (type & 0x10) {
			/* sub feature without compute mapping */
			feature.data.mapping = i -
				i % SENSORS_FEATURE_MAX_SUB_FEATURES + 1;
			feature.data.compute_mapping = SENSORS_NO_MAPPING;
		} else {
			feature.data.mapping = i -
				i % SENSORS_FEATURE_MAX_SUB_FEATURES + 1;
			feature.data.compute_mapping = feature.data.mapping;
		}
		
		if (attr->method & SYSFS_METHOD_SHOW)
			feature.data.mode |= SENSORS_MODE_R;
		if (attr->method & SYSFS_METHOD_STORE)
			feature.data.mode |= SENSORS_MODE_W;

		feature.scaling = get_type_scaling(type);

		features[i] = feature;
		fnum++;
	}

	if (fnum == 1) { /* No feature */
		chip->feature = NULL;
		return 0;
	}

	dyn_features = calloc(fnum, sizeof(sensors_chip_feature));
	if (dyn_features == NULL) {
		sensors_fatal_error(__FUNCTION__,"Out of memory");
	}
	
	fnum = 0;
	for(i = 0; i < ARRAY_SIZE(features); i++) {
		if (features[i].data.name) {
			dyn_features[fnum] = features[i];
			fnum++;
		}
	}
	
	chip->feature = dyn_features;
	
	return 0;
}

/* returns !0 if sysfs filesystem was found, 0 otherwise */
int sensors_init_sysfs(void)
{
	struct stat statbuf;

	/* libsysfs will return success even if sysfs is not mounted,
	   so we have to double-check */
	if (sysfs_get_mnt_path(sensors_sysfs_mount, NAME_MAX)
	 || stat(sensors_sysfs_mount, &statbuf) < 0
	 || statbuf.st_nlink <= 2)	/* Empty directory */
		return 0;

	return 1;
}

/* returns: 0 if successful, !0 otherwise */
static int sensors_read_one_sysfs_chip(struct sysfs_device *dev)
{
	int domain, bus, slot, fn;
	int err = -SENSORS_ERR_PARSE;
	struct sysfs_attribute *attr, *bus_attr;
	char bus_path[SYSFS_PATH_MAX];
	sensors_chip_features entry;

	/* ignore any device without name attribute */
	if (!(attr = sysfs_get_device_attr(dev, "name")))
		return 0;

	/* NB: attr->value[attr->len-1] == '\n'; chop that off */
	entry.chip.prefix = strndup(attr->value, attr->len - 1);
	if (!entry.chip.prefix)
		sensors_fatal_error(__FUNCTION__, "out of memory");

	entry.chip.path = strdup(dev->path);
	if (!entry.chip.path)
		sensors_fatal_error(__FUNCTION__, "out of memory");

	if (sscanf(dev->name, "%d-%x", &entry.chip.bus, &entry.chip.addr) == 2) {
		/* find out if legacy ISA or not */
		if (entry.chip.bus == 9191)
			entry.chip.bus = SENSORS_CHIP_NAME_BUS_ISA;
		else {
			snprintf(bus_path, sizeof(bus_path),
				"%s/class/i2c-adapter/i2c-%d/device/name",
				sensors_sysfs_mount, entry.chip.bus);

			if ((bus_attr = sysfs_open_attribute(bus_path))) {
				if (sysfs_read_attribute(bus_attr)) {
					sysfs_close_attribute(bus_attr);
					goto exit_free;
				}

				if (bus_attr->value
				 && !strncmp(bus_attr->value, "ISA ", 4))
					entry.chip.bus = SENSORS_CHIP_NAME_BUS_ISA;

				sysfs_close_attribute(bus_attr);
			}
		}
	} else if (sscanf(dev->name, "%*[a-z0-9_].%d", &entry.chip.addr) == 1) {
		/* must be new ISA (platform driver) */
		entry.chip.bus = SENSORS_CHIP_NAME_BUS_ISA;
	} else if (sscanf(dev->name, "%x:%x:%x.%x", &domain, &bus, &slot, &fn) == 4) {
		/* PCI */
		entry.chip.addr = (domain << 16) + (bus << 8) + (slot << 3) + fn;
		entry.chip.bus = SENSORS_CHIP_NAME_BUS_PCI;
	} else
		goto exit_free;
	
	if (sensors_read_dynamic_chip(&entry, dev) < 0)
		goto exit_free;
	if (!entry.feature) { /* No feature, discard chip */
		err = 0;
		goto exit_free;
	}
	sensors_add_proc_chips(&entry);

	return 0;

exit_free:
	free(entry.chip.prefix);
	free(entry.chip.path);
	return err;
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

		if (sscanf(clsdev->name, "i2c-%d", &entry.number) != 1 ||
		    entry.number == 9191) /* legacy ISA */
			continue;

		/* NB: attr->value[attr->len-1] == '\n'; chop that off */
		entry.adapter = strndup(attr->value, attr->len - 1);
		if (!entry.adapter)
			sensors_fatal_error(__FUNCTION__, "out of memory");

		sensors_add_proc_bus(&entry);
	}

exit1:
	/* this frees *cls _and_ *clsdevs */
	sysfs_close_class(cls);

exit0:
	return ret;
}

int sensors_read_sysfs_attr(const sensors_chip_name *name, int feature,
			    double *value)
{
	const sensors_chip_feature *the_feature;
	int mag;
	char n[NAME_MAX];
	FILE *f;
	int dummy;
	char check;
	const char *suffix = "";

	if (!(the_feature = sensors_lookup_feature_nr(name, feature)))
		return -SENSORS_ERR_NO_ENTRY;

	/* REVISIT: this is a ugly hack */
	if (sscanf(the_feature->data.name, "in%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "fan%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "temp%d%c", &dummy, &check) == 1)
		suffix = "_input";

	snprintf(n, NAME_MAX, "%s/%s%s", name->path, the_feature->data.name,
		 suffix);
	if ((f = fopen(n, "r"))) {
		int res = fscanf(f, "%lf", value);
		fclose(f);
		if (res != 1)
			return -SENSORS_ERR_PROC;
		for (mag = the_feature->scaling; mag > 0; mag --)
			*value /= 10.0;
	} else
		return -SENSORS_ERR_PROC;

	return 0;
}
  
int sensors_write_sysfs_attr(const sensors_chip_name *name, int feature,
			     double value)
{
	const sensors_chip_feature *the_feature;
	int mag;
	char n[NAME_MAX];
	FILE *f;
	int dummy;
	char check;
	const char *suffix = "";
 
	if (!(the_feature = sensors_lookup_feature_nr(name, feature)))
		return -SENSORS_ERR_NO_ENTRY;

	/* REVISIT: this is a ugly hack */
	if (sscanf(the_feature->data.name, "in%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "fan%d%c", &dummy, &check) == 1
	 || sscanf(the_feature->data.name, "temp%d%c", &dummy, &check) == 1)
		suffix = "_input";

	snprintf(n, NAME_MAX, "%s/%s%s", name->path, the_feature->data.name,
		 suffix);
	if ((f = fopen(n, "w"))) {
		for (mag = the_feature->scaling; mag > 0; mag --)
			value *= 10.0;
		fprintf(f, "%d", (int) value);
		fclose(f);
	} else
		return -SENSORS_ERR_PROC;

	return 0;
}
