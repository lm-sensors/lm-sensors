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
MODULE_DIR := kernel
KERNELDIR := $(MODULE_DIR)

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
KERNELTARGETS := 
ifneq ($(shell if grep -q '^CONFIG_SENSORS=y' $(LINUX)/.config; then echo 1; fi),1)
# sensors.c moved to i2c-proc.c in i2c package
#KERNELTARGETS += $(MODULE_DIR)/sensors.o
endif

# Include all dependency files
INCLUDEFILES += $(KERNELTARGETS:.o=.d)

all-kernel: $(KERNELTARGETS)
all :: all-kernel

# Remove sensors.o possibly left from old versions
install-kernel: all-kernel
#	if [ -n "$(KERNELTARGETS)" ] ; then \
#	  $(MKDIR) $(DESTDIR)$(MODDIR) ; \
#	  $(INSTALL) -o root -g root -m 644 $(KERNELTARGETS) $(DESTDIR)$(MODDIR) ;\
#	fi
	$(RM) $(DESTDIR)$(MODPREF)/misc/sensors.o

install :: install-kernel 

clean-kernel:
	$(RM) $(KERNELDIR)/*.o $(KERNELDIR)/*.d
clean :: clean-kernel
