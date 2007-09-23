/*
    sysfs.c - Part of libsensors, a library for reading Linux sensor data
    Copyright (c) 2005 Mark M. Hoffman <mhoffman@lightlink.com>
    Copyright (C) 2007 Jean Delvare <khali@linux-fr.org>

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

#define MAX_SENSORS_PER_TYPE	20
#define MAX_SUBFEATURES		8
/* Room for all 3 types (in, fan, temp) with all their subfeatures + VID
   + misc features */
#define ALL_POSSIBLE_SUBFEATURES \
				(MAX_SENSORS_PER_TYPE * MAX_SUBFEATURES * 6 \
				 + MAX_SENSORS_PER_TYPE + 1)

static
int get_type_scaling(sensors_subfeature_type type)
{
	switch (type & 0xFF10) {
	case SENSORS_SUBFEATURE_IN_INPUT:
	case SENSORS_SUBFEATURE_TEMP_INPUT:
		return 1000;
	case SENSORS_SUBFEATURE_FAN_INPUT:
		return 1;
	}

	switch (type) {
	case SENSORS_SUBFEATURE_VID:
	case SENSORS_SUBFEATURE_TEMP_OFFSET:
		return 1000;
	default:
		return 1;
	}
}

static
char *get_feature_name(sensors_feature_type ftype, char *sfname)
{
	char *name, *underscore;

	switch (ftype) {
	case SENSORS_FEATURE_IN:
	case SENSORS_FEATURE_FAN:
	case SENSORS_FEATURE_TEMP:
		underscore = strchr(sfname, '_');
		name = strndup(sfname, underscore - sfname);
		break;
	default:
		name = strdup(sfname);
	}

	return name;
}

/* Static mappings for use by sensors_subfeature_get_type() */
struct subfeature_type_match
{
	const char *name;
	sensors_subfeature_type type;
};

struct feature_type_match
{
	const char *name;
	const struct subfeature_type_match *submatches;
};

static const struct subfeature_type_match temp_matches[] = {
	{ "input", SENSORS_SUBFEATURE_TEMP_INPUT },
	{ "max", SENSORS_SUBFEATURE_TEMP_MAX },
	{ "max_hyst", SENSORS_SUBFEATURE_TEMP_MAX_HYST },
	{ "min", SENSORS_SUBFEATURE_TEMP_MIN },
	{ "crit", SENSORS_SUBFEATURE_TEMP_CRIT },
	{ "crit_hyst", SENSORS_SUBFEATURE_TEMP_CRIT_HYST },
	{ "alarm", SENSORS_SUBFEATURE_TEMP_ALARM },
	{ "min_alarm", SENSORS_SUBFEATURE_TEMP_MIN_ALARM },
	{ "max_alarm", SENSORS_SUBFEATURE_TEMP_MAX_ALARM },
	{ "crit_alarm", SENSORS_SUBFEATURE_TEMP_CRIT_ALARM },
	{ "fault", SENSORS_SUBFEATURE_TEMP_FAULT },
	{ "type", SENSORS_SUBFEATURE_TEMP_TYPE },
	{ "offset", SENSORS_SUBFEATURE_TEMP_OFFSET },
	{ NULL, 0 }
};

static const struct subfeature_type_match in_matches[] = {
	{ "input", SENSORS_SUBFEATURE_IN_INPUT },
	{ "min", SENSORS_SUBFEATURE_IN_MIN },
	{ "max", SENSORS_SUBFEATURE_IN_MAX },
	{ "alarm", SENSORS_SUBFEATURE_IN_ALARM },
	{ "min_alarm", SENSORS_SUBFEATURE_IN_MIN_ALARM },
	{ "max_alarm", SENSORS_SUBFEATURE_IN_MAX_ALARM },
	{ NULL, 0 }
};

