#  Makefile - Makefile for a Linux module for reading sensor data.
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

# Everything you may want to change is in the top of this file. Usually, you
# can just use the defaults, fortunately.

# You need a full complement of GNU utilities to run this Makefile succesfully;
# most notably, you need bash, GNU make, flex (>= 2.5.1) and bison.

# If your /bin/sh is not bash, change the below definition so that make can
# find bash. Or you can hope your sh-like shell understands all scripts.
# I think so, but I have not tested it.
#SHELL := /usr/bin/bash

# The currently running kernel version. This is used right below to
# determine where the kernel sources can be found.
KERNELVERSION := $(shell uname -r)

# The location of linux itself. This is used to find the kernel headers
# and other things.
#LINUX := /usr/src/linux
LINUX := $(shell if [ -L "/lib/modules/$(KERNELVERSION)/build" ] ; \
	then echo "/lib/modules/$(KERNELVERSION)/build" ; \
	else echo "/usr/src/linux" ; fi)
LINUX_HEADERS := $(LINUX)/include

# If you have installed the i2c header at some other place (like 
# /usr/local/include/linux), set that directory here. Please check this out
# if you get strange compilation errors; the default Linux i2c headers
# may be used mistakenly. Note: This should point to the directory
# *above* the linux/ subdirectory, so to /usr/local/include in the
# above example.
I2C_HEADERS := /usr/local/include
#I2C_HEADERS := $(LINUX_HEADERS)

# Uncomment the third line on SMP systems if the magic invocation fails.
SMP := $(shell if grep -q '^[[:space:]]*\#define[[:space:]]*CONFIG_SMP[[:space:]]*1' $(LINUX_HEADERS)/linux/autoconf.h ; \
               then echo 1; else echo 0; fi)
#SMP := 0
#SMP := 1

# Uncomment the second or third line if the magic invocation fails.
# We need to know whether CONFIG_MODVERSIONS is defined.
MODVER := $(shell if grep -q '^[[:space:]]*\#define[[:space:]]*CONFIG_MODVERSIONS[[:space:]]*1' $(LINUX_HEADERS)/linux/autoconf.h ; \
                  then echo 1; else echo 0; fi)
#MODVER := 0
#MODVER := 1

# Uncomment the second line if you are a developer. This will enable many
# additional warnings at compile-time
WARN := 0
#WARN := 1

# Uncomment the second line if you want to get (loads of) debug information
# at run-time.
# Not recommended, unless you are actually debugging the code
DEBUG := 0
#DEBUG := 1

# If you want to install at some other place then at from which you will run
# everything, set DESTDIR to the extra prefix.
DESTDIR :=

# This is the prefix that will be used for almost all directories below.
PREFIX := /usr/local

# Your C compiler
CC := gcc

# This is the main modules directory into which the modules will be installed.
# The magic invocation will return something like this:
#   /lib/modules/2.4.29
MODPREF := /lib/modules/$(shell $(CC) -I$(LINUX_HEADERS) -E etc/config.c | grep uts_release |cut -f 2 -d'"')

# This is the directory where sensors.conf will be installed, if no other
# configuration file is found
ETCDIR := /etc

# You should not need to change this. It is the directory into which the
# library files (both static and shared) will be installed.
LIBDIR := $(PREFIX)/lib

EXLDFLAGS := -Wl,-rpath,$(LIBDIR)

# You should not need to change this. It is the directory into which the
# executable program files will be installed. BINDIR for programs that are
# also useful for normal users, SBINDIR for programs that can only be run
# by the superuser.
# Note that not all programs in this package are really installed;
# some are just examples. You can always install them by hand, of
# course.
BINDIR := $(PREFIX)/bin
SBINDIR := $(PREFIX)/sbin

# You should not need to change this. It is the basic directory into which
# include files will be installed. The actual directory will be 
# $(INCLUDEDIR)/linux for system include files, and $(INCLUDEDIR)/sensors
# for library include files. If PREFIX equals the default /usr/local/bin,
# you will be able to use '#include <linux/sensors.h>' regardless of the
# current kernel selected.
INCLUDEDIR := $(PREFIX)/include
SYSINCLUDEDIR := $(INCLUDEDIR)/linux
LIBINCLUDEDIR := $(INCLUDEDIR)/sensors

# You should not need to change this. It is the base directory under which the
# manual pages will be installed.
MANDIR := $(PREFIX)/man

MACHINE := $(shell uname -m)

# Extra non-default programs to build; e.g., sensord
# PROG_EXTRA := sensord

