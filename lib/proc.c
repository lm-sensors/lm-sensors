/*
    proc.c - Part of libsensors, a Linux library for reading sensor data.
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>

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

/* for open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include "kernel/include/sensors.h"
#include "data.h"
#include "error.h"
#include "access.h"
#include "general.h"
#include <limits.h>
#include <dirent.h>

/* OK, this proves one thing: if there are too many chips detected, we get in
   trouble. The limit is around 4096/sizeof(struct sensors_chip_data), which
   works out to about 100 entries right now. That seems sensible enough,
   but if we ever get at the point where more chips can be detected, we must
   enlarge buf, and check that sysctl can handle larger buffers. */

#define BUF_LEN 4096

static char buf[BUF_LEN];

sensors_proc_chips_entry *sensors_proc_chips;
int sensors_proc_chips_count, sensors_proc_chips_max;
int sensors_sys_chips_count, sensors_sys_chips_max;

sensors_bus *sensors_sys_bus;
int sensors_sys_bus_count, sensors_sys_bus_max;
sensors_bus *sensors_proc_bus;
int sensors_proc_bus_count, sensors_proc_bus_max;

static int sensors_get_chip_id(sensors_chip_name name);

int foundsysfs=0;
char sysfsmount[NAME_MAX];

#define add_proc_chips(el) sensors_add_array_el(el,\
                                       &sensors_proc_chips,\
                                       &sensors_proc_chips_count,\
                                       &sensors_proc_chips_max,\
                                       sizeof(struct sensors_proc_chips_entry))

#define add_proc_bus(el) sensors_add_array_el(el,\
                                       &sensors_proc_bus,\
                                       &sensors_proc_bus_count,\
                                       &sensors_proc_bus_max,\
                                       sizeof(struct sensors_bus))

int getsysname(const sensors_chip_feature *feature, char *sysname,
	int *sysmag, char *altsysname);

/* This reads /proc/sys/dev/sensors/chips into memory */
int sensors_read_proc_chips(void)
{
	struct dirent *de;
	DIR *dir;
	FILE *f;
	char dev[NAME_MAX], fstype[NAME_MAX], sysfs[NAME_MAX], n[NAME_MAX];
	char dirname[NAME_MAX];
	int res;

	int name[3] = { CTL_DEV, DEV_SENSORS, SENSORS_CHIPS };
	int buflen = BUF_LEN;
	char *bufptr = buf;
	sensors_proc_chips_entry entry;
	int lineno;

	/* First figure out where sysfs was mounted */
	if ((f = fopen("/proc/mounts", "r")) == NULL)
		goto proc;
	while (fgets(n, NAME_MAX, f)) {
		sscanf(n, "%[^ ] %[^ ] %[^ ] %*s\n", dev, sysfs, fstype);
		if (strcasecmp(fstype, "sysfs") == 0) {
			foundsysfs++;
			break;
		}
	}
	fclose(f);
	if (! foundsysfs)
		goto proc;
	strcpy(sysfsmount, sysfs);
	strcat(sysfs, "/bus/i2c/devices");

	/* Then read from it */
	dir = opendir(sysfs);
	if (! dir)
		goto proc;

	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, "."))
			continue;
		if (!strcmp(de->d_name, ".."))
			continue;
/*
		if (de->d_type != DT_DIR && de->d_type != DT_LNK)
			continue;
*/

		sprintf(n, "%s/%s", sysfs, de->d_name);
		strcpy(dirname, n);
		strcat(n, "/name");

		if ((f = fopen(n, "r")) != NULL) {
			char x[81];
			int len = 0;
			if (!fscanf(f, "%80[a-zA-z0-9_ ]%n", x, &len)) {
				fclose(f);
				continue;
			}
			fclose(f);
			if (len >= 10 && !strcmp(x+len-10, " subclient"))
				continue;
			
			/* HACK */ strcat(x, "-*");
			if ((res = sensors_parse_chip_name(x, &entry.name))) {
				char	em[NAME_MAX + 20];
				strcpy(em, "Parsing ");
				strcat(em, n);
				sensors_parse_error(em, 0);
				return res;
			}
			entry.name.busname = strdup(dirname);
			sscanf(de->d_name, "%d-%x", &entry.name.bus, &entry.name.addr);
			/* find out if ISA or not */
			sprintf(n, "%s/class/i2c-adapter/i2c-%d/device/name",
			        sysfsmount, entry.name.bus);
			if ((f = fopen(n, "r")) != NULL) {
				fgets(x, 5, f);
				fclose(f);
				if(!strncmp(x, "ISA ", 4))
					entry.name.bus = SENSORS_CHIP_NAME_BUS_ISA;
			}
			add_proc_chips(&entry);
		}
	}
	closedir(dir);
	return 0;

