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
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301 USA.

# Note that MODULE_DIR (the directory in which this file resides) is a
# 'simply expanded variable'. That means that its value is substituted
# verbatim in the rules, until it is redefined. 
MODULE_DIR := etc
ETC_DIR := $(MODULE_DIR)

ETCTARGET := $(MODULE_DIR)/sensors.conf.default
ETCINSTALL := $(ETCDIR)/sensors3.conf


# No all rule

install-etc:
	$(MKDIR) $(DESTDIR)$(ETCDIR)
	if [ ! -e $(DESTDIR)$(ETCINSTALL) ] ; then \
	  $(INSTALL) -m 644 $(ETCTARGET) $(DESTDIR)$(ETCINSTALL); \
	fi
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(ETC_DIR)/sensors-conf-convert $(DESTDIR)$(BINDIR)

user_install :: install-etc

uninstall-etc:
	$(RM) $(DESTDIR)$(BINDIR)/sensors-conf-convert

user_uninstall :: uninstall-etc

# No clean rule
