#  Module.mk - Makefile for a Linux module for reading sensor data.
#  Copyright (c) 1998 - 2001 Frodo Looijaard <frodol@dds.nl>
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
KERNELCHIPSDIR := $(MODULE_DIR)

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
# These targets are NOT included in 'mkpatch' ...
KERNELCHIPSTARGETS := $(MODULE_DIR)/pcf8591.o

# These targets ARE included in 'mkpatch', except for LTC1710, which we
# leave here because it used to be in 'mkpatch' ...
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1021=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1021.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1024=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1024.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1025=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1025.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM9240=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm9240.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_BT869=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/bt869.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_DDCMON=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/ddcmon.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_DS1621=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/ds1621.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_EEPROM=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/eeprom.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_FSCPOS=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/fscpos.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_FSCSCY=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/fscscy.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_GL518SM=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/gl518sm.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_GL520SM=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/gl520sm.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_IT87=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/it87.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM75=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm75.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM78=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm78.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM80=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm80.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM87=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm87.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LTC1710=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/ltc1710.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MATORB=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/matorb.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MAXILIFE=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/maxilife.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MTP008=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/mtp008.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_PCF8574=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/pcf8574.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_SIS5595=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/sis5595.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_THMC50=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/thmc50.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_W83781D=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/w83781d.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_VIA686A=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/via686a.o
endif

# Include all dependency files
INCLUDEFILES += $(KERNELCHIPSTARGETS:.o=.d)

all-kernel-chips: $(KERNELCHIPSTARGETS)
all :: all-kernel-chips

install-kernel-chips: all-kernel-chips
	if [ -n "$(KERNELCHIPSTARGETS)" ] ; then \
	  $(MKDIR) $(DESTDIR)$(MODDIR) ; \
	  $(INSTALL) -o root -g root -m 644 $(KERNELCHIPSTARGETS) $(DESTDIR)$(MODDIR) ;\
	fi

install :: install-kernel-chips

clean-kernel-chips:
	$(RM) $(KERNELCHIPSDIR)/*.o $(KERNELCHIPSDIR)/*.d
clean :: clean-kernel-chips
