/*
    i2cbusses: print the installed i2c busses.
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

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <dirent.h>

/*
   this just prints out the installed i2c busses in a consistent format, whether
   on a 2.4 kernel using /proc or a 2.6 kernel using /sys.
*/
void print_i2c_busses()
{
	FILE *fptr;
	char s[100];
	struct dirent *de;
	DIR *dir;
	FILE *f;
	char *border;
	char dev[NAME_MAX], fstype[NAME_MAX], sysfs[NAME_MAX], n[NAME_MAX];
	int foundsysfs = 0;
	int tmp;
	int count=0;


	/* look in /proc/bus/i2c */
	if((fptr = fopen("/proc/bus/i2c", "r"))) {
		fprintf(stderr,"  Installed I2C busses:\n");
		while(fgets(s, 100, fptr)) {
			fprintf(stderr, "    %s", s);	
			count++;
		}
		fclose(fptr);
		if(count == 0)
			fprintf(stderr,"  Warning - no I2C busses found!\n");
		return;
	}

	/* look in sysfs */
	/* First figure out where sysfs was mounted */
	if ((f = fopen("/proc/mounts", "r")) == NULL)
		return;
	while (fgets(n, NAME_MAX, f)) {
		sscanf(n, "%[^ ] %[^ ] %[^ ] %*s\n", dev, sysfs, fstype);
		if (strcasecmp(fstype, "sysfs") == 0) {
			foundsysfs++;
			break;
		}
	}
	fclose(f);
	if (! foundsysfs)
		return;
	strcat(sysfs, "/class/i2c-adapter");
	dir = opendir(sysfs);
	if (! dir)
		return;
        fprintf(stderr,"  Installed I2C busses:\n");
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, "."))
			continue;
		if (!strcmp(de->d_name, ".."))
			continue;

		sprintf(n, "%s/%s/device/name", sysfs, de->d_name);

		if ((f = fopen(n, "r")) != NULL) {
			char	x[120];
			fgets(x, 120, f);
			fclose(f);
			if((border = index(x, '\n')) != NULL)
				*border = 0;
			if(!strncmp(x, "ISA ", 4))
				fprintf(stderr, "    %s\t%-10s\t%-32s\t%s\n", de->d_name,
				        "dummy", x, "ISA bus algorithm");
			else if(!sscanf(de->d_name, "i2c-%x", &tmp))
				fprintf(stderr, "    %s\t%-10s\t%-32s\t%s\n", de->d_name,
				        "dummy", x, "Dummy bus algorithm");
			else
				fprintf(stderr, "    %s\t%-10s\t%-32s\t%s\n", de->d_name,
				        "unknown", x, "Algorithm unavailable");
			count++;
		}
	}
	closedir(dir);
	if(count == 0)
		fprintf(stderr,"  Warning - no I2C busses found!\n");
}
