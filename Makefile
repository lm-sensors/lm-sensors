# Uncomment the second line on SMP systems if the magic invocation fails.
#SMP := 0
#SMP := 1
SMP := $(shell if grep -q '^SMP[[:space:]]*=' /usr/src/linux/Makefile; then echo 1; else echo 0; fi)

# Uncomment the second line if you do not want to compile the included
# i2c modules. WARNING! If the i2c module version does not match the 
# smbus/sensor module versions, you will get into severe problems.
I2C := 1
#I2C := 0

# Uncomment the second line if you are a developer. This will enable many
# additional warnings at compile-time
WARN := 0
#WARN := 1

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
MODULES :=
ifeq ($(I2C),1)
MODULES += i2c
endif

# Some often-used commands with default options
MKDIR := mkdir -p
RM := rm -f

# Determine the default compiler flags
CFLAGS := -D__KERNEL__ -DMODULE -I. -O2 -fomit-frame-pointer

ifeq ($(SMP),1)
CFLAGS += -D__SMP__
endif

ifeq ($(WARN),1)
CFLAGS += -Wall -Wstrict-prototypes -Wshadow -Wpointer-arith -Wcast-qual \
          -Wcast-align -Wwrite-strings -Wnested-externs -Winline
endif

.PHONY: all clean install version package

# Include all makefiles for sub-modules
include $(patsubst %,%/Module.mk,$(MODULES))


all::

install::

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