static const struct subfeature_type_match fan_matches[] = {
	{ "input", SENSORS_SUBFEATURE_FAN_INPUT },
	{ "min", SENSORS_SUBFEATURE_FAN_MIN },
	{ "div", SENSORS_SUBFEATURE_FAN_DIV },
	{ "alarm", SENSORS_SUBFEATURE_FAN_ALARM },
	{ "fault", SENSORS_SUBFEATURE_FAN_FAULT },
	{ NULL, 0 }
};

static const struct subfeature_type_match cpu_matches[] = {
	{ "vid", SENSORS_SUBFEATURE_VID },
	{ NULL, 0 }
};

static struct feature_type_match matches[] = {
	{ "temp%d%c", temp_matches },
	{ "in%d%c", in_matches },
	{ "fan%d%c", fan_matches },
	{ "cpu%d%c", cpu_matches },
};

/* Return the subfeature type and channel number based on the subfeature
   name */
static
sensors_subfeature_type sensors_subfeature_get_type(const char *name, int *nr)
{
	char c;
	int i, count;
	const struct subfeature_type_match *submatches;

	/* Special case */
	if (!strcmp(name, "beep_enable")) {
		*nr = 0;
		return SENSORS_SUBFEATURE_BEEP_ENABLE;
	}

	for (i = 0; i < ARRAY_SIZE(matches); i++)
		if ((count = sscanf(name, matches[i].name, nr, &c)))
			break;

	if (i == ARRAY_SIZE(matches) || count != 2 || c != '_')
		return SENSORS_SUBFEATURE_UNKNOWN;  /* no match */

	submatches = matches[i].submatches;
	name = strchr(name + 3, '_') + 1;
	for (i = 0; submatches[i].name != NULL; i++)
		if (!strcmp(name, submatches[i].name))
			return submatches[i].type;

	return SENSORS_SUBFEATURE_UNKNOWN;
}

static int sensors_read_dynamic_chip(sensors_chip_features *chip,
				     struct sysfs_device *sysdir)
{
	int i, fnum = 0, sfnum = 0, prev_slot;
	struct sysfs_attribute *attr;
	struct dlist *attrs;
	sensors_subfeature *all_subfeatures;
	sensors_subfeature *dyn_subfeatures;
	sensors_feature *dyn_features;
	sensors_feature_type ftype;
	sensors_subfeature_type sftype;

	attrs = sysfs_get_device_attributes(sysdir);

	if (attrs == NULL)
		return -ENOENT;

	/* We use a large sparse table at first to store all found
	   subfeatures, so that we can store them sorted at type and index
	   and then later create a dense sorted table. */
	all_subfeatures = calloc(ALL_POSSIBLE_SUBFEATURES,
				 sizeof(sensors_subfeature));
	if (!all_subfeatures)
		sensors_fatal_error(__FUNCTION__, "Out of memory");

	dlist_for_each_data(attrs, attr, struct sysfs_attribute) {
		char *name = attr->name;
		int nr;

		sftype = sensors_subfeature_get_type(name, &nr);
		if (sftype == SENSORS_SUBFEATURE_UNKNOWN)
			continue;

		/* Adjust the channel number */
		switch (sftype & 0xFF00) {
			case SENSORS_SUBFEATURE_FAN_INPUT:
			case SENSORS_SUBFEATURE_TEMP_INPUT:
				if (nr)
					nr--;
				break;
		}

		if (nr >= MAX_SENSORS_PER_TYPE) {
			fprintf(stderr, "libsensors error, more sensors of one"
				" type then MAX_SENSORS_PER_TYPE, ignoring "
				"subfeature: %s\n", name);
			continue;
		}

		/* "calculate" a place to store the subfeature in our sparse,
		   sorted table */
		switch (sftype) {
		case SENSORS_SUBFEATURE_VID:
			i = nr + MAX_SENSORS_PER_TYPE * MAX_SUBFEATURES * 6;
			break;
		case SENSORS_SUBFEATURE_BEEP_ENABLE:
			i = MAX_SENSORS_PER_TYPE * MAX_SUBFEATURES * 6 +
			    MAX_SENSORS_PER_TYPE;
			break;
		default:
			i = (sftype >> 8) * MAX_SENSORS_PER_TYPE *
			    MAX_SUBFEATURES * 2 + nr * MAX_SUBFEATURES * 2 +
			    ((sftype & 0x10) >> 4) * MAX_SUBFEATURES +
			    (sftype & 0x0F);
		}

		if (all_subfeatures[i].name) {
			fprintf(stderr, "libsensors error, trying to add dupli"
				"cate subfeature: %s to dynamic feature table\n",
				name);
			continue;
		}

		/* fill in the subfeature members */
		all_subfeatures[i].type = sftype;
		all_subfeatures[i].name = strdup(name);
		if (!(sftype & 0x10))
			all_subfeatures[i].flags |= SENSORS_COMPUTE_MAPPING;
		if (attr->method & SYSFS_METHOD_SHOW)
			all_subfeatures[i].flags |= SENSORS_MODE_R;
		if (attr->method & SYSFS_METHOD_STORE)
			all_subfeatures[i].flags |= SENSORS_MODE_W;

		sfnum++;
	}