proc:

  if (sysctl(name, 3, bufptr, &buflen, NULL, 0))
    return -SENSORS_ERR_PROC;

  lineno = 1;
  while (buflen >= sizeof(struct i2c_chips_data)) {
    if ((res = 
          sensors_parse_chip_name(((struct i2c_chips_data *) bufptr)->name, 
                                   &entry.name))) {
      sensors_parse_error("Parsing /proc/sys/dev/sensors/chips",lineno);
      return res;
    }
    entry.sysctl = ((struct i2c_chips_data *) bufptr)->sysctl_id;
    add_proc_chips(&entry);
    bufptr += sizeof(struct i2c_chips_data);
    buflen -= sizeof(struct i2c_chips_data);
    lineno++;
  }
  return 0;
}

int sensors_read_proc_bus(void)
{
	struct dirent *de;
	DIR *dir;
	FILE *f;
	char line[255];
	char *border;
	sensors_bus entry;
	int lineno;
	char sysfs[NAME_MAX], n[NAME_MAX];
	char dirname[NAME_MAX];

	if(foundsysfs) {
		strcpy(sysfs, sysfsmount);
		strcat(sysfs, "/class/i2c-adapter");
		/* Then read from it */
		dir = opendir(sysfs);
		if (! dir)
			goto proc;

		while ((de = readdir(dir)) != NULL) {
			if (!strcmp(de->d_name, "."))
				continue;
			if (!strcmp(de->d_name, ".."))
				continue;

			strcpy(n, sysfs);
			strcat(n, "/");
			strcat(n, de->d_name);
			strcpy(dirname, n);
			strcat(n, "/device/name");

			if ((f = fopen(n, "r")) != NULL) {
				char	x[120];
				fgets(x, 120, f);
				fclose(f);
				if((border = index(x, '\n')) != NULL)
					*border = 0;
				entry.adapter=strdup(x);
				if(!strncmp(x, "ISA ", 4)) {
					entry.number = SENSORS_CHIP_NAME_BUS_ISA;
					entry.algorithm = strdup("ISA bus algorithm");
				} else if(!sscanf(de->d_name, "i2c-%d", &entry.number)) {
					entry.number = SENSORS_CHIP_NAME_BUS_DUMMY;
					entry.algorithm = strdup("Dummy bus algorithm");
				} else
					entry.algorithm = strdup("Unavailable from sysfs");
				if (entry.algorithm == NULL)
					goto FAT_ERROR_SYS;
				add_proc_bus(&entry);
			}
		}
		closedir(dir);
		return 0;
FAT_ERROR_SYS:
		sensors_fatal_error("sensors_read_proc_bus", "Allocating entry");
		closedir(dir);
		return -SENSORS_ERR_PROC;
	}

proc:

  f = fopen("/proc/bus/i2c","r");
  if (!f)
    return -SENSORS_ERR_PROC;
  lineno=1;
  while (fgets(line,255,f)) {
    if (strlen(line) > 0)
      line[strlen(line)-1] = '\0';
    if (! (border = rindex(line,'\t')))
      goto ERROR;
    if (! (entry.algorithm = strdup(border+1)))
      goto FAT_ERROR;
    *border='\0';
    if (! (border = rindex(line,'\t')))
      goto ERROR;
    if (! (entry.adapter = strdup(border + 1)))
      goto FAT_ERROR;
    *border='\0';
    if (! (border = rindex(line,'\t')))
      goto ERROR;
    *border='\0';
    if (strncmp(line,"i2c-",4))
      goto ERROR;
    if (sensors_parse_i2cbus_name(line,&entry.number))
      goto ERROR;
    sensors_strip_of_spaces(entry.algorithm);
    sensors_strip_of_spaces(entry.adapter);
    add_proc_bus(&entry);
    lineno++;
  }
  fclose(f);
  return 0;
FAT_ERROR:
  sensors_fatal_error("sensors_read_proc_bus","Allocating entry");
ERROR:
  sensors_parse_error("Parsing /proc/bus/i2c",lineno);
  fclose(f);
  return -SENSORS_ERR_PROC;
}
    

/* This returns the first detected chip which matches the name */
int sensors_get_chip_id(sensors_chip_name name)
{
  int i;
  for (i = 0; i < sensors_proc_chips_count; i++)
    if (sensors_match_chip(name, sensors_proc_chips[i].name))
      return sensors_proc_chips[i].sysctl;
  return -SENSORS_ERR_NO_ENTRY;
}
  