# Set these to add preprocessor or compiler flags, or use
# environment variables
# CFLAGS :=
# CPPFLAGS :=

##################################################
# Below this, nothing should need to be changed. #
##################################################

# Note that this is a monolithic Makefile; it calls no sub-Makefiles,
# but instead, it compiles everything right from here. Yes, there are
# some distinct advantages to this; see the following paper for more info:
#   http://www.tip.net.au/~millerp/rmch/recu-make-cons-harm.html
# Note that is still uses Makefile fragments in sub-directories; these
# are called 'Module.mk'.

# Within each Module.mk, rules and dependencies can be added to targets
# all, install and clean. Use double colons instead of single ones
# to do this. 

# The subdirectories we need to build things in 
SRCDIRS :=
ifneq ($(MAKECMDGOALS),user)
ifneq ($(MAKECMDGOALS),user_install)
ifneq ($(MAKECMDGOALS),user_uninstall)
ifneq ($(MAKECMDGOALS),package)
ifneq ($(MAKECMDGOALS),userpackage)
ifneq ($(MAKECMDGOALS),manhtml)
SRCDIRS += mkpatch
SRCDIRS += kernel kernel/busses kernel/chips
endif
endif
endif
endif
endif
endif
SRCDIRS += kernel/include
SRCDIRS += lib prog/detect prog/dump prog/eeprom prog/pwm \
           prog/sensors prog/xeon ${PROG_EXTRA:%=prog/%} etc

# Some often-used commands with default options
MKDIR := mkdir -p
RMDIR := rmdir
RM := rm -f
BISON := bison
FLEX := flex
AR := ar
INSTALL := install
LN := ln -sf
GREP := grep
AWK := awk
SED := sed

# Determine the default compiler flags
# Set CFLAGS or CPPFLAGS above to add your own flags to all.
# ALLCPPFLAGS/ALLCFLAGS are common flags, plus any user-specified overrides from the environment or make command line.
# MODCPPFLAGS/MODCFLAGS is to create in-kernel object files (modules).
# PROGCPPFLAGS/PROGCFLAGS is to create non-kernel object files (which are linked into executables).
# ARCPPFLAGS/ARCFLAGS are used to create archive object files (static libraries).
# LIBCPPFLAGS/LIBCFLAGS are for shared library objects.
ALL_CPPFLAGS := -I. -Ikernel/include -I$(I2C_HEADERS)
ALL_CFLAGS := -Wall

ifeq ($(DEBUG),1)
ALL_CPPFLAGS += -DDEBUG
ALL_CFLAGS += -O -g
else
ALL_CFLAGS += -O2
endif

ifeq ($(WARN),1)
ALL_CFLAGS += -W -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
            -Wcast-align -Wwrite-strings -Wnested-externs -Winline
endif

ALL_CPPFLAGS += $(CPPFLAGS)
ALL_CFLAGS += $(CFLAGS)

MODCPPFLAGS :=
MODCFLAGS := -fno-strict-aliasing

ifeq ($(MACHINE),alpha)
MODCFLAGS += -ffixed-8 -mno-fp-regs -mcpu=ev56
endif

ifeq ($(MACHINE),x86_64)
MODCFLAGS += -fno-common -fomit-frame-pointer -mno-red-zone \
	     -mcmodel=kernel -fno-reorder-blocks -finline-limit=2000 -fno-strength-reduce
endif

ifeq ($(MACHINE),mips)
MODCFLAGS += -mabi=32 -mips3 -Wa,-32 -Wa,-mips3 -Wa,--trap
endif

ifeq ($(MACHINE),sparc32)
MODCFLAGS += -m32 -pipe -mno-fpu -fcall-used-g5 -fcall-used-g7
endif

ifeq ($(MACHINE),sparc64)
MODCFLAGS += -m64 -pipe -mno-fpu -mcpu=ultrasparc -mcmodel=medlow \
	     -ffixed-g4 -fcall-used-g5 -fcall-used-g7 -Wno-sign-compare \
	     -Wa,--undeclared-regs
endif

ifeq ($(SMP),1)
MODCPPFLAGS += -D__SMP__
endif

ifeq ($(MODVER),1)
MODCPPFLAGS += -DMODVERSIONS -include $(LINUX_HEADERS)/linux/modversions.h
endif

# This magic is from the kernel Makefile.
# Extra cflags for kbuild 2.4.  The default is to forbid includes by kernel code
# from user space headers.
kbuild_2_4_nostdinc := -nostdinc $(shell LC_ALL=C $(CC) -print-search-dirs | sed -ne 's/install: \(.*\)/-I \1include/gp')

