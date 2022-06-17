/*
    sysfs.c - Part of libsensors, a library for reading Linux sensor data
    Copyright (c) 2005 Mark M. Hoffman <mhoffman@lightlink.com>
    Copyright (C) 2007-2014 Jean Delvare <jdelvare@suse.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/

/* this define needed for strndup() */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>
#include "data.h"
#include "error.h"
#include "access.h"
#include "general.h"
#include "sysfs.h"


/****************************************************************************/

#define ATTR_MAX	128
#define SYSFS_MAGIC	0x62656572

/*
 * Read an attribute from sysfs
 * Returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
static char *sysfs_read_attr(const char *device, const char *attr)
{
	char path[NAME_MAX];
	char buf[ATTR_MAX], *p;
	FILE *f;

	snprintf(path, NAME_MAX, "%s/%s", device, attr);

	if (!(f = fopen(path, "r")))
		return NULL;
	p = fgets(buf, ATTR_MAX, f);
	fclose(f);
	if (!p)
		return NULL;

	/* Last byte is a '\n'; chop that off */
	p = strndup(buf, strlen(buf) - 1);
	if (!p)
		sensors_fatal_error(__func__, "Out of memory");
	return p;
}

/*
 * Call an arbitrary function for each class device of the given class
 * Returns 0 on success (all calls returned 0), a positive errno for
 * local errors, or a negative error value if any call fails.
 */
static int sysfs_foreach_classdev(const char *class_name,
				   int (*func)(const char *, const char *))
{
	char path[NAME_MAX];
	int path_off, ret;
	DIR *dir;
	struct dirent *ent;

	path_off = snprintf(path, NAME_MAX, "%s/class/%s",
			    sensors_sysfs_mount, class_name);
	if (!(dir = opendir(path)))
		return errno;

	ret = 0;
	while (!ret && (ent = readdir(dir))) {
		if (ent->d_name[0] == '.')	/* skip hidden entries */
			continue;

		snprintf(path + path_off, NAME_MAX - path_off, "/%s",
			 ent->d_name);
		ret = func(path, ent->d_name);
	}

	closedir(dir);
	return ret;
}

/*
 * Call an arbitrary function for each device of the given bus type
 * Returns 0 on success (all calls returned 0), a positive errno for
 * local errors, or a negative error value if any call fails.
 */
static int sysfs_foreach_busdev(const char *bus_type,
				 int (*func)(const char *, const char *))
{
	char path[NAME_MAX];
	int path_off, ret;
	DIR *dir;
	struct dirent *ent;

	path_off = snprintf(path, NAME_MAX, "%s/bus/%s/devices",
			    sensors_sysfs_mount, bus_type);
	if (!(dir = opendir(path)))
		return errno;

	ret = 0;
	while (!ret && (ent = readdir(dir))) {
		if (ent->d_name[0] == '.')	/* skip hidden entries */
			continue;

		snprintf(path + path_off, NAME_MAX - path_off, "/%s",
			 ent->d_name);
		ret = func(path, ent->d_name);
	}

	closedir(dir);
	return ret;
}

/****************************************************************************/

char sensors_sysfs_mount[NAME_MAX];

