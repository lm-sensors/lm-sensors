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
KERNELCHIPSDIR := kernel/chips

KERNELINCLUDEFILES := $(MODULE_DIR)/i2c-dev.h $(MODULE_DIR)/sensors.h

$(KERNELINCLUDEDIR)/sensors.h: $(KERNELINCLUDEDIR)/sensors.h.template
	cat $@.template > $@
	$(AWK) '/SENSORS SYSCTL START/,/SENSORS SYSCTL END/' $(KERNELCHIPSDIR)/*.c >> $@
	echo '#endif' >> $@

$(KERNELINCLUDEDIR)/sensors.hd:
	( $(GREP) 'SENSORS SYSCTL START' /dev/null $(KERNELCHIPSDIR)/*.c | \
	  $(SED) -e 's/:.*//' -e 's#^#$(KERNELINCLUDEDIR)/sensors.h: #' ) > $@

# Get dependencies of sensors.h
INCLUDEFILES += $(MODULE_DIR)/sensors.hd

REMOVEKERNELINC := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(SYSINCLUDEDIR)/%,$(KERNELINCLUDEFILES))

install-all-kernel-include:
	if [ -n "$(KERNELINCLUDEFILES)" ] ; then \
	  $(MKDIR) $(DESTDIR)$(SYSINCLUDEDIR) ; \
	  $(INSTALL) -m 644 $(KERNELINCLUDEFILES) $(DESTDIR)$(SYSINCLUDEDIR) ; \
	fi

user_install :: install-all-kernel-include

user_uninstall::
	$(RM) $(REMOVEKERNELINC)

clean-all-kernel-include:
	$(RM) $(KERNELINCLUDEDIR)/*.h.install $(KERNELINCLUDEDIR)/sensors.h $(KERNELINCLUDEDIR)/sensors.hd

clean :: clean-all-kernel-include
