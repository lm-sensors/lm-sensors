#################################################
# config
KERNEL_LOCATION=/usr/src/linux
CURRENT=$(shell uname -r)

#################################################
# some magic for using linux kernel settings
# when compiling module(s)

M_OBJS       = eeprom.o hw-monitor.c
MX_OBJS      = 
EXTRA_CFLAGS = -DVERIFY_HEADERS -DSLOW_STARTER -I..

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	su -c "cp -v $(M_OBJS) $(MX_OBJS) /lib/modules/$(CURRENT)/misc"

clean:
	-rm -f $(M_OBJS) $(MX_OBJS) .*.o.flags *~

include $(KERNEL_LOCATION)/Rules.make