MODCPPFLAGS += -D__KERNEL__ -DMODULE -DEXPORT_SYMTAB -fomit-frame-pointer $(ALL_CPPFLAGS) -I$(LINUX_HEADERS) $(kbuild_2_4_nostdinc)
MODCFLAGS += $(ALL_CFLAGS)
PROGCPPFLAGS := -DETCDIR="\"$(ETCDIR)\"" $(ALL_CPPFLAGS)
PROGCFLAGS := $(ALL_CFLAGS)
ARCPPFLAGS := $(ALL_CPPFLAGS)
ARCFLAGS := $(ALL_CFLAGS)
LIBCPPFLAGS := $(ALL_CPPFLAGS)
LIBCFLAGS := -fpic $(ALL_CFLAGS)

.PHONY: all clean install version package dep

# Make all the default rule
all::

# Include all makefiles for sub-modules
INCLUDEFILES := 
include $(patsubst %,%/Module.mk,$(SRCDIRS))
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),uninstall)
ifneq ($(MAKECMDGOALS),user_uninstall)
ifneq ($(MAKECMDGOALS),help)
ifneq ($(MAKECMDGOALS),package)
ifneq ($(MAKECMDGOALS),userpackage)
include $(INCLUDEFILES)
endif
endif
endif
endif
endif
endif

# Man pages
MANPAGES := $(LIBMAN3FILES) $(LIBMAN5FILES) $(PROGDETECTMAN8FILES) $(PROGDUMPMAN8FILES) \
            $(PROGSENSORSMAN1FILES) $(PROGPWMMAN8FILES) prog/sensord/sensord.8

# Making the dependency files - done automatically!
dep : 

user ::
user_install::
	@echo "*** Important note:"
	@echo "***  * The libsensors configuration file ($(ETCDIR)/sensors.conf) is never"
	@echo "***    overwritten by our installation process, so that you won't lose"
	@echo "***    your personal settings in that file. You still can get our latest"
	@echo "***    default config file in etc/sensors.conf.eg and manually copy it to"
	@echo "***    $(ETCDIR)/sensors.conf if you want. You will then want to edit it"
	@echo "***    to fit your needs again."
all :: user
install :: all user_install
ifeq ($(DESTDIR),)
	-/sbin/depmod -a
else
	@echo "*** This is a \`staged' install using \"$(DESTDIR)\" as prefix."
	@echo "***"
	@echo "*** Once the modules have been moved to their final destination"
	@echo "*** you must run the command \"/sbin/depmod -a\"."
	@echo "***"
	@echo "*** Alternatively, if you build a package (e.g. rpm), include the"
	@echo "*** command \"/sbin/depmod -a\" in the post-(un)install procedure."
	@echo "***"
	@echo "*** The depmod command mentioned above may generate errors. We are"
	@echo "*** aware of the problem and are working on a solution."
endif

clean::
	$(RM) lm_sensors-*

user_uninstall::
	
uninstall :: user_uninstall
	@echo "*** Note:"
	@echo "***  * Kernel modules were not uninstalled."

# This is tricky, but it works like a charm. It needs lots of utilities
# though: cut, find, gzip, ln, tail and tar.
package: version clean
	lmversion=`tail -1 version.h|cut -f 2 -d \"`; \
	lmpackage=lm_sensors-$$lmversion; \
	ln -s . $$lmpackage;  \
	find $$lmpackage/ -type f | grep -v ^$$lmpackage/$$lmpackage$$ | \
	                            grep -v ^$$lmpackage/$$lmpackage.tar$$ | \
	                            grep -v ^$$lmpackage/$$ | \
	                            grep -v /CVS | \
	                            grep -v /\\.# | \
	                            tar rvf $$lmpackage.tar -T -; \
        gzip -9 $$lmpackage.tar ;\
        $(RM) $$lmpackage.tar $$lmpackage
	cat doc/developers/checklist

# doesn't work well yet... needs Makefile changes too
userpackage: version clean $(KERNELINCLUDEDIR)/sensors.h
	lmversion=`tail -1 version.h|cut -f 2 -d \"`; \
	lmpackage=lm_sensors-user-$$lmversion; \
	ln -s . $$lmpackage;  \
	find $$lmpackage/ -type f | grep -v ^$$lmpackage/$$lmpackage$$ | \
	                            grep -v ^$$lmpackage/$$lmpackage.tar$$ | \
	                            grep -v ^$$lmpackage/doc/chips | \
	                            grep -v ^$$lmpackage/doc/busses | \
	                            grep -v ^$$lmpackage/kernel/chips | \
	                            grep -v ^$$lmpackage/kernel/busses | \
	                            grep -v ^$$lmpackage/$$ | \
	                            grep -v /CVS | \
	                            grep -v /\\.# | \
	                            tar rvf $$lmpackage.tar -T -; \
        gzip -9 $$lmpackage.tar ;\
        $(RM) $$lmpackage.tar $$lmpackage
	cat doc/developers/checklist

