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
# These targets are NOT included in 'mkpatch' ...
KERNELBUSSESTARGETS :=
ifeq ($(shell if grep -q '^CONFIG_IPMI_HANDLER=' $(LINUX)/.config; then echo 1; fi),1)
#doesn't work yet
#KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-ipmb.o
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-ipmi.o
endif

# These targets ARE included in 'mkpatch' ...
ifneq ($(shell if grep -q '^CONFIG_I2C_ALI1535=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-ali1535.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_ALI15X3=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-ali15x3.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_AMD756=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-amd756.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_AMD756_S4882=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-amd756-s4882.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_AMD8111=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-amd8111.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_HYDRA=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-hydra.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_I801=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-i801.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_I810=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-i810.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_ISA=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-isa.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_NFORCE2=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-nforce2.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_SIS5595=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-sis5595.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_SIS630=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-sis630.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_SIS645=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-sis645.o
endif
# don't compile dmi_scan unless x86 because it needs isa access
ifneq ($(shell if grep -q '^CONFIG_I2C_PIIX4=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-piix4.o
ifeq ($(shell if grep -q '^CONFIG_X86=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/dmi_scan.o
endif
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_SAVAGE4=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-savage4.o
endif
# don't compile unless alpha because of kernel include-file dependencies
ifeq ($(MACHINE),alpha)
ifneq ($(shell if grep -q '^CONFIG_I2C_TSUNAMI=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-tsunami.o
endif
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_VIA=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-via.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_VIAPRO=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-viapro.o
endif
ifneq ($(shell if grep -q '^CONFIG_I2C_VOODOO3=y' $(LINUX)/.config; then echo 1; fi),1)
KERNELBUSSESTARGETS += $(MODULE_DIR)/i2c-voodoo3.o
endif

# Include all dependency files
INCLUDEFILES += $(KERNELBUSSESTARGETS:.o=.d)

all-kernel-busses: $(KERNELBUSSESTARGETS)
all :: all-kernel-busses

#
# If $MODPREF/kernel exists, we presume the new (2.4.0) /lib/modules/x.y.z directory
# layout, so we install in kernel/drivers/i2c/busses and remove old versions in misc/
# and kernel/drivers/i2c/ . Otherwise we install in misc/ as before.
#
install-kernel-busses: all-kernel-busses
	if [ -n "$(KERNELBUSSESTARGETS)" ] ; then \
	  $(MKDIR) $(DESTDIR)$(MODPREF)/kernel/drivers/i2c/busses ; \
	  $(INSTALL) -m 644 $(KERNELBUSSESTARGETS) $(DESTDIR)$(MODPREF)/kernel/drivers/i2c/busses ; \
	  for i in $(KERNELBUSSESTARGETS) ; do \
	    $(RM) $(DESTDIR)$(MODPREF)/misc/`basename $$i` $(DESTDIR)$(MODPREF)/kernel/drivers/i2c/`basename $$i` \
	          $(DESTDIR)$(MODPREF)/kernel/drivers/i2c/`basename $$i`.gz $(DESTDIR)$(MODPREF)/kernel/drivers/i2c/busses/`basename $$i`.gz ; \
	  done ; \
	  $(RMDIR) $(DESTDIR)$(MODPREF)/misc 2> /dev/null || true ; \
	fi

install :: install-kernel-busses

clean-kernel-busses:
	$(RM) $(KERNELBUSSESDIR)/*.o $(KERNELBUSSESDIR)/*.d
clean :: clean-kernel-busses
