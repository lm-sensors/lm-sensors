#  Module.mk - Makefile for a Linux module for reading sensor data.
#  Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Note that MODULE_DIR (the directory in which this file resides) is a
# 'simply expanded variable'. That means that its value is substituted
# verbatim in the rules, until it is redefined. 
MODULE_DIR := i2c/eeprom

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
I2CEEPROMTARGETS := $(MODULE_DIR)/eeprom
I2CEEPROMSOURCES := $(MODULE_DIR)/eeprom.c

# Include all dependency files
INCLUDEFILES += $(I2CEEPROMSOURCES:.c=.d)

all :: $(I2CEEPROMTARGETS)

# No install rule

clean ::
	$(RM) $(I2CEEPROMSOURCES:.c=.d) $(I2CEEPROMSOURCES:.c=.o) \
	      $(I2CEEPROMTARGETS)

# The targets
$(MODULE_DIR)/eeprom: $(MODULE_DIR)/eeprom.o


# Oops, we need to use EXCFLAGS instead of CFLAGS... And we have to deal with
# an executable. Ugly code approaching... :-)

$(I2CEEPROMSOURCES:.c=.o):
	$(CC) $(EXCFLAGS) -c $(@:.o=.c) -o $@

$(I2CEEPROMSOURCES:.c=.d):
	$(CC) -M -MG $(EXCFLAGS) $(@:.d=.c) | \
	sed -e \
        's@^\(.*\)\.o:@$@ $(@:.d=.o) Makefile '`dirname $@`/Module.mk':@' > $@