	if (!sfnum) { /* No subfeature */
		chip->subfeature = NULL;
		goto exit_free;
	}

	/* How many main features? */
	prev_slot = -1;
	for (i = 0; i < ALL_POSSIBLE_SUBFEATURES; i++) {
		if (!all_subfeatures[i].name)
			continue;

		if (i >= MAX_SENSORS_PER_TYPE * MAX_SUBFEATURES * 6 ||
		    i / (MAX_SUBFEATURES * 2) != prev_slot) {
			fnum++;
			prev_slot = i / (MAX_SUBFEATURES * 2);
		}
	}

	dyn_subfeatures = calloc(sfnum, sizeof(sensors_subfeature));
	dyn_features = calloc(fnum, sizeof(sensors_feature));
	if (!dyn_subfeatures || !dyn_features)
		sensors_fatal_error(__FUNCTION__, "Out of memory");

	/* Copy from the sparse array to the compact array */
	sfnum = 0;
	fnum = -1;
	prev_slot = -1;
	for (i = 0; i < ALL_POSSIBLE_SUBFEATURES; i++) {
		if (!all_subfeatures[i].name)
			continue;

		/* New main feature? */
		if (i >= MAX_SENSORS_PER_TYPE * MAX_SUBFEATURES * 6 ||
		    i / (MAX_SUBFEATURES * 2) != prev_slot) {
			ftype = all_subfeatures[i].type >> 8;
			fnum++;
			prev_slot = i / (MAX_SUBFEATURES * 2);

			dyn_features[fnum].name = get_feature_name(ftype,
						all_subfeatures[i].name);
			dyn_features[fnum].number = fnum;
			dyn_features[fnum].first_subfeature = sfnum;
			dyn_features[fnum].type = ftype;
		}

		dyn_subfeatures[sfnum] = all_subfeatures[i];
		dyn_subfeatures[sfnum].number = sfnum;
		/* Back to the feature */
		dyn_subfeatures[sfnum].mapping = fnum;

		sfnum++;
	}

	chip->subfeature = dyn_subfeatures;
	chip->subfeature_count = sfnum;
	chip->feature = dyn_features;
	chip->feature_count = ++fnum;

exit_free:
	free(all_subfeatures);
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
	int err = -SENSORS_ERR_KERNEL;
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

