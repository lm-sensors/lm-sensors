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
MODULE_DIR := kernel/chips

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
KERNELCHIPSTARGETS := $(MODULE_DIR)/adm1021.o $(MODULE_DIR)/adm9240.o \
	              $(MODULE_DIR)/eeprom.o $(MODULE_DIR)/gl518sm.o \
        	      $(MODULE_DIR)/lm75.o $(MODULE_DIR)/lm78.o \
        	      $(MODULE_DIR)/lm80.o $(MODULE_DIR)/ltc1710.o \
        	      $(MODULE_DIR)/w83781d.o $(MODULE_DIR)/sis5595.o \
        	      $(MODULE_DIR)/maxilife.o

# Include all dependency files
INCLUDEFILES += $(KERNELCHIPSTARGETS:.o=.d)

all-kernel-chips: $(KERNELCHIPSTARGETS)
all :: all-kernel-chips

install-kernel-chips: all-kernel-chips
	$(MKDIR) $(MODDIR) 
	$(INSTALL) -o root -g root -m 644 $(KERNELCHIPSTARGETS) $(MODDIR)
install :: install-kernel-chips

clean-kernel-chips:
	$(RM) $(KERNELCHIPSTARGETS) $(KERNELCHIPSTARGETS:.o=.d)
clean :: clean-kernel-chips
