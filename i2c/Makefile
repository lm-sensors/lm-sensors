#################################################
# config
KERNEL_LOCATION=/usr/src/linux
CURRENT=$(shell uname -r)

#################################################
# some magic for using linux kernel settings
# when compiling module(s)

MOD_SUB_DIRS = drivers
#MX_OBJS       = i2c-core.o algo-bit.o i2c-dev.o bit-lp.o bit-velle.o algo-pcf.o
MX_OBJS       = i2c-core.o algo-bit.o i2c-dev.o

M_OBJS      = bit-lp.o bit-velle.o
EXTRA_CFLAGS = -Wno-unused

here:
	DIR=`pwd`; (cd $(KERNEL_LOCATION); make SUBDIRS=$$DIR modules)

install:
	su -c "cp -v $(M_OBJS) $(MX_OBJS) /lib/modules/$(CURRENT)/misc"

clean:
	-rm -f $(M_OBJS) $(MX_OBJS) .*.o.flags *~

up:
	insmod i2c-core.o i2c_debug=0
	insmod algo-bit.o bit_test=1  i2c_debug=1
	insmod bit-lp.o base=0x278
	insmod i2c-dev.o
	insmod drivers/eeprom.o test=1

down:
	-rmmod eeprom
	-rmmod i2c-dev
	-rmmod bit-lp
	-rmmod algo-bit
	-rmmod i2c-core

include $(KERNEL_LOCATION)/Rules.make
