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
MODULE_DIR := i2c

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
I2CTARGETS := $(MODULE_DIR)/i2c-core.o  $(MODULE_DIR)/algo-bit.o \
              $(MODULE_DIR)/i2c-dev.o   $(MODULE_DIR)/bit-lp.o \
              $(MODULE_DIR)/bit-velle.o $(MODULE_DIR)/bit-mb.o

I2CHEADERFILES := $(MODULE_DIR)/i2c.h

# Include all dependency files
INCLUDEFILES += $(I2CTARGETS:.o=.d)

all-i2c: $(I2CTARGETS)
all :: all-i2c

install-i2c:
	$(MKDIR) $(MODDIR) $(SYSINCLUDEDIR)
	$(INSTALL) -o root -g root -m 644 $(I2CTARGETS) $(MODDIR)
	$(INSTALL) -o root -g root -m 644 $(I2CHEADERFILES) $(SYSINCLUDEDIR)
install :: install-i2c

clean-i2c:
	$(RM) $(I2CTARGETS) $(I2CTARGETS:.o=.d)
clean :: clean-i2c
