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
MODULE_DIR := prog/dump
PROGDUMPDIR := $(MODULE_DIR)

# Regrettably, even 'simply expanded variables' will not put their currently
# defined value verbatim into the command-list of rules...
PROGDUMPTARGETS := $(MODULE_DIR)/isadump $(MODULE_DIR)/i2cdump
PROGDUMPSOURCES := $(MODULE_DIR)/isadump.c  $(MODULE_DIR)/i2cdump.c

# Include all dependency files. We use '.rd' to indicate this will create
# executables.
INCLUDEFILES += $(PROGDUMPSOURCES:.c=.rd)

all-prog-dump: $(PROGDUMPTARGETS)
all :: all-prog-dump

clean-prog-dump:
	$(RM) $(PROGDUMPDIR)/*.rd $(PROGDUMPDIR)/*.ro $(PROGDUMPTARGETS)
clean :: clean-prog-dump
