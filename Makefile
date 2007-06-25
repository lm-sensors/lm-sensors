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

# You need a full complement of GNU utilities to run this Makefile
# successfully; most notably, you need bash, GNU make, flex (>= 2.5.1)
# and bison.

# If your /bin/sh is not bash, change the below definition so that make can
# find bash. Or you can hope your sh-like shell understands all scripts.
# I think so, but I have not tested it.
#SHELL := /usr/bin/bash

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
SRCDIRS += kernel/include
SRCDIRS += lib prog/detect prog/dump prog/eeprom prog/pwm \
           prog/sensors prog/xeon ${PROG_EXTRA:%=prog/%} etc
SRCDIRS += lib/test

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
# PROGCPPFLAGS/PROGCFLAGS is to create regular object files (which are linked into executables).
# ARCPPFLAGS/ARCFLAGS are used to create archive object files (static libraries).
# LIBCPPFLAGS/LIBCFLAGS are for shared library objects.
ALL_CPPFLAGS := -I. -Ikernel/include
ALL_CFLAGS := -Wall

ifeq ($(DEBUG),1)
ALL_CPPFLAGS += -DDEBUG
ALL_CFLAGS += -O -g
else
ALL_CFLAGS += -O2
endif

ifeq ($(WARN),1)
ALL_CFLAGS += -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
            -Wcast-align -Wwrite-strings -Wnested-externs -Winline
endif

ALL_CPPFLAGS += $(CPPFLAGS)
ALL_CFLAGS += $(CFLAGS)

PROGCPPFLAGS := -DETCDIR="\"$(ETCDIR)\"" $(ALL_CPPFLAGS) -Wundef
PROGCFLAGS := $(ALL_CFLAGS)
ARCPPFLAGS := $(ALL_CPPFLAGS)
ARCFLAGS := $(ALL_CFLAGS)
LIBCPPFLAGS := $(ALL_CPPFLAGS)
LIBCFLAGS := -fpic -D_REENTRANT $(ALL_CFLAGS)

.PHONY: all user clean install user_install uninstall user_uninstall version package

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

clean::
	$(RM) lm_sensors-* lex.backup

user_uninstall::

uninstall :: user_uninstall

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

help:
	@echo 'Make targets are:'
	@echo '  all (default): build library and userspace programs'
	@echo '  install: install library and userspace programs'
	@echo '  uninstall: uninstall library and userspace programs'
	@echo '  clean: cleanup'
	@echo '  package: create a distribution package'

# Generate html man pages to be copied to the lm_sensors website.
# This uses the man2html from here
# http://ftp.math.utah.edu/pub/sgml/
# which works directly from the nroff source
manhtml:
	$(MKDIR) html
	cp $(MANPAGES) html
	cd html ; \
	export LOGNAME=sensors ; \
	export HOSTNAME=www.lm-sensors.org ; \
	man2html *.[1-8] ; \
	$(RM) *.[1-8]

# Here, we define all implicit rules we want to use.

.SUFFIXES:

# We need to create dependency files. Tricky. The sed rule puts dir/file.d and
# dir/file.c in front of the dependency file rule.


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

ifeq ($(DEBUG),1)
FLEX_FLAGS := -Psensors_yy -t -b -Cfe -8
else
FLEX_FLAGS := -Psensors_yy -t -Cfe -8
endif

%.c: %.l
	$(FLEX) $(FLEX_FLAGS) $< > $@
