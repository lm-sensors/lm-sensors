#  Module.mk
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

MODULE_DIR := prog/xeon
PROGXEONDIR := $(MODULE_DIR)

PROGXEONTARGETS := $(MODULE_DIR)/decode-xeon.pl

REMOVEXEONBIN := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(BINDIR)/%,$(PROGXEONTARGETS))

install-prog-xeon: $(PROGXEONTARGETS)
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(PROGXEONTARGETS) $(DESTDIR)$(BINDIR)

user_install :: install-prog-xeon

user_uninstall::
	$(RM) $(REMOVEXEONBIN)
