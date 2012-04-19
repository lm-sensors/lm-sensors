#  Module.mk - Makefile for a Linux module for reading sensor data.
#  Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301 USA.

# Note that MODULE_DIR (the directory in which this file resides) is a
# 'simply expanded variable'. That means that its value is substituted
# verbatim in the rules, until it is redefined. 
MODULE_DIR := lib
LIB_DIR := $(MODULE_DIR)

# The manual dirs and files
LIBMAN3DIR := $(MANDIR)/man3
LIBMAN3FILES := $(MODULE_DIR)/libsensors.3
LIBMAN5DIR := $(MANDIR)/man5
LIBMAN5FILES := $(MODULE_DIR)/sensors.conf.5

# The main and minor version of the library
# The library soname (major number) must be changed if and only if the interface is
# changed in a backward incompatible way.  The interface is defined by
# the public header files - in this case they are error.h and sensors.h.
LIBMAINVER := 4
LIBMINORVER := 3.2
LIBVER := $(LIBMAINVER).$(LIBMINORVER)

# The static lib name, the shared lib name, and the internal ('so') name of
# the shared lib.
LIBSHBASENAME := libsensors.so
LIBSHLIBNAME := libsensors.so.$(LIBVER)
LIBSTLIBNAME := libsensors.a
LIBSHSONAME := libsensors.so.$(LIBMAINVER)

LIBTARGETS := $(MODULE_DIR)/$(LIBSHLIBNAME) \
              $(MODULE_DIR)/$(LIBSHSONAME) $(MODULE_DIR)/$(LIBSHBASENAME)
ifeq ($(BUILD_STATIC_LIB),1)
LIBTARGETS += $(MODULE_DIR)/$(LIBSTLIBNAME)
endif

LIBCSOURCES := $(MODULE_DIR)/data.c $(MODULE_DIR)/general.c \
               $(MODULE_DIR)/error.c $(MODULE_DIR)/access.c \
               $(MODULE_DIR)/init.c $(MODULE_DIR)/sysfs.c

LIBOTHEROBJECTS := $(MODULE_DIR)/conf-parse.o $(MODULE_DIR)/conf-lex.o
LIBSHOBJECTS := $(LIBCSOURCES:.c=.lo) $(LIBOTHEROBJECTS:.o=.lo)
LIBSTOBJECTS := $(LIBCSOURCES:.c=.ao) $(LIBOTHEROBJECTS:.o=.ao)
LIBEXTRACLEAN := $(MODULE_DIR)/conf-parse.h $(MODULE_DIR)/conf-parse.c \
                 $(MODULE_DIR)/conf-lex.c

LIBHEADERFILES := $(MODULE_DIR)/error.h $(MODULE_DIR)/sensors.h

# How to create the shared library
$(MODULE_DIR)/$(LIBSHLIBNAME): $(LIBSHOBJECTS)
	$(CC) -shared $(LDFLAGS) -Wl,--version-script=$(LIB_DIR)/libsensors.map -Wl,-soname,$(LIBSHSONAME) -o $@ $^ -lc -lm

$(MODULE_DIR)/$(LIBSHSONAME): $(MODULE_DIR)/$(LIBSHLIBNAME)
	$(RM) $@
	$(LN) $(LIBSHLIBNAME) $@

$(MODULE_DIR)/$(LIBSHBASENAME): $(MODULE_DIR)/$(LIBSHLIBNAME)
	$(RM) $@ 
	$(LN) $(LIBSHLIBNAME) $@

# And the static library
$(MODULE_DIR)/$(LIBSTLIBNAME): $(LIBSTOBJECTS)
	$(RM) $@
	$(AR) rcvs $@ $^

# Depencies for non-C sources
$(MODULE_DIR)/conf-lex.c: $(MODULE_DIR)/conf-lex.l $(MODULE_DIR)/general.h \
                          $(MODULE_DIR)/data.h $(MODULE_DIR)/conf-parse.h
$(MODULE_DIR)/conf-parse.c: $(MODULE_DIR)/conf-parse.y $(MODULE_DIR)/general.h \
                            $(MODULE_DIR)/data.h
$(MODULE_DIR)/conf-parse.h: $(MODULE_DIR)/conf-parse.c

# Include all dependency files
INCLUDEFILES += $(LIBSHOBJECTS:.lo=.ld) $(LIBSTOBJECTS:.ao=.ad)

# Special warning prevention for flex-generated files
FLEXNOWARN:=-Wno-shadow -Wno-undef -Wno-unused -Wno-missing-prototypes -Wno-sign-compare
$(MODULE_DIR)/conf-lex.ao: $(MODULE_DIR)/conf-lex.c
	$(CC) $(ARCPPFLAGS) $(ARCFLAGS) $(FLEXNOWARN) -c $< -o $@
$(MODULE_DIR)/conf-lex.lo: $(MODULE_DIR)/conf-lex.c
	$(CC) $(LIBCPPFLAGS) $(LIBCFLAGS) $(FLEXNOWARN) -c $< -o $@

# Special warning prevention for bison-generated files
YACCNOWARN:=-Wno-undef
$(MODULE_DIR)/conf-parse.ao: $(MODULE_DIR)/conf-parse.c
	$(CC) $(ARCPPFLAGS) $(ARCFLAGS) $(YACCNOWARN) -c $< -o $@
