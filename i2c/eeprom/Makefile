# $Id: Makefile,v 1.1 1996/07/23 20:31:56 i2c Exp $
# $Log: Makefile,v $
# Revision 1.1  1996/07/23 20:31:56  i2c
# Initial revision
#

I2C_HOME	= ..

#----------------------------------------------------------------------------
I2C_OPT         = -DVERIFY_HEADERS -DSLOW_STARTER
WARNINGS        = -Wall -Wstrict-prototypes
COMPILE         = -pipe -fomit-frame-pointer -m486 -O2 \
		  -I$(I2C_HOME) -I.
DEBUG           = 

SRCS  	= i2c_main.c adap-illeg.c adap-phil.c adap-lppcf.c adap-isapcf.c \
#		adap-lpbit.c
OBJS  	= $(SRCS:.c=.o)
HDRS	= i2c_priv.h
MAIN   	= eeprom

CFLAGS = $(DEBUG) $(WARNINGS) $(COMPILE) $(I2C_OPT) $(KERNEL_OPT)
SFLAGS = -fverbose-asm $(WARNINGS) $(COMPILE) $(PRT_OPT) $(KERNEL_OPT)

all: $(MAIN)

install: $(MAIN)
	-@mkdir $(I2C_HOME)/bin
	cp $(MAIN) $(I2C_HOME)/bin

clean:
	-rm -f a.out $(OBJS) $(MAIN) core .depend detect 

dep depend .depend: $(HDRS) $(SRCS)
	gcc $(CFLAGS) $(SRCS) -M >.depend
#
# include dependency files if they exist
#
#ifeq (.depend,$(wildcard .depend))
#include .depend
#endif