version:
	$(RM) version.h
	echo '#define LM_DATE "'`date +'%Y%m%d'`\" > version.h
	echo -n 'Version: '; \
	echo '#define LM_VERSION "'`read VER; echo $$VER`\" >> version.h

help:
	@echo 'Make targets are:'
	@echo '  all (default): build modules and userspace programs'
	@echo '  install: install modules and userspace programs'
	@echo '  user: build userspace programs'
	@echo '  user_install: install userspace programs'
	@echo '  user_uninstall: remove userspace programs'
	@echo '  clean: cleanup'
	@echo '  package: create a distribution package'
	@echo 'Note: make dep is automatic'

$(LINUX)/.config:
	@echo
	@echo "Error - missing file $(LINUX)/.config !! "
	@echo "  Verify kernel source is in $(LINUX) and then"
	@echo "  cd to $(LINUX) and run 'make config' !!"
	@echo
	@echo "Exception: if you're using a stock RedHat kernel..."
	@echo "  (1) Install the appropriate kernel-source RPM."
	@echo "  (2) Copy the appropriate config..."
	@echo "      from $(LINUX)/configs/<...>"
	@echo "      to $(LINUX)/.config"
	@echo "  (3) Do *NOT* 'make dep' or 'make config'."
	@echo
	@exit 1

# Generate html man pages to be copied to the lm_sensors website.
# This uses the man2html from here
# http://ftp.math.utah.edu/pub/sgml/
# which works directly from the nroff source
manhtml:
	$(MKDIR) html
	cp $(MANPAGES) html
	cd html ; \
	export LOGNAME=sensors ; \
	export HOSTNAME=stimpy.netroedge.com ; \
	man2html *.[1-8] ; \
	$(RM) *.[1-8]

# Here, we define all implicit rules we want to use.

.SUFFIXES:

# We need to create dependency files. Tricky. We sed rule puts dir/file.d and
# dir/file.c in front of the dependency file rule.

# .o files are used for modules
# depend on the kernel config file!
%.o: %.c $(LINUX)/.config
	$(CC) $(MODCPPFLAGS) $(MODCFLAGS) -c $< -o $@

%.d: %.c $(LINUX)/.config
	$(CC) -M -MG $(MODCPPFLAGS) $(MODCFLAGS) $< | \
       	$(SED) -e 's@^\(.*\)\.o:@$*.d $*.o: Makefile '`dirname $*.d`/Module.mk' @' > $@



# .ro files are used for programs (as opposed to modules)
%.ro: %.c
	$(CC) $(PROGCPPFLAGS) $(PROGCFLAGS) -c $< -o $@

%.rd: %.c
	$(CC) -M -MG $(PROGCPPFLAGS) $(PROGCFLAGS) $< | \
       	$(SED) -e 's@^\(.*\)\.o:@$*.rd $*.ro: Makefile '`dirname $*.rd`/Module.mk' @' > $@


%: %.ro
	$(CC) $(EXLDFLAGS) -o $@ $^


# .ao files are used for static archives
%.ao: %.c
	$(CC) $(ARCPPFLAGS) $(ARCFLAGS) -c $< -o $@

%.ad: %.c
	$(CC) -M -MG $(ARCPPFLAGS) $(ARCFLAGS) $< | \
       	$(SED) -e 's@^\(.*\)\.o:@$*.ad $*.ao: Makefile '`dirname $*.ad`/Module.mk' @' > $@


# .lo files are used for shared libraries
%.lo: %.c
	$(CC) $(LIBCPPFLAGS) $(LIBCFLAGS) -c $< -o $@

%.ld: %.c
	$(CC) -M -MG $(LIBCPPFLAGS) $(LIBCFLAGS) $< | \
       	$(SED) -e 's@^\(.*\)\.o:@$*.ld $*.lo: Makefile '`dirname $*.ld`/Module.mk' @' > $@


# Flex and Bison
%c: %y
	$(BISON) -p sensors_yy -d $< -o $@

%.c: %.l
	$(FLEX) -Psensors_yy -t $< > $@
