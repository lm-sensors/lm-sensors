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

MODULE_DIR := prog/pwm
PROGPWMDIR := $(MODULE_DIR)

PROGPWMTARGETS := $(MODULE_DIR)/fancontrol \
                  $(MODULE_DIR)/fancontrol.pl \
                  $(MODULE_DIR)/pwmconfig

REMOVEPWMBIN := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(SBINDIR)/%,$(PROGPWMTARGETS))

install-prog-pwm: $(PROGPWMTARGETS)
	$(MKDIR) $(DESTDIR)$(SBINDIR)
	$(INSTALL) -m 755 $(PROGPWMTARGETS) $(DESTDIR)$(SBINDIR)

user_install :: install-prog-pwm

user_uninstall::
	$(RM) $(REMOVEPWMBIN)
