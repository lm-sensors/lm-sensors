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
MODULE_DIR := kernel/busses

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
KERNELBUSSESTARGETS := $(MODULE_DIR)/i2c-piix4.o $(MODULE_DIR)/i2c-isa.o \
		       $(MODULE_DIR)/i2c-via.o $(MODULE_DIR)/i2c-ali15x3.o \
                       $(MODULE_DIR)/i2c-hydra.o $(MODULE_DIR)/i2c-voodoo3.o \
                       $(MODULE_DIR)/i2c-viapro.o $(MODULE_DIR)/i2c-i801.o

KERNELBUSSESOLD := bit-via.o bit-mb.o isa.o piix4.o

# Include all dependency files
INCLUDEFILES += $(KERNELBUSSESTARGETS:.o=.d)

all-kernel-busses: $(KERNELBUSSESTARGETS)
all :: all-kernel-busses

install-kernel-busses: all-kernel-busses
	$(RM) $(addprefix $(MODDIR)/,$(KERNELBUSSESOLD))
	$(MKDIR) $(MODDIR) 
	$(INSTALL) -o root -g root -m 644 $(KERNELBUSSESTARGETS) $(MODDIR)
install :: install-kernel-busses

clean-kernel-busses:
	$(RM) $(KERNELBUSSESTARGETS) $(KERNELBUSSESTARGETS:.o=.d)
clean :: clean-kernel-busses
