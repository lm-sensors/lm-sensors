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
MODULE_DIR := kernel/include
KERNELINCLUDEDIR := $(MODULE_DIR)

KERNELINCLUDEFILES := 
ifneq ($(shell if grep -q '^CONFIG_SENSORS=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELINCLUDEFILES += $(MODULE_DIR)/sensors.h
endif
# No longer used, part of <linux/i2c.h>
#ifneq ($(shell if grep -q '^CONFIG_I2C_ISA=y' $(LINUX)/.config; then echo 1; fi),1)
#KERNELINCLUDEFILES += $(MODULE_DIR)/i2c-isa.h
#endif

install-all-kernel-include:
	if [ -n "$(KERNELINCLUDEFILES)" ] ; then \
	  $(MKDIR) $(DESTDIR)$(SYSINCLUDEDIR) ; \
	  $(INSTALL) -o root -g root -m 644 $(KERNELINCLUDEFILES) $(DESTDIR)$(SYSINCLUDEDIR) ; \
	fi

install :: install-all-kernel-include

clean-all-kernel-include:
	$(RM) $(KERNELINCLUDEDIR)/*.h.install

clean :: clean-all-kernel-include
