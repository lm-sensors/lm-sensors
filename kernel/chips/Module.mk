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

# These targets are NOT included in 'mkpatch'
KERNELCHIPSTARGETS :=
ifeq ($(shell if grep -q '^CONFIG_IPMI_HANDLER=' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/bmcsensors.o
endif
KERNELCHIPSTARGETS += $(MODULE_DIR)/ds1307.o
KERNELCHIPSTARGETS += $(MODULE_DIR)/ltc1710.o
KERNELCHIPSTARGETS += $(MODULE_DIR)/mic74.o
KERNELCHIPSTARGETS += $(MODULE_DIR)/saa1064.o
KERNELCHIPSTARGETS += $(MODULE_DIR)/smartbatt.o
KERNELCHIPSTARGETS += $(MODULE_DIR)/smbus-arp.o
KERNELCHIPSTARGETS += $(MODULE_DIR)/w83792d.o


# These targets ARE included in 'mkpatch'
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1021=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1021.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1024=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1024.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1025=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1025.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1026=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1026.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM1031=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm1031.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ADM9240=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/adm9240.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_ASB100=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/asb100.o
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
ifneq ($(shell if grep -q '^CONFIG_SENSORS_FSCHER=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/fscher.o
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
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM63=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm63.o
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
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM83=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm83.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM85=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm85.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM87=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm87.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM90=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm90.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM92=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm92.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_LM93=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/lm93.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MATORB=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/matorb.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MAX1619=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/max1619.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MAX6650=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/max6650.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MAXILIFE=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/maxilife.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_MTP008=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/mtp008.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_PC87360=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/pc87360.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_PCA9540=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/pca9540.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_PCF8574=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/pcf8574.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_PCF8591=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/pcf8591.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_SIS5595=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/sis5595.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_SMSC47M1=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/smsc47m1.o
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
ifneq ($(shell if grep -q '^CONFIG_SENSORS_VT1211=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/vt1211.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_VT8231=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/vt8231.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_W83627HF=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/w83627hf.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_W83L785TS=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/w83l785ts.o
endif
ifneq ($(shell if grep -q '^CONFIG_SENSORS_XEONTEMP=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELCHIPSTARGETS += $(MODULE_DIR)/xeontemp.o
endif

# Include all dependency files
INCLUDEFILES += $(KERNELCHIPSTARGETS:.o=.d)

all-kernel-chips: $(KERNELCHIPSTARGETS)
all :: all-kernel-chips

#
# If $MODPREF/kernel exists, we presume the new (2.4.0) /lib/modules/x.y.z directory
# layout, so we install in kernel/drivers/i2c/chips/ and remove old versions in misc/
# and kernel/drivers/sensors/ . Otherwise we install in misc/ as before.
#
install-kernel-chips: all-kernel-chips
	if [ -n "$(KERNELCHIPSTARGETS)" ] ; then \
	  $(MKDIR) $(DESTDIR)$(MODPREF)/kernel/drivers/i2c/chips ; \
	  $(INSTALL) -m 644 $(KERNELCHIPSTARGETS) $(DESTDIR)$(MODPREF)/kernel/drivers/i2c/chips ; \
	  for i in $(KERNELCHIPSTARGETS) ; do \
	    $(RM) $(DESTDIR)$(MODPREF)/misc/`basename $$i` $(DESTDIR)$(MODPREF)/kernel/drivers/sensors/`basename $$i` \
	          $(DESTDIR)$(MODPREF)/kernel/drivers/sensors/`basename $$i`.gz $(DESTDIR)$(MODPREF)/kernel/drivers/chips/`basename $$i`.gz ; \
	  done ; \
	  $(RMDIR) $(DESTDIR)$(MODPREF)/misc $(DESTDIR)$(MODPREF)/kernel/drivers/sensors 2> /dev/null || true ; \
	fi

install :: install-kernel-chips

clean-kernel-chips:
	$(RM) $(KERNELCHIPSDIR)/*.o $(KERNELCHIPSDIR)/*.d
clean :: clean-kernel-chips