static
int get_type_scaling(sensors_subfeature_type type)
{
	/* Multipliers for subfeatures */
	switch (type & 0xFF80) {
	case SENSORS_SUBFEATURE_IN_INPUT:
	case SENSORS_SUBFEATURE_TEMP_INPUT:
	case SENSORS_SUBFEATURE_CURR_INPUT:
	case SENSORS_SUBFEATURE_HUMIDITY_INPUT:
		return 1000;
	case SENSORS_SUBFEATURE_FAN_INPUT:
		return 1;
	case SENSORS_SUBFEATURE_POWER_AVERAGE:
	case SENSORS_SUBFEATURE_ENERGY_INPUT:
		return 1000000;
	}

	/* Multipliers for second class subfeatures
	   that need their own multiplier */
	switch (type) {
	case SENSORS_SUBFEATURE_POWER_AVERAGE_INTERVAL:
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
	case SENSORS_FEATURE_POWER:
	case SENSORS_FEATURE_ENERGY:
	case SENSORS_FEATURE_CURR:
	case SENSORS_FEATURE_HUMIDITY:
	case SENSORS_FEATURE_INTRUSION:
		underscore = strchr(sfname, '_');
		name = strndup(sfname, underscore - sfname);
		if (!name)
			sensors_fatal_error(__func__, "Out of memory");

		break;
	default:
		name = strdup(sfname);
		if (!name)
			sensors_fatal_error(__func__, "Out of memory");
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
	{ "min_hyst", SENSORS_SUBFEATURE_TEMP_MIN_HYST },
	{ "crit", SENSORS_SUBFEATURE_TEMP_CRIT },
	{ "crit_hyst", SENSORS_SUBFEATURE_TEMP_CRIT_HYST },
	{ "lcrit", SENSORS_SUBFEATURE_TEMP_LCRIT },
	{ "lcrit_hyst", SENSORS_SUBFEATURE_TEMP_LCRIT_HYST },
	{ "emergency", SENSORS_SUBFEATURE_TEMP_EMERGENCY },
	{ "emergency_hyst", SENSORS_SUBFEATURE_TEMP_EMERGENCY_HYST },
	{ "lowest", SENSORS_SUBFEATURE_TEMP_LOWEST },
	{ "highest", SENSORS_SUBFEATURE_TEMP_HIGHEST },
	{ "alarm", SENSORS_SUBFEATURE_TEMP_ALARM },
	{ "min_alarm", SENSORS_SUBFEATURE_TEMP_MIN_ALARM },
	{ "max_alarm", SENSORS_SUBFEATURE_TEMP_MAX_ALARM },
	{ "crit_alarm", SENSORS_SUBFEATURE_TEMP_CRIT_ALARM },
	{ "emergency_alarm", SENSORS_SUBFEATURE_TEMP_EMERGENCY_ALARM },
	{ "lcrit_alarm", SENSORS_SUBFEATURE_TEMP_LCRIT_ALARM },
	{ "fault", SENSORS_SUBFEATURE_TEMP_FAULT },
	{ "type", SENSORS_SUBFEATURE_TEMP_TYPE },
	{ "offset", SENSORS_SUBFEATURE_TEMP_OFFSET },
	{ "beep", SENSORS_SUBFEATURE_TEMP_BEEP },
	{ NULL, 0 }
};

static const struct subfeature_type_match in_matches[] = {
	{ "input", SENSORS_SUBFEATURE_IN_INPUT },
	{ "min", SENSORS_SUBFEATURE_IN_MIN },
	{ "max", SENSORS_SUBFEATURE_IN_MAX },
	{ "lcrit", SENSORS_SUBFEATURE_IN_LCRIT },
	{ "crit", SENSORS_SUBFEATURE_IN_CRIT },
	{ "average", SENSORS_SUBFEATURE_IN_AVERAGE },
	{ "lowest", SENSORS_SUBFEATURE_IN_LOWEST },
	{ "highest", SENSORS_SUBFEATURE_IN_HIGHEST },
	{ "alarm", SENSORS_SUBFEATURE_IN_ALARM },
	{ "min_alarm", SENSORS_SUBFEATURE_IN_MIN_ALARM },
	{ "max_alarm", SENSORS_SUBFEATURE_IN_MAX_ALARM },
	{ "lcrit_alarm", SENSORS_SUBFEATURE_IN_LCRIT_ALARM },
	{ "crit_alarm", SENSORS_SUBFEATURE_IN_CRIT_ALARM },
	{ "beep", SENSORS_SUBFEATURE_IN_BEEP },
	{ NULL, 0 }
};

static const struct subfeature_type_match fan_matches[] = {
	{ "input", SENSORS_SUBFEATURE_FAN_INPUT },
	{ "min", SENSORS_SUBFEATURE_FAN_MIN },
	{ "max", SENSORS_SUBFEATURE_FAN_MAX },
	{ "div", SENSORS_SUBFEATURE_FAN_DIV },
	{ "pulses", SENSORS_SUBFEATURE_FAN_PULSES },
	{ "alarm", SENSORS_SUBFEATURE_FAN_ALARM },
	{ "min_alarm", SENSORS_SUBFEATURE_FAN_MIN_ALARM },
	{ "max_alarm", SENSORS_SUBFEATURE_FAN_MAX_ALARM },
	{ "fault", SENSORS_SUBFEATURE_FAN_FAULT },
	{ "beep", SENSORS_SUBFEATURE_FAN_BEEP },
	{ NULL, 0 }
};

static const struct subfeature_type_match power_matches[] = {
	{ "average", SENSORS_SUBFEATURE_POWER_AVERAGE },
	{ "average_highest", SENSORS_SUBFEATURE_POWER_AVERAGE_HIGHEST },
	{ "average_lowest", SENSORS_SUBFEATURE_POWER_AVERAGE_LOWEST },
	{ "input", SENSORS_SUBFEATURE_POWER_INPUT },
	{ "input_highest", SENSORS_SUBFEATURE_POWER_INPUT_HIGHEST },
	{ "input_lowest", SENSORS_SUBFEATURE_POWER_INPUT_LOWEST },
	{ "cap", SENSORS_SUBFEATURE_POWER_CAP },
	{ "cap_hyst", SENSORS_SUBFEATURE_POWER_CAP_HYST },
	{ "cap_alarm", SENSORS_SUBFEATURE_POWER_CAP_ALARM },
	{ "alarm", SENSORS_SUBFEATURE_POWER_ALARM },
	{ "max", SENSORS_SUBFEATURE_POWER_MAX },
	{ "min", SENSORS_SUBFEATURE_POWER_MIN },
	{ "min_alarm", SENSORS_SUBFEATURE_POWER_MIN_ALARM },
	{ "max_alarm", SENSORS_SUBFEATURE_POWER_MAX_ALARM },
	{ "crit", SENSORS_SUBFEATURE_POWER_CRIT },
	{ "lcrit", SENSORS_SUBFEATURE_POWER_LCRIT },
	{ "crit_alarm", SENSORS_SUBFEATURE_POWER_CRIT_ALARM },
	{ "lcrit_alarm", SENSORS_SUBFEATURE_POWER_LCRIT_ALARM },
	{ "average_interval", SENSORS_SUBFEATURE_POWER_AVERAGE_INTERVAL },
	{ NULL, 0 }
};

static const struct subfeature_type_match energy_matches[] = {
	{ "input", SENSORS_SUBFEATURE_ENERGY_INPUT },
	{ NULL, 0 }
};

static const struct subfeature_type_match curr_matches[] = {
	{ "input", SENSORS_SUBFEATURE_CURR_INPUT },
	{ "min", SENSORS_SUBFEATURE_CURR_MIN },
	{ "max", SENSORS_SUBFEATURE_CURR_MAX },
	{ "lcrit", SENSORS_SUBFEATURE_CURR_LCRIT },
	{ "crit", SENSORS_SUBFEATURE_CURR_CRIT },
	{ "average", SENSORS_SUBFEATURE_CURR_AVERAGE },
	{ "lowest", SENSORS_SUBFEATURE_CURR_LOWEST },
	{ "highest", SENSORS_SUBFEATURE_CURR_HIGHEST },
	{ "alarm", SENSORS_SUBFEATURE_CURR_ALARM },
	{ "min_alarm", SENSORS_SUBFEATURE_CURR_MIN_ALARM },
	{ "max_alarm", SENSORS_SUBFEATURE_CURR_MAX_ALARM },
	{ "lcrit_alarm", SENSORS_SUBFEATURE_CURR_LCRIT_ALARM },
	{ "crit_alarm", SENSORS_SUBFEATURE_CURR_CRIT_ALARM },
	{ "beep", SENSORS_SUBFEATURE_CURR_BEEP },
	{ NULL, 0 }
};

static const struct subfeature_type_match humidity_matches[] = {
	{ "input", SENSORS_SUBFEATURE_HUMIDITY_INPUT },
	{ NULL, 0 }
};

static const struct subfeature_type_match cpu_matches[] = {
	{ "vid", SENSORS_SUBFEATURE_VID },
	{ NULL, 0 }
};

static const struct subfeature_type_match intrusion_matches[] = {
	{ "alarm", SENSORS_SUBFEATURE_INTRUSION_ALARM },
	{ "beep", SENSORS_SUBFEATURE_INTRUSION_BEEP },
	{ NULL, 0 }
};
static struct feature_type_match matches[] = {
	{ "temp%d%c", temp_matches },
	{ "in%d%c", in_matches },
	{ "fan%d%c", fan_matches },
	{ "cpu%d%c", cpu_matches },
	{ "power%d%c", power_matches },
	{ "curr%d%c", curr_matches },
	{ "energy%d%c", energy_matches },
	{ "intrusion%d%c", intrusion_matches },
	{ "humidity%d%c", humidity_matches },
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

static int sensors_compute_max_sf(void)
{
	int i, j, max, offset;
	const struct subfeature_type_match *submatches;
	sensors_feature_type ftype;

	max = 0;
	for (i = 0; i < ARRAY_SIZE(matches); i++) {
		submatches = matches[i].submatches;
		for (j = 0; submatches[j].name != NULL; j++) {
			ftype = submatches[j].type >> 8;

			if (ftype < SENSORS_FEATURE_VID) {
				offset = submatches[j].type & 0x7F;
				if (offset >= max)
					max = offset + 1;
			} else {
				offset = submatches[j].type & 0xFF;
				if (offset >= max * 2)
					max = ((offset + 1) + 1) / 2;
			}
		}
	}

	return max;
}

static int sensors_get_attr_mode(const char *device, const char *attr)
{
	char path[NAME_MAX];
	struct stat st;
	int mode = 0;

	snprintf(path, NAME_MAX, "%s/%s", device, attr);
	if (!stat(path, &st)) {
		if (st.st_mode & S_IRUSR)
			mode |= SENSORS_MODE_R;
		if (st.st_mode & S_IWUSR)
			mode |= SENSORS_MODE_W;
	}
	return mode;
}

static int sensors_read_dynamic_chip(sensors_chip_features *chip,
				     const char *dev_path)
{
	int i, fnum = 0, sfnum = 0, prev_slot;
	static int max_subfeatures, feature_size;
	DIR *dir;
	struct dirent *ent;
	struct {
		int count;
		sensors_subfeature *sf;
	} all_types[SENSORS_FEATURE_MAX];
	sensors_subfeature *dyn_subfeatures;
	sensors_feature *dyn_features;
	sensors_feature_type ftype;
	sensors_subfeature_type sftype;

	if (!(dir = opendir(dev_path)))
		return -errno;

	/* Dynamically figure out the max number of subfeatures */
	if (!max_subfeatures) {
		max_subfeatures = sensors_compute_max_sf();
		feature_size = max_subfeatures * 2;
	}

	/* We use a set of large sparse tables at first (one per main
	   feature type present) to store all found subfeatures, so that we
	   can store them sorted and then later create a dense sorted table. */
	memset(&all_types, 0, sizeof(all_types));

	while ((ent = readdir(dir))) {
		char *name;
		int nr;

		/* Skip directories and symlinks */
		if (ent->d_type != DT_REG)
			continue;

		name = ent->d_name;

		sftype = sensors_subfeature_get_type(name, &nr);
		if (sftype == SENSORS_SUBFEATURE_UNKNOWN)
			continue;
		ftype = sftype >> 8;

		/* Adjust the channel number */
		switch (ftype) {
		case SENSORS_FEATURE_FAN:
		case SENSORS_FEATURE_TEMP:
		case SENSORS_FEATURE_POWER:
		case SENSORS_FEATURE_ENERGY:
		case SENSORS_FEATURE_CURR:
		case SENSORS_FEATURE_HUMIDITY:
			nr--;
			break;
		default:
			break;
		}

		/* Skip invalid entries. The high limit is arbitrary, we just
		   don't want to allocate an insane amount of memory. */
		if (nr < 0 || nr >= 1024) {
#ifdef DEBUG
			sensors_fatal_error(__func__,
					    "Invalid channel number!");
#endif
			continue;
		}

		/* (Re-)allocate memory if needed */
		if (all_types[ftype].count < nr + 1) {
			int old_count = all_types[ftype].count;
			int step = ftype < SENSORS_FEATURE_VID ? 8 :
				   ftype < SENSORS_FEATURE_BEEP_ENABLE ? 2 : 1;

			while (all_types[ftype].count < nr + 1)
				all_types[ftype].count += step;

			all_types[ftype].sf = realloc(all_types[ftype].sf,
						all_types[ftype].count *
						feature_size *
						sizeof(sensors_subfeature));
			if (!all_types[ftype].sf)
				sensors_fatal_error(__func__, "Out of memory");
			memset(all_types[ftype].sf + old_count * feature_size,
			       0, (all_types[ftype].count - old_count) *
				  feature_size * sizeof(sensors_subfeature));
		}

		/* "calculate" a place to store the subfeature in our sparse,
		   sorted table */
		if (ftype < SENSORS_FEATURE_VID)
			i = nr * feature_size +
			    ((sftype & 0x80) >> 7) * max_subfeatures +
			    (sftype & 0x7F);
		else
			i = nr * feature_size + (sftype & 0xFF);

		if (all_types[ftype].sf[i].name) {
#ifdef DEBUG
			sensors_fatal_error(__func__, "Duplicate subfeature");
#endif
			continue;
		}

		/* fill in the subfeature members */
		all_types[ftype].sf[i].type = sftype;
		all_types[ftype].sf[i].name = strdup(name);
		if (!all_types[ftype].sf[i].name)
			sensors_fatal_error(__func__, "Out of memory");

		/* Other and misc subfeatures are never scaled */
		if (sftype < SENSORS_SUBFEATURE_VID && !(sftype & 0x80))
			all_types[ftype].sf[i].flags |= SENSORS_COMPUTE_MAPPING;
		all_types[ftype].sf[i].flags |=
					sensors_get_attr_mode(dev_path, name);

		sfnum++;
	}
	closedir(dir);

	if (!sfnum) { /* No subfeature */
		chip->subfeature = NULL;
		goto exit_free;
	}

	/* How many main features? */
	for (ftype = 0; ftype < SENSORS_FEATURE_MAX; ftype++) {
		prev_slot = -1;
		for (i = 0; i < all_types[ftype].count * feature_size; i++) {
			if (!all_types[ftype].sf[i].name)
				continue;

			if (i / feature_size != prev_slot) {
				fnum++;
				prev_slot = i / feature_size;
			}
		}
	}

	dyn_subfeatures = calloc(sfnum, sizeof(sensors_subfeature));
	dyn_features = calloc(fnum, sizeof(sensors_feature));
	if (!dyn_subfeatures || !dyn_features)
		sensors_fatal_error(__func__, "Out of memory");

	/* Copy from the sparse array to the compact array */
	sfnum = 0;
	fnum = -1;
	for (ftype = 0; ftype < SENSORS_FEATURE_MAX; ftype++) {
		prev_slot = -1;
		for (i = 0; i < all_types[ftype].count * feature_size; i++) {
			if (!all_types[ftype].sf[i].name)
				continue;

			/* New main feature? */
			if (i / feature_size != prev_slot) {
				fnum++;
				prev_slot = i / feature_size;

				dyn_features[fnum].name =
					get_feature_name(ftype,
						all_types[ftype].sf[i].name);
				dyn_features[fnum].number = fnum;
				dyn_features[fnum].first_subfeature = sfnum;
				dyn_features[fnum].type = ftype;
			}

			dyn_subfeatures[sfnum] = all_types[ftype].sf[i];
			dyn_subfeatures[sfnum].number = sfnum;
			/* Back to the feature */
			dyn_subfeatures[sfnum].mapping = fnum;

			sfnum++;
		}
	}

	chip->subfeature = dyn_subfeatures;
	chip->subfeature_count = sfnum;
	chip->feature = dyn_features;
	chip->feature_count = ++fnum;

exit_free:
	for (ftype = 0; ftype < SENSORS_FEATURE_MAX; ftype++)
		free(all_types[ftype].sf);
	return 0;
}

/* returns !0 if sysfs filesystem was found, 0 otherwise */
int sensors_init_sysfs(void)
{
	struct statfs statfsbuf;

	snprintf(sensors_sysfs_mount, NAME_MAX, "%s", "/sys");
	if (statfs(sensors_sysfs_mount, &statfsbuf) < 0
	 || statfsbuf.f_type != SYSFS_MAGIC)
		return 0;

	return 1;
}

static int classify_device(const char *dev_name,
                           const char *subsys,
                           sensors_chip_features *entry)
{
	int domain, bus, slot, fn, vendor, product, id;
	char bus_path[NAME_MAX];
	char *bus_attr;
	int ret = 1;

	if ((!subsys || !strcmp(subsys, "i2c")) &&
	    sscanf(dev_name, "%hd-%x", &entry->chip.bus.nr,
		   &entry->chip.addr) == 2) {
		/* find out if legacy ISA or not */
		if (entry->chip.bus.nr == 9191) {
			entry->chip.bus.type = SENSORS_BUS_TYPE_ISA;
			entry->chip.bus.nr = 0;
		} else {
			entry->chip.bus.type = SENSORS_BUS_TYPE_I2C;
			snprintf(bus_path, sizeof(bus_path),
				"%s/class/i2c-adapter/i2c-%d/device",
				sensors_sysfs_mount, entry->chip.bus.nr);

			if ((bus_attr = sysfs_read_attr(bus_path, "name"))) {
				if (!strncmp(bus_attr, "ISA ", 4)) {
					entry->chip.bus.type = SENSORS_BUS_TYPE_ISA;
					entry->chip.bus.nr = 0;
				}

				free(bus_attr);
			}
		}
	} else
	if ((!subsys || !strcmp(subsys, "spi")) &&
	    sscanf(dev_name, "spi%hd.%d", &entry->chip.bus.nr,
		   &entry->chip.addr) == 2) {
		/* SPI */
		entry->chip.bus.type = SENSORS_BUS_TYPE_SPI;
	} else
	if ((!subsys || !strcmp(subsys, "pci")) &&
	    sscanf(dev_name, "%x:%x:%x.%x", &domain, &bus, &slot, &fn) == 4) {
		/* PCI */
		entry->chip.addr = (domain << 16) + (bus << 8) + (slot << 3) + fn;
		entry->chip.bus.type = SENSORS_BUS_TYPE_PCI;
		entry->chip.bus.nr = 0;
	} else
	if ((!subsys || !strcmp(subsys, "platform") ||
			!strcmp(subsys, "of_platform"))) {
		/* must be new ISA (platform driver) */
		if (sscanf(dev_name, "%*[a-zA-Z0-9_]%*1[.:]%d", &entry->chip.addr) != 1)
			entry->chip.addr = 0;
		entry->chip.bus.type = SENSORS_BUS_TYPE_ISA;
		entry->chip.bus.nr = 0;
	} else if (subsys && !strcmp(subsys, "acpi")) {
		entry->chip.bus.type = SENSORS_BUS_TYPE_ACPI;
		/* For now we assume that acpi devices are unique */
		entry->chip.bus.nr = 0;
		entry->chip.addr = 0;
	} else
	if (subsys && !strcmp(subsys, "hid") &&
	    sscanf(dev_name, "%x:%x:%x.%x", &bus, &vendor, &product, &id) == 4) {
		entry->chip.bus.type = SENSORS_BUS_TYPE_HID;
		/* As of kernel 2.6.32, the hid device names don't look good */
		entry->chip.bus.nr = bus;
		entry->chip.addr = id;
	} else
	if (subsys && !strcmp(subsys, "mdio_bus")) {
		if (sscanf(dev_name, "%*[^:]:%d", &entry->chip.addr) != 1)
			entry->chip.addr = 0;
		entry->chip.bus.type = SENSORS_BUS_TYPE_MDIO;
		entry->chip.bus.nr = 0;
	} else
	if (subsys && !strcmp(subsys, "scsi") &&
	    sscanf(dev_name, "%d:%d:%d:%x", &domain, &bus, &slot, &fn) == 4) {
		/* adapter(host), channel(bus), id(target), lun */
		entry->chip.addr = (bus << 8) + (slot << 4) + fn;
		entry->chip.bus.type = SENSORS_BUS_TYPE_SCSI;
		entry->chip.bus.nr = domain;
	} else {
		/* Unknown device */
		ret = 0;
	}

	return ret;
}

static int find_bus_type(const char *dev_path,
                         const char *dev_name,
                         sensors_chip_features *entry)
{
	char linkpath[NAME_MAX];
	char subsys_path[NAME_MAX], *subsys;
	int sub_len;
	char *my_dev_path = NULL;
	int ret = 0;

	my_dev_path = strdup(dev_path);
	if (my_dev_path == NULL)
		sensors_fatal_error(__func__, "Out of memory");

	/* Find bus type */
	while (!ret && my_dev_path != NULL) {
		snprintf(linkpath, NAME_MAX, "%s/subsystem", my_dev_path);
		sub_len = readlink(linkpath, subsys_path, NAME_MAX - 1);
		if (sub_len < 0 && errno == ENOENT) {
			/* Fallback to "bus" link for kernels <= 2.6.17 */
			snprintf(linkpath, NAME_MAX, "%s/bus", my_dev_path);
			sub_len = readlink(linkpath, subsys_path, NAME_MAX - 1);
		}
		if (sub_len < 0) {
			/* Older kernels (<= 2.6.11) have neither the subsystem
			   symlink nor the bus symlink */
			if (errno == ENOENT) {
				subsys = NULL;
			} else {
				ret = -SENSORS_ERR_KERNEL;
				break;
			}
		} else {
			subsys_path[sub_len] = '\0';
			subsys = strrchr(subsys_path, '/') + 1;
		}
		ret = classify_device(dev_name, subsys, entry);
		if (!ret) {
			snprintf(linkpath, NAME_MAX, "%s/device", my_dev_path);
			free(my_dev_path);
			my_dev_path = realpath(linkpath, NULL);
			if (my_dev_path != NULL)
				dev_name = strrchr(my_dev_path, '/') + 1;
			else if (errno == ENOMEM)
				sensors_fatal_error(__func__, "Out of memory");
		}
	}

	if (my_dev_path != NULL)
		free(my_dev_path);
	return ret;
}

/* returns: number of devices added (0 or 1) if successful, <0 otherwise */
static int sensors_read_one_sysfs_chip(const char *dev_path,
				       const char *dev_name,
				       const char *hwmon_path)
{
	int ret = 1;
	int virtual = 0;
	sensors_chip_features entry;

	/* ignore any device without name attribute */
	if (!(entry.chip.prefix = sysfs_read_attr(hwmon_path, "name")))
		return 0;

	entry.chip.path = strdup(hwmon_path);
	if (!entry.chip.path)
		sensors_fatal_error(__func__, "Out of memory");

	if (dev_path == NULL) {
		virtual = 1;
	} else {
		ret = find_bus_type(dev_path, dev_name, &entry);
		if (ret == 0) {
			virtual = 1;
			ret = 1;
		} else if (ret < 0) {
			goto exit_free;
		}
	}
	if (virtual) {
		/* Virtual device */
		entry.chip.bus.type = SENSORS_BUS_TYPE_VIRTUAL;
		entry.chip.bus.nr = 0;
		/* For now we assume that virtual devices are unique */
		entry.chip.addr = 0;
	}

	if (sensors_read_dynamic_chip(&entry, hwmon_path) < 0) {
		ret = -SENSORS_ERR_KERNEL;
		goto exit_free;
	}
	if (!entry.subfeature) { /* No subfeature, discard chip */
		ret = 0;
		goto exit_free;
	}
	sensors_add_proc_chips(&entry);

	return ret;

exit_free:
	free(entry.chip.prefix);
	free(entry.chip.path);
	return ret;
}

static int sensors_add_hwmon_device_compat(const char *path,
					   const char *dev_name)
{
	int err;

	err = sensors_read_one_sysfs_chip(path, dev_name, path);
	if (err < 0)
		return err;
	return 0;
}

/* returns 0 if successful, !0 otherwise */
static int sensors_read_sysfs_chips_compat(void)
{
	int ret;

	ret = sysfs_foreach_busdev("i2c", sensors_add_hwmon_device_compat);
	if (ret && ret != ENOENT)
		return -SENSORS_ERR_KERNEL;

	return 0;
}

static int sensors_add_hwmon_device(const char *path, const char *classdev)
{
	char linkpath[NAME_MAX];
	char *dev_path, *dev_name;
	int err = 0;
	(void)classdev; /* hide warning */

	snprintf(linkpath, NAME_MAX, "%s/device", path);
	dev_path = realpath(linkpath, NULL);
	if (dev_path == NULL) {
		if (errno == ENOMEM) {
			sensors_fatal_error(__func__, "Out of memory");
		} else {
			/* No device link? Treat as virtual */
			err = sensors_read_one_sysfs_chip(NULL, NULL, path);
		}
	} else {
		dev_name = strrchr(dev_path, '/') + 1;

		/* The attributes we want might be those of the hwmon class
		   device, or those of the device itself. */
		err = sensors_read_one_sysfs_chip(dev_path, dev_name, path);
		if (err == 0)
			err = sensors_read_one_sysfs_chip(dev_path, dev_name,
							  dev_path);
		free(dev_path);
	}
	if (err < 0)
		return err;
	return 0;
}

/* returns 0 if successful, !0 otherwise */
int sensors_read_sysfs_chips(void)
{
	int ret;

	ret = sysfs_foreach_classdev("hwmon", sensors_add_hwmon_device);
	if (ret == ENOENT) {
		/* compatibility function for kernel 2.6.n where n <= 13 */
		return sensors_read_sysfs_chips_compat();
	}

	if (ret > 0)
		ret = -SENSORS_ERR_KERNEL;
	return ret;
}

/* returns 0 if successful, !0 otherwise */
static int sensors_add_i2c_bus(const char *path, const char *classdev)
{
	sensors_bus entry;

	if (sscanf(classdev, "i2c-%hd", &entry.bus.nr) != 1 ||
	    entry.bus.nr == 9191) /* legacy ISA */
		return 0;
	entry.bus.type = SENSORS_BUS_TYPE_I2C;

	/* Get the adapter name from the classdev "name" attribute
	 * (Linux 2.6.20 and later). If it fails, fall back to
	 * the device "name" attribute (for older kernels). */
	entry.adapter = sysfs_read_attr(path, "name");
	if (!entry.adapter)
		entry.adapter = sysfs_read_attr(path, "device/name");
	if (entry.adapter)
		sensors_add_proc_bus(&entry);

	return 0;
}

/* returns 0 if successful, !0 otherwise */
int sensors_read_sysfs_bus(void)
{
	int ret;

	ret = sysfs_foreach_classdev("i2c-adapter", sensors_add_i2c_bus);
	if (ret == ENOENT)
		ret = sysfs_foreach_busdev("i2c", sensors_add_i2c_bus);
	if (ret && ret != ENOENT)
		return -SENSORS_ERR_KERNEL;

	return 0;
}

int sensors_read_sysfs_attr(const sensors_chip_name *name,
			    const sensors_subfeature *subfeature,
			    double *value)
{
	char n[NAME_MAX];
	FILE *f;

	snprintf(n, NAME_MAX, "%s/%s", name->path, subfeature->name);
	if ((f = fopen(n, "r"))) {
		int res, err = 0;

		errno = 0;
		res = fscanf(f, "%lf", value);
		if (res == EOF && errno == EIO)
			err = -SENSORS_ERR_IO;
		else if (res != 1)
			err = -SENSORS_ERR_ACCESS_R;
		res = fclose(f);
		if (err)
			return err;

		if (res == EOF) {
			if (errno == EIO)
				return -SENSORS_ERR_IO;
			else
				return -SENSORS_ERR_ACCESS_R;
		}
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
		int res, err = 0;

		value *= get_type_scaling(subfeature->type);
		res = fprintf(f, "%d", (int) value);
		if (res == -EIO)
			err = -SENSORS_ERR_IO;
		else if (res < 0)
			err = -SENSORS_ERR_ACCESS_W;
		res = fclose(f);
		if (err)
			return err;

		if (res == EOF) {
			if (errno == EIO)
				return -SENSORS_ERR_IO;
			else
				return -SENSORS_ERR_ACCESS_W;
		}
	} else
		return -SENSORS_ERR_KERNEL;

	return 0;
}
