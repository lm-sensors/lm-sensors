#  Module.mk - Makefile for a Linux module for reading sensor data.
#  Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
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
MODULE_DIR := i2c/drivers

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
I2CDRIVERTARGETS := $(MODULE_DIR)/eeprom.o  

# Include all dependency files
INCLUDEFILES += $(I2CDRIVERTARGETS:.o=.d)

all-i2c-drivers: $(I2CDRIVERTARGETS)
all :: all-i2c-drivers

# No install rule: Our own eeprom.o driver is better :-)

clean-i2c-drivers:
	$(RM) $(I2CDRIVERTARGETS) $(I2CDRIVERTARGETS:.o=.d)
clean :: clean-i2c-drivers