	if (sscanf(dev->name, "%hd-%x", &entry.chip.bus.nr, &entry.chip.addr) == 2) {
		/* find out if legacy ISA or not */
		if (entry.chip.bus.nr == 9191) {
			entry.chip.bus.type = SENSORS_BUS_TYPE_ISA;
			entry.chip.bus.nr = 0;
		} else {
			entry.chip.bus.type = SENSORS_BUS_TYPE_I2C;
			snprintf(bus_path, sizeof(bus_path),
				"%s/class/i2c-adapter/i2c-%d/device/name",
				sensors_sysfs_mount, entry.chip.bus.nr);

			if ((bus_attr = sysfs_open_attribute(bus_path))) {
				if (sysfs_read_attribute(bus_attr)) {
					sysfs_close_attribute(bus_attr);
					goto exit_free;
				}

				if (bus_attr->value
				 && !strncmp(bus_attr->value, "ISA ", 4)) {
					entry.chip.bus.type = SENSORS_BUS_TYPE_ISA;
					entry.chip.bus.nr = 0;
				}

				sysfs_close_attribute(bus_attr);
			}
		}
	} else if (sscanf(dev->name, "spi%hd.%d", &entry.chip.bus.nr,
			  &entry.chip.addr) == 2) {
		/* SPI */
		entry.chip.bus.type = SENSORS_BUS_TYPE_SPI;
	} else if (sscanf(dev->name, "%*[a-z0-9_].%d", &entry.chip.addr) == 1) {
		/* must be new ISA (platform driver) */
		entry.chip.bus.type = SENSORS_BUS_TYPE_ISA;
		entry.chip.bus.nr = 0;
	} else if (sscanf(dev->name, "%x:%x:%x.%x", &domain, &bus, &slot, &fn) == 4) {
		/* PCI */
		entry.chip.addr = (domain << 16) + (bus << 8) + (slot << 3) + fn;
		entry.chip.bus.type = SENSORS_BUS_TYPE_PCI;
		entry.chip.bus.nr = 0;
	} else
		goto exit_free;

	if (sensors_read_dynamic_chip(&entry, dev) < 0)
		goto exit_free;
	if (!entry.subfeature) { /* No subfeature, discard chip */
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
			ret = -SENSORS_ERR_NO_DEVS;
		goto exit0;
	}

	if (!(devs = sysfs_get_bus_devices(bus))) {
		if (errno && errno != ENOENT)
			ret = -SENSORS_ERR_NO_DEVS;
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
			ret = -SENSORS_ERR_NO_DEVS;
		goto exit;
	}

	dlist_for_each_data(clsdevs, clsdev, struct sysfs_class_device) {
		struct sysfs_device *dev;
		if (!(dev = sysfs_get_classdev_device(clsdev))) {
			ret = -SENSORS_ERR_NO_DEVS;
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
			ret = -SENSORS_ERR_KERNEL;
		goto exit0;
	}

	if (!(clsdevs = sysfs_get_class_devices(cls))) {
		if (errno && errno != ENOENT)
			ret = -SENSORS_ERR_KERNEL;
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

		if (sscanf(clsdev->name, "i2c-%hd", &entry.bus.nr) != 1 ||
		    entry.bus.nr == 9191) /* legacy ISA */
			continue;
		entry.bus.type = SENSORS_BUS_TYPE_I2C;

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

int sensors_read_sysfs_attr(const sensors_chip_name *name,
			    const sensors_subfeature *subfeature,
			    double *value)
{
	char n[NAME_MAX];
	FILE *f;

	snprintf(n, NAME_MAX, "%s/%s", name->path, subfeature->name);
	if ((f = fopen(n, "r"))) {
		int res = fscanf(f, "%lf", value);
		fclose(f);
		if (res != 1)
			return -SENSORS_ERR_KERNEL;
		*value /= get_type_scaling(subfeature->type);
	} else
		return -SENSORS_ERR_KERNEL;

	return 0;
}

int sensors_write_sysfs_attr(const sensors_chip_name *name,
			     const sensors_subfeature *subfeature,
			     double value)
{
	char n[NAME_MAX];
	FILE *f;

	snprintf(n, NAME_MAX, "%s/%s", name->path, subfeature->name);
	if ((f = fopen(n, "w"))) {
		value *= get_type_scaling(subfeature->type);
		fprintf(f, "%d", (int) value);
		fclose(f);
	} else
		return -SENSORS_ERR_KERNEL;

	return 0;
}
