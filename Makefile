#  Makefile - Makefile for a Linux module for reading sensor data.
#  Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl>
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


# Uncomment the third line on SMP systems if the magic invocation fails.
SMP := $(shell if grep -q '^SMP[[:space:]]*=' /usr/src/linux/Makefile; then echo 1; else echo 0; fi)
#SMP := 0
#SMP := 1

# Uncomment the second or third line if the magic invocation fails.
# We need to know whether CONFIG_MODVERSIONS is defined.
MODVER := $(shell if cat /usr/include/linux/config.h /usr/include/linux/autoconf.h 2>/dev/null | grep -q '^[[:space:]]*\#define[[:space:]]*CONFIG_MODVERSIONS[[:space:]]*1'; then echo 1; else echo 0; fi)
#MODVER := 0
#MODVER := 1

# Uncomment the second line if you do not want to compile the included
# i2c modules. WARNING! If the i2c module version does not match the 
# smbus/sensor module versions, you will get into severe problems.
# If you want to use a self-compiled version of the i2c modules, make
# sure <linux/i2c.h> contains the *correct* i2c header file!
I2C := 1
#I2C := 0

# Uncomment the second line if you are a developer. This will enable many
# additional warnings at compile-time
WARN := 0
#WARN := 1

# Uncomment the second line if you want to get (loads of) debug information.
# Not recommended, unless you are actually debugging the code
DEBUG := 0
#DEBUG := 1

# This is the directory into which the modules will be installed, if you
# call 'make install'
MODDIR := /lib/modules/extra/misc

# This is the directory into which the include files will be installed,
# if you call 'make install'. If you install it in the directory below,
# #include <linux/smbus.h>, for example, should work, regardless of the
# current linux kernel.
INCLUDEDIR := /usr/local/include/linux


# Below this, nothing should need to be changed.

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
MODULES := src
ifeq ($(I2C),1)
MODULES += i2c i2c/detect i2c/drivers i2c/eeprom
endif

# Some often-used commands with default options
MKDIR := mkdir -p
RM := rm -f
CC := gcc

# Determine the default compiler flags
# CFLAGS is to create in-kernel object files (modules); EXCFLAGS is to create
# non-kernel object files (which are linked into executables).
CFLAGS := -D__KERNEL__ -DMODULE -I. -Ii2c -O2 -fomit-frame-pointer -DLM_SENSORS
EXCFLAGS := -I. -O2 -Ii2c -D LM_SENSORS

ifeq ($(DEBUG),1)
CFLAGS += -DDEBUG
EXCFLAGS += -DDEBUG
endif

ifeq ($(SMP),1)
CFLAGS += -D__SMP__
endif

ifeq ($(WARN),1)
CFLAGS += -Wall -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
          -Wcast-align -Wwrite-strings -Wnested-externs -Winline
EXCFLAGS += -Wall -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
            -Wcast-align -Wwrite-strings -Wnested-externs -Winline
endif

ifeq ($(MODVER),1)
CFLAGS += -DMODVERSIONS -include /usr/include/linux/modversions.h
endif

ifeq ($(I2C),1)
CFLAGS += -DI2C
endif

.PHONY: all clean install version package dep

# Make all the default rule
all::

# Include all makefiles for sub-modules
INCLUDEFILES := 
include $(patsubst %,%/Module.mk,$(MODULES))
ifneq ($(MAKECMDGOALS),clean)
include $(INCLUDEFILES)
endif

# Making the dependency files - done automatically!
dep : 

all ::

install :: all

clean::
	$(RM) lm_sensors-*

# Create a dependency file. Tricky. We sed rule puts dir/file.d and dir/file.c
# in front of the dependency file rule.
%.d: %.c
	$(CC) -M -MG $(CFLAGS) $< | \
       	sed -e 's@^\(.*\)\.o:@$*.d $*.o Makefile '`dirname $*.d`/Module.mk':@' > $@
	

# This is tricky, but it works like a charm. It needs lots of utilities
# though: cut, find, gzip, ln, tail and tar.
package: version clean
	lmversion=`tail -1 version.h|cut -f 2 -d \"`; \
	lmpackage=lm_sensors-$$lmversion; \
	ln -s . $$lmpackage;  \
	find $$lmpackage/ -type f | grep -v ^$$lmpackage/$$lmpackage$$ | \
	                            grep -v ^$$lmpackage/$$lmpackage.tar$$ | \
	                            grep -v ^$$lmpackage/$$ | \
	                            grep -v CVS | \
	                            grep -v ^$$lmpackage/.# | \
	                            tar rvf $$lmpackage.tar -T -; \
        gzip -9 $$lmpackage.tar ;\
        $(RM) $$lmpackage.tar $$lmpackage

version:
	$(RM) version.h
	echo '#define LM_DATE "'`date +'%Y%m%d'`\" > version.h
	echo -n 'Version: '; \
	echo '#define LM_VERSION "'`read VER; echo $$VER`\" >> version.h