/* This reads a feature /proc or /sys file.
   Sysfs uses a one-value-per file system...
   except for eeprom, which puts the entire eeprom into one file.
*/
int sensors_read_proc(sensors_chip_name name, int feature, double *value)
{
	int sysctl_name[4] = { CTL_DEV, DEV_SENSORS };
	const sensors_chip_feature *the_feature;
	int buflen = BUF_LEN;
	int mag, eepromoffset, fd, ret=0;

	if(!foundsysfs)
		if ((sysctl_name[2] = sensors_get_chip_id(name)) < 0)
			return sysctl_name[2];
	if (! (the_feature = sensors_lookup_feature_nr(name.prefix,feature)))
		return -SENSORS_ERR_NO_ENTRY;
	if(foundsysfs) {
		char n[NAME_MAX], altn[NAME_MAX];
		FILE *f;
		strcpy(n, name.busname);
		strcat(n, "/");
		/* total hack for eeprom */
		if (! strcmp(name.prefix, "eeprom")){
			strcat(n, "eeprom");
			/* we use unbuffered I/O to benefit from eeprom driver
			   optimization */
			if ((fd = open(n, O_RDONLY)) >= 0) {
				eepromoffset =
				  (the_feature->offset / sizeof(long))  +
				  (16 * (the_feature->sysctl - EEPROM_SYSCTL1));
				if (lseek(fd, eepromoffset, SEEK_SET) < 0
				 || read(fd, &ret, 1) != 1) {
					close(fd);
					return -SENSORS_ERR_PROC;
				}
				close(fd);
				*value = ret;
				return 0;
			} else
				return -SENSORS_ERR_PROC;
		} else {
			strcpy(altn, n);
			/* use rindex to append sysname to n */
			getsysname(the_feature, rindex(n, '\0'), &mag, rindex(altn, '\0'));
			if ((f = fopen(n, "r")) != NULL
			 || (f = fopen(altn, "r")) != NULL) {
				fscanf(f, "%lf", value);
				fclose(f);
				for (; mag > 0; mag --)
					*value /= 10.0;
		//		fprintf(stderr, "Feature %s value %lf scale %d offset %d\n",
		//			the_feature->name, *value,
		//			the_feature->scaling, the_feature->offset);
				return 0;
			} else
				return -SENSORS_ERR_PROC;
		}
	} else {
		sysctl_name[3] = the_feature->sysctl;
		if (sysctl(sysctl_name, 4, buf, &buflen, NULL, 0))
			return -SENSORS_ERR_PROC;
		*value = *((long *) (buf + the_feature->offset));
		for (mag = the_feature->scaling; mag > 0; mag --)
			*value /= 10.0;
		for (; mag < 0; mag ++)
			*value *= 10.0;
	}
	return 0;
}
  
int sensors_write_proc(sensors_chip_name name, int feature, double value)
{
	int sysctl_name[4] = { CTL_DEV, DEV_SENSORS };
	const sensors_chip_feature *the_feature;
	int buflen = BUF_LEN;
	int mag;
 
	if(!foundsysfs)
		if ((sysctl_name[2] = sensors_get_chip_id(name)) < 0)
			return sysctl_name[2];
	if (! (the_feature = sensors_lookup_feature_nr(name.prefix,feature)))
		return -SENSORS_ERR_NO_ENTRY;
	if(foundsysfs) {
		char n[NAME_MAX], altn[NAME_MAX];
		FILE *f;
		strcpy(n, name.busname);
		strcat(n, "/");
		strcpy(altn, n);
		/* use rindex to append sysname to n */
		getsysname(the_feature, rindex(n, '\0'), &mag, rindex(altn, '\0'));
		if ((f = fopen(n, "w")) != NULL
		 || (f = fopen(altn, "w")) != NULL) {
			for (; mag > 0; mag --)
				value *= 10.0;
			fprintf(f, "%d", (int) value);
			fclose(f);
		} else
			return -SENSORS_ERR_PROC;
	} else {
		sysctl_name[3] = the_feature->sysctl;
		if (sysctl(sysctl_name, 4, buf, &buflen, NULL, 0))
			return -SENSORS_ERR_PROC;
		/* The following line is known to solve random problems, still it
		   can't be considered a definitive solution...
		if (sysctl_name[0] != CTL_DEV) { sysctl_name[0] = CTL_DEV ; } */
		for (mag = the_feature->scaling; mag > 0; mag --)
			value *= 10.0;
		for (; mag < 0; mag ++)
			value /= 10.0;
		* ((long *) (buf + the_feature->offset)) = (long) value;
		buflen = the_feature->offset + sizeof(long);
#ifdef DEBUG
		/* The following get* calls don't do anything, they are here
		   for debugging purposes only. Strace will show the
		   returned values. */
		getuid(); geteuid();
		getgid(); getegid();
#endif
		if (sysctl(sysctl_name, 4, NULL, 0, buf, buflen))
			return -SENSORS_ERR_PROC;
	}
	return 0;
}

