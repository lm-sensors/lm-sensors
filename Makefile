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
# SHELL=/usr/bin/bash

# The location of linux itself. This is used to find the kernel headers
# and other things.
LINUX=/usr/src/linux
LINUX_HEADERS=$(LINUX)/include

# Determine whether we need to compile the kernel modules, or only the
# user-space utilities. By default, the kernel modules are compiled.
#COMPILE_KERNEL := 0
COMPILE_KERNEL := 1

# If you have installed the i2c header at some other place (like 
# /usr/local/include/linux), set that directory here. Please check this out
# if you get strange compilation errors; the default Linux i2c headers
# may be used mistakenly. Note: This should point to the directory
# *above* the linux/ subdirectory, so to /usr/local/include in the
# above example.
I2C_HEADERS=/usr/local/include
#I2C_HEADERS=$(LINUX_HEADERS)

# Uncomment the third line on SMP systems if the magic invocation fails. It
# is a bit complicated because SMP configuration changed around kernel 2.1.130
SMP := $(shell if grep -q '^SMP[[:space:]]*=' $(LINUX)/Makefile || \
                  grep -q '^[[:space:]]*\#define[[:space:]]*CONFIG_SMP[[:space:]]*1' $(LINUX_HEADERS)/linux/autoconf.h ; \
               then echo 1; else echo 0; fi)
#SMP := 0
#SMP := 1

# Uncomment the second or third line if the magic invocation fails.
# We need to know whether CONFIG_MODVERSIONS is defined.
MODVER := $(shell if cat $(LINUX_HEADERS)/linux/config.h $(LINUX_HEADERS)/linux/autoconf.h 2>/dev/null | grep -q '^[[:space:]]*\#define[[:space:]]*CONFIG_MODVERSIONS[[:space:]]*1'; then echo 1; else echo 0; fi)
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

# This is the directory into which the modules will be installed.
# The magic invocation will return something like this:
#   /lib/modules/2.2.15-ac9/misc
MODDIR := /lib/modules/`grep UTS_RELEASE $(LINUX_HEADERS)/linux/version.h|cut -f 2 -d'"'`/misc

# This is the directory where sensors.conf will be installed, if no other
# configuration file is found
ETCDIR := /etc

# You should not need to change this. It is the directory into which the
# library files (both static and shared) will be installed.
LIBDIR := $(PREFIX)/lib

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

# You should not need to change this. It defines the manual owner and group
# as which manual pages are installed.
MANOWN := root
MANGRP := root

MACHINE := $(shell uname -m)

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
SRCDIRS := mkpatch
ifeq ($(COMPILE_KERNEL),1)
SRCDIRS += kernel kernel/busses kernel/chips kernel/include
endif
SRCDIRS += lib prog/sensors prog/sensord prog/dump prog/detect etc

# Some often-used commands with default options
MKDIR := mkdir -p
RM := rm -f
CC := gcc
BISON := bison
FLEX := flex
AR := ar
INSTALL := install
LN := ln -sfn
GREP := grep

# Determine the default compiler flags
# MODCFLAGS is to create in-kernel object files (modules); PROGFLAGS is to
# create non-kernel object files (which are linked into executables).
# ARCFLAGS are used to create archive object files (static libraries), and
# LIBCFLAGS are for shared library objects.
CFLAGS := -I. -Ikernel/include -I$(I2C_HEADERS) -I$(LINUX_HEADERS) -O2 

ifeq ($(DEBUG),1)
CFLAGS += -DDEBUG
endif

ifeq ($(WARN),1)
CFLAGS += -Wall -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
          -Wcast-align -Wwrite-strings -Wnested-externs -Winline
endif

MODCFLAGS := $(CFLAGS) -D__KERNEL__ -DMODULE -fomit-frame-pointer 
MODCFLAGS := $(MODCFLAGS) -DEXPORT_SYMTAB
PROGCFLAGS := $(CFLAGS)
ARCFLAGS := $(CFLAGS)
LIBCFLAGS := $(CFLAGS) -fpic

ifeq ($(MACHINE),alpha)
MODCFLAGS += -ffixed-8
endif

ifeq ($(SMP),1)
MODCFLAGS += -D__SMP__
endif

ifeq ($(MODVER),1)
MODCFLAGS += -DMODVERSIONS -include $(LINUX_HEADERS)/linux/modversions.h
endif

.PHONY: all clean install version package dep

# Make all the default rule
all::

# Include all makefiles for sub-modules
INCLUDEFILES := 
include $(patsubst %,%/Module.mk,$(SRCDIRS))
ifneq ($(MAKECMDGOALS),clean)
include $(INCLUDEFILES)
endif

# Making the dependency files - done automatically!
dep : 

all ::

install :: all

clean::
	$(RM) lm_sensors-*

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

version:
	$(RM) version.h
	echo '#define LM_DATE "'`date +'%Y%m%d'`\" > version.h
	echo -n 'Version: '; \
	echo '#define LM_VERSION "'`read VER; echo $$VER`\" >> version.h


# Here, we define all implicit rules we want to use.

.SUFFIXES:

# We need to create dependency files. Tricky. We sed rule puts dir/file.d and
# dir/file.c in front of the dependency file rule.

# .o files are used for modules
%.o: %.c
	$(CC) $(MODCFLAGS) -c $< -o $@

%.d: %.c
	$(CC) -M -MG $(MODCFLAGS) $< | \
       	sed -e 's@^\(.*\)\.o:@$*.d $*.o: Makefile '`dirname $*.d`/Module.mk' @' > $@



# .ro files are used for programs (as opposed to modules)
%.ro: %.c
	$(CC) $(PROGCFLAGS) -c $< -o $@

%.rd: %.c
	$(CC) -M -MG $(PROGCFLAGS) $< | \
       	sed -e 's@^\(.*\)\.o:@$*.rd $*.ro: Makefile '`dirname $*.rd`/Module.mk' @' > $@


%: %.ro
	$(CC) $(EXLDFLAGS) -o $@ $^


# .ao files are used for static archives
%.ao: %.c
	$(CC) $(ARCFLAGS) -c $< -o $@

%.ad: %.c
	$(CC) -M -MG $(ARCFLAGS) $< | \
       	sed -e 's@^\(.*\)\.o:@$*.ad $*.ao: Makefile '`dirname $*.ad`/Module.mk' @' > $@


# .lo files are used for shared libraries
%.lo: %.c
	$(CC) $(LIBCFLAGS) -c $< -o $@

%.ld: %.c
	$(CC) -M -MG $(LIBCFLAGS) $< | \
       	sed -e 's@^\(.*\)\.o:@$*.ld $*.lo: Makefile '`dirname $*.ld`/Module.mk' @' > $@


# Flex and Bison
%c: %y
	$(BISON) -p sensors_yy -d $< -o $@

%.c: %.l
	$(FLEX) -Psensors_yy -t $< > $@