$(MODULE_DIR)/conf-parse.lo: $(MODULE_DIR)/conf-parse.c
	$(CC) $(LIBCPPFLAGS) $(LIBCFLAGS) $(YACCNOWARN) -c $< -o $@

REMOVELIBST := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBDIR)/%,$(LIB_DIR)/$(LIBSTLIBNAME))
REMOVELIBSH := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBDIR)/%,$(LIB_DIR)/$(LIBSHLIBNAME))
REMOVELNSO  := $(DESTDIR)$(LIBDIR)/$(LIBSHSONAME)
REMOVELNBS  := $(DESTDIR)$(LIBDIR)/$(LIBSHBASENAME)
REMOVELIBHF := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBINCLUDEDIR)/%,$(LIBHEADERFILES))
REMOVEMAN3  := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBMAN3DIR)/%,$(LIBMAN3FILES))
REMOVEMAN5  := $(patsubst $(MODULE_DIR)/%,$(DESTDIR)$(LIBMAN5DIR)/%,$(LIBMAN5FILES))

all-lib: $(LIBTARGETS)
user :: all-lib

# Generate warnings if the install directory isn't in /etc/ld.so.conf
# or if the library wasn't there before (which means ldconfig must be run).
# Note that some ld.so's put /usr/lib and /lib first, others put them last,
# so we can't make any assumptions.
install-lib: all-lib
	$(MKDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(LIBINCLUDEDIR) $(DESTDIR)$(LIBMAN3DIR) $(DESTDIR)$(LIBMAN5DIR)
	@if [ -z "$(DESTDIR)" -a ! -e "$(LIBDIR)/$(LIBSHSONAME)" ] ; then \
	     echo '******************************************************************************' ; \
	     echo 'Warning: This is the first installation of the $(LIBSHSONAME)*' ; \
	     echo '         library files in $(LIBDIR)!' ; \
	     echo '         You must update the library cache or the userspace tools may fail' ; \
	     echo '         or have unpredictable results!' ; \
	     echo '         Run the following command: /sbin/ldconfig' ; \
	     echo '******************************************************************************' ; \
	fi
ifeq ($(BUILD_STATIC_LIB),1)
	$(INSTALL) -m 644 $(LIB_DIR)/$(LIBSTLIBNAME) $(DESTDIR)$(LIBDIR)
endif
	$(INSTALL) -m 755 $(LIB_DIR)/$(LIBSHLIBNAME) $(DESTDIR)$(LIBDIR)
	$(LN) $(LIBSHLIBNAME) $(DESTDIR)$(LIBDIR)/$(LIBSHSONAME)
	$(LN) $(LIBSHSONAME) $(DESTDIR)$(LIBDIR)/$(LIBSHBASENAME)
	@if [ -z "$(DESTDIR)" -a "$(LIBDIR)" != "/usr/lib" -a "$(LIBDIR)" != "/lib" ] ; then \
	   if [ -e "/usr/lib/$(LIBSHSONAME)" -o -e "/usr/lib/$(LIBSHBASENAME)" ] ; then \
	     echo '******************************************************************************' ; \
	     echo 'Warning: You have at least one $(LIBSHBASENAME) library file in /usr/lib' ; \
	     echo '         and the new library files are in $(LIBDIR)!' ; \
	     echo '         These old files must be removed or the userspace tools may fail' ; \
	     echo '         or have unpredictable results!' ; \
	     echo '         Run the following command: rm /usr/lib/$(LIBSHBASENAME)*' ; \
	     echo '******************************************************************************' ; \
	   fi ; \
	   cat /etc/ld.so.conf /etc/ld.so.conf.d/*.conf 2>/dev/null | grep -q '\(^\|[[:space:]:,]\)$(LIBDIR)\([[:space:]:,=]\|$$\)' || \
		( echo '******************************************************************************' ; \
		  echo 'Warning: Library directory $(LIBDIR) is not in /etc/ld.so.conf!' ; \
		  echo '         Add it and run /sbin/ldconfig for the userspace tools to work.' ; \
		  echo '******************************************************************************' ) ; \
	fi
	$(INSTALL) -m 644 $(LIBHEADERFILES) $(DESTDIR)$(LIBINCLUDEDIR)
	$(INSTALL) -m 644 $(LIBMAN3FILES) $(DESTDIR)$(LIBMAN3DIR)
	$(INSTALL) -m 644 $(LIBMAN5FILES) $(DESTDIR)$(LIBMAN5DIR)
	$(LN) sensors.conf.5 $(DESTDIR)$(LIBMAN5DIR)/sensors3.conf.5


user_install :: install-lib

user_uninstall::
	$(RM) $(REMOVELIBSH) $(REMOVELNSO) $(REMOVELNBS)
ifeq ($(BUILD_STATIC_LIB),1)
	$(RM) $(REMOVELIBST)
endif
	$(RM) $(REMOVELIBHF) $(REMOVEMAN3) $(REMOVEMAN5)
# Remove directory if empty, ignore failure
	$(RMDIR) $(DESTDIR)$(LIBINCLUDEDIR) 2> /dev/null || true

clean-lib:
	$(RM) $(LIB_DIR)/*.ld $(LIB_DIR)/*.ad
	$(RM) $(LIB_DIR)/*.lo $(LIB_DIR)/*.ao
	$(RM) $(LIBTARGETS) $(LIBEXTRACLEAN)
# old versions
	$(RM) $(LIB_DIR)/$(LIBSHBASENAME).*
clean :: clean-lib
