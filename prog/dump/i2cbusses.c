/*
    i2cbusses: Print the installed i2c busses for both 2.4 and 2.6 kernels.
               Part of user-space programs to access for I2C 
               devices.
    Copyright (c) 1999-2003  Frodo Looijaard <frodol@dds.nl> and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>

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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include "i2cbusses.h"

/*
   this just prints out the installed i2c busses in a consistent format, whether
   on a 2.4 kernel using /proc or a 2.6 kernel using /sys.
   If procfmt == 1, print out exactly /proc/bus/i2c format on stdout.
   This allows this to be used in a program to emulate /proc/bus/i2c on a
   sysfs system.
*/
void print_i2c_busses(int procfmt)
{
	FILE *fptr;
	char s[100];
	struct dirent *de, *dde;
	DIR *dir, *ddir;
	FILE *f;
	char *border;
	char dev[NAME_MAX], fstype[NAME_MAX], sysfs[NAME_MAX], n[NAME_MAX];
	int foundsysfs = 0;
	int tmp;
	int count=0;


	/* look in /proc/bus/i2c */
	if((fptr = fopen("/proc/bus/i2c", "r"))) {
		while(fgets(s, 100, fptr)) {
			if(count++ == 0 && !procfmt)
				fprintf(stderr,"  Installed I2C busses:\n");
			if(procfmt)
				printf("%s", s);	
			else
				fprintf(stderr, "    %s", s);	
		}
		fclose(fptr);
		goto done;
	}

	/* look in sysfs */
	/* First figure out where sysfs was mounted */
	if ((f = fopen("/proc/mounts", "r")) == NULL) {
		goto done;
	}
	while (fgets(n, NAME_MAX, f)) {
		sscanf(n, "%[^ ] %[^ ] %[^ ] %*s\n", dev, sysfs, fstype);
		if (strcasecmp(fstype, "sysfs") == 0) {
			foundsysfs++;
			break;
		}
	}
	fclose(f);
	if (! foundsysfs) {
		goto done;
	}

	/* Bus numbers in i2c-adapter don't necessarily match those in
	   i2c-dev and what we really care about are the i2c-dev numbers.
	   Unfortunately the names are harder to get in i2c-dev */
	strcat(sysfs, "/class/i2c-dev");
	if(!(dir = opendir(sysfs)))
		goto done;
	/* go through the busses */
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, "."))
			continue;
		if (!strcmp(de->d_name, ".."))
			continue;

		/* this should work for kernels 2.6.5 or higher and */
		/* is preferred because is unambiguous */
		sprintf(n, "%s/%s/name", sysfs, de->d_name);
		f = fopen(n, "r");
		/* this seems to work for ISA */
		if(f == NULL) {
			sprintf(n, "%s/%s/device/name", sysfs, de->d_name);
			f = fopen(n, "r");
		}
		/* non-ISA is much harder */
		/* and this won't find the correct bus name if a driver
		   has more than one bus */
		if(f == NULL) {
			sprintf(n, "%s/%s/device", sysfs, de->d_name);
			if(!(ddir = opendir(n)))
				continue;       	
			while ((dde = readdir(ddir)) != NULL) {
				if (!strcmp(dde->d_name, "."))
					continue;
				if (!strcmp(dde->d_name, ".."))
					continue;
				if ((!strncmp(dde->d_name, "i2c-", 4))) {
					sprintf(n, "%s/%s/device/%s/name",
					        sysfs, de->d_name, dde->d_name);
					if((f = fopen(n, "r")))
						goto found;
				}
			}
		}

found:
		if (f != NULL) {
			char	x[120];

			fgets(x, 120, f);
			fclose(f);
			if((border = index(x, '\n')) != NULL)
				*border = 0;
			if(count++ == 0 && !procfmt)
				fprintf(stderr,"  Installed I2C busses:\n");
			/* match 2.4 /proc/bus/i2c format as closely as possible */
			if(!strncmp(x, "ISA ", 4)) {
				if(procfmt)
					printf("%s\t%-10s\t%-32s\t%s\n", de->d_name,
					        "dummy", x, "ISA bus algorithm");
				else
					fprintf(stderr, "    %s\t%-10s\t%-32s\t%s\n", de->d_name,
					        "dummy", x, "ISA bus algorithm");
			} else if(!sscanf(de->d_name, "i2c-%d", &tmp)) {
				if(procfmt)
					printf("%s\t%-10s\t%-32s\t%s\n", de->d_name,
					        "dummy", x, "Dummy bus algorithm");
				else
					fprintf(stderr, "    %s\t%-10s\t%-32s\t%s\n", de->d_name,
					        "dummy", x, "Dummy bus algorithm");
			} else {
				if(procfmt)
					printf("%s\t%-10s\t%-32s\t%s\n", de->d_name,
					        "unknown", x, "Algorithm unavailable");
				else
					fprintf(stderr, "    %s\t%-10s\t%-32s\t%s\n", de->d_name,
					        "unknown", x, "Algorithm unavailable");
			}
		}
	}
	closedir(dir);

done:
	if(count == 0 && !procfmt)
		fprintf(stderr,"Error: No I2C busses found!\n"
		               "Be sure you have done 'modprobe i2c-dev'\n"
		               "and also modprobed your i2c bus drivers\n");
}

int open_i2c_dev(const int i2cbus, char *filename)
{
	int file;

	sprintf(filename, "/dev/i2c/%d", i2cbus);
	file = open(filename, O_RDWR);

	if (file < 0 && errno == ENOENT) {
		sprintf(filename, "/dev/i2c-%d", i2cbus);
		file = open(filename, O_RDWR);
	}

	if (file < 0) {
		if (errno == ENOENT) {
			fprintf(stderr, "Error: Could not open file "
			        "`/dev/i2c-%d' or `/dev/i2c/%d': %s\n",
			        i2cbus, i2cbus, strerror(ENOENT));
		} else {
			fprintf(stderr, "Error: Could not open file "
			        "`%s': %s\n", filename, strerror(errno));
			if (errno == EACCES)
				fprintf(stderr, "Run as root?\n");
		}
	}
	
	return file;
}