#define CURRMAG 3
#define FANMAG 0
#define INMAG 3
#define TEMPMAG 3

/*
	Returns the sysfs name and magnitude for a given feature.
	First looks for a sysfs name and magnitude in the feature structure.
	These should be added in chips.c for all non-standard feature names.
	If that fails, converts common /proc feature names
	to their sysfs equivalent, and uses common sysfs magnitude.
	Common magnitudes are #defined above.
	Common conversions are as follows:
		fan%d_min -> fan%d_min (for magnitude)
		fan%d_state -> fan%d_status
		fan%d -> fan_input%d
		pwm%d -> fan%d_pwm
		pwm%d_enable -> fan%d_pwm_enable
		in%d_max -> in%d_max (for magnitude)
		in%d_min -> in%d_min (for magnitude)
		in%d -> in%d_input
		vin%d_max -> in%d_max
		vin%d_min -> in%d_min
		vin%d -> in_input%d
		temp%d_over -> temp%d_max
		temp%d_hyst -> temp%d_max_hyst
		temp%d_max -> temp%d_max (for magnitude)
		temp%d_high -> temp%d_max
		temp%d_min -> temp%d_min (for magnitude)
		temp%d_low -> temp%d_min
		temp%d_state -> temp%d_status
		temp%d -> temp%d_input
		sensor%d -> temp%d_type
	AND all conversions listed in the matches[] structure below.

	If that fails, returns old /proc feature name and magnitude.

	References: doc/developers/proc in the lm_sensors package;
	            Documentation/i2c/sysfs_interface in the kernel
*/
int getsysname(const sensors_chip_feature *feature, char *sysname,
	int *sysmag, char *altsysname)
{
	const char * name = feature->name;
	char last;
	char check; /* used to verify end of string */
	int num;
	
	struct match {
		const char * name, * sysname;
		const int sysmag;
		const char * altsysname;
	};

	struct match *m;

	struct match matches[] = {
		{ "beeps", "beep_mask", 0 },
		{ "pwm", "fan1_pwm", 0 },
		{ "vid", "cpu0_vid", INMAG, "in0_ref" },
		{ "remote_temp", "temp2_input", TEMPMAG },
		{ "remote_temp_hyst", "temp2_max_hyst", TEMPMAG },
		{ "remote_temp_low", "temp2_min", TEMPMAG },
		{ "remote_temp_over", "temp2_max", TEMPMAG },
		{ "temp", "temp1_input", TEMPMAG },
		{ "temp_hyst", "temp1_max_hyst", TEMPMAG },
		{ "temp_low", "temp1_min", TEMPMAG },
		{ "temp_over", "temp1_max", TEMPMAG },
		{ "temp_high", "temp1_max", TEMPMAG },
		{ "temp_crit", "temp1_crit", TEMPMAG },
		{ "pwm1", "pwm1", 0, "fan1_pwm" },
		{ "pwm2", "pwm2", 0, "fan2_pwm" },
		{ "pwm3", "pwm3", 0, "fan3_pwm" },
		{ "pwm4", "pwm4", 0, "fan4_pwm" },
		{ "pwm1_enable", "pwm1_enable", 0, "fan1_pwm_enable" },
		{ "pwm2_enable", "pwm2_enable", 0, "fan2_pwm_enable" },
		{ "pwm3_enable", "pwm3_enable", 0, "fan3_pwm_enable" },
		{ "pwm4_enable", "pwm4_enable", 0, "fan4_pwm_enable" },
		{ NULL, NULL }
	};


/* default to a non-existent alternate name (should rarely be tried) */
	strcpy(altsysname, "_");

/* use override in feature structure if present */
	if(feature->sysname != NULL) {
		strcpy(sysname, feature->sysname);
		if(feature->sysscaling)
			*sysmag = feature->sysscaling;
		else
			*sysmag = feature->scaling;
		if(feature->altsysname != NULL)
			strcpy(altsysname, feature->altsysname);
		return 0;
	}

/* check for constant mappings */
	for(m = matches; m->name != NULL; m++) {
		if(!strcmp(m->name, name)) {
			strcpy(sysname, m->sysname);
			if (m->altsysname != NULL)
				strcpy(altsysname, m->altsysname);
			*sysmag = m->sysmag;
			return 0;
		}
	}

/* convert common /proc names to common sysfs names */
	if(sscanf(name, "fan%d_mi%c%c", &num, &last, &check) == 2 && last == 'n') {
		strcpy(sysname, name);
		*sysmag = FANMAG;
		return 0;
	}
	if(sscanf(name, "fan%d_stat%c%c", &num, &last, &check) == 2 && last == 'e') {
		sprintf(sysname, "fan%d_status", num);
		*sysmag = 0;
		return 0;
	}
	if(sscanf(name, "fan%d%c", &num, &check) == 1) {
		sprintf(sysname, "fan%d_input", num);
		*sysmag = FANMAG;
		return 0;
	}
	if(sscanf(name, "pwm%d%c", &num, &check) == 1) {
		sprintf(sysname, "fan%d_pwm", num);
		*sysmag = 0;
		return 0;
	}
	if(sscanf(name, "pwm%d_enabl%c%c", &num, &last, &check) == 2 && last == 'e') {
		sprintf(sysname, "fan%d_pwm_enable", num);
		*sysmag = 0;
		return 0;
	}

	if((sscanf(name, "in%d_mi%c%c", &num, &last, &check) == 2 && last == 'n')
	|| (sscanf(name, "in%d_ma%c%c", &num, &last, &check) == 2 && last == 'x')) {
		strcpy(sysname, name);
		*sysmag = INMAG;
		return 0;
	}
	if((sscanf(name, "in%d%c", &num, &check) == 1)
	|| (sscanf(name, "vin%d%c", &num, &check) == 1)) {
		sprintf(sysname, "in%d_input", num);
		*sysmag = INMAG;
		return 0;
	}
	if(sscanf(name, "vin%d_mi%c%c", &num, &last, &check) == 2 && last == 'n') {
		sprintf(sysname, "in%d_min", num);
		*sysmag = INMAG;
		return 0;
	}
	if(sscanf(name, "vin%d_ma%c%c", &num, &last, &check) == 2 && last == 'x') {
		sprintf(sysname, "in%d_max", num);
		*sysmag = INMAG;
		return 0;
	}

	if(sscanf(name, "temp%d_hys%c%c", &num, &last, &check) == 2 && last == 't') {
		sprintf(sysname, "temp%d_max_hyst", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if((sscanf(name, "temp%d_ove%c%c", &num, &last, &check) == 2 && last == 'r')
	|| (sscanf(name, "temp%d_ma%c%c", &num, &last, &check) == 2 && last == 'x')
	|| (sscanf(name, "temp%d_hig%c%c", &num, &last, &check) == 2 && last == 'h')) {
		sprintf(sysname, "temp%d_max", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if((sscanf(name, "temp%d_mi%c%c", &num, &last, &check) == 2 && last == 'n')
	|| (sscanf(name, "temp%d_lo%c%c", &num, &last, &check) == 2 && last == 'w')) {
		sprintf(sysname, "temp%d_min", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if(sscanf(name, "temp%d_cri%c%c", &num, &last, &check) == 2 && last == 't') {
		sprintf(sysname, "temp%d_crit", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if(sscanf(name, "temp%d_stat%c%c", &num, &last, &check) == 2 && last == 'e') {
		sprintf(sysname, "temp%d_status", num);
		*sysmag = 0;
		return 0;
	}
	if(sscanf(name, "temp%d%c", &num, &check) == 1) {
		sprintf(sysname, "temp%d_input", num);
		*sysmag = TEMPMAG;
		return 0;
	}
	if(sscanf(name, "sensor%d%c", &num, &check) == 1) {
		sprintf(sysname, "temp%d_type", num);
		*sysmag = 0;
		return 0;
	}

/* bmcsensors only, not yet in kernel */
/*
	if((sscanf(name, "curr%d_mi%c%c", &num, &last, &check) == 2 && last == 'n')
	|| (sscanf(name, "curr%d_ma%c%c", &num, &last, &check) == 2 && last == 'x')) {
		strcpy(sysname, name);
		*sysmag = CURRMAG;
		return 0;
	}
	if(sscanf(name, "curr%d%c", &num, &check) == 1) {
		sprintf(sysname, "curr%d_input", num);
		*sysmag = CURRMAG;
		return 0;
	}
*/

/* give up, use old name (probably won't work though...) */
/* known to be the same:
	"alarms", "beep_enable", "vrm", "fan%d_div"
*/
	strcpy(sysname, name);
	*sysmag = feature->scaling;
	return 0;
}
