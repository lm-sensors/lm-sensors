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
MODULE_DIR := prog/sensors
PROGSENSORSDIR := $(MODULE_DIR)

PROGSENSORSMAN1DIR := $(MANDIR)/man1
PROGSENSORSMAN1FILES := $(MODULE_DIR)/sensors.1

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
PROGSENSORSTARGETS := $(MODULE_DIR)/sensors
PROGSENSORSSOURCES := $(MODULE_DIR)/main.c $(MODULE_DIR)/chips.c

# Include all dependency files. We use '.rd' to indicate this will create
# executables.
INCLUDEFILES += $(PROGSENSORSSOURCES:.c=.rd)

$(PROGSENSORSTARGETS): $(PROGSENSORSSOURCES:.c=.ro) lib/$(LIBSHBASENAME)
	$(CC) -o $@ $(PROGSENSORSSOURCES:.c=.ro) -Llib -lsensors

all-prog-sensors: $(PROGSENSORSTARGETS)
all :: all-prog-sensors

install-prog-sensors: all-prog-sensors
	mkdir -p $(BINDIR) $(PROGSENSORSMAN1DIR)
	$(INSTALL) -o root -g root -m 755 $(PROGSENSORSTARGETS) $(BINDIR)
	$(INSTALL) -o $(MANOWN) -g $(MANGRP) -m 644 $(PROGSENSORSMAN1FILES) $(PROGSENSORSMAN1DIR)
install :: install-prog-sensors

clean-prog-sensors:
	$(RM) $(PROGSENSORSDIR)/*.rd $(PROGSENSORSDIR)/*.ro 
	$(RM) $(PROGSENSORSTARGETS)
clean :: clean-prog-sensors
