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
KERNELBUSSESDIR := $(MODULE_DIR)

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
KERNELBUSSESTARGETS := $(MODULE_DIR)/i2c-i801.o \
                       $(MODULE_DIR)/i2c-viapro.o \
                       $(MODULE_DIR)/i2c-voodoo3.o \
                       $(MODULE_DIR)/i2c-amd756.o \
                       $(MODULE_DIR)/i2c-sis5595.o
ifneq ($(shell if grep -q '^CONFIG_I2C_ALI15X3=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-ali15x3.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_HYDRA=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-hydra.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_ISA=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-isa.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_PIIX4=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-piix4.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_VIA=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-via.o
endif

# Include all dependency files
INCLUDEFILES += $(KERNELBUSSESTARGETS:.o=.d)

all-kernel-busses: $(KERNELBUSSESTARGETS)
all :: all-kernel-busses

install-kernel-busses: all-kernel-busses
	$(MKDIR) $(MODDIR) 
	if [ -n "$(MODDIR)" ] ; then \
	  $(INSTALL) -o root -g root -m 644 $(KERNELBUSSESTARGETS) $(MODDIR) ; \
	fi

install :: install-kernel-busses

clean-kernel-busses:
	$(RM) $(KERNELBUSSESDIR)/*.o $(KERNELBUSSESDIR)/*.d
clean :: clean-kernel-busses
