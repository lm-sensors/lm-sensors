# $Id: Makefile,v 1.1 1996/12/04 20:40:10 i2c Exp $
# $Log: Makefile,v $
# Revision 1.1  1996/12/04 20:40:10  i2c
# Initial revision
#
#


I2C_HOME	= ..

#----------------------------------------------------------------------------
I2C_OPT         = -DVERIFY_HEADERS -DSLOW_STARTER
KERNEL_OPT      = -D__KERNEL__ -DKERNEL -DLINUX -DMODULE 
WARNINGS        = -Wall -Wstrict-prototypes
COMPILE         = -pipe -fomit-frame-pointer -m486 -O2 \
		  -I$(I2C_HOME) -I.
DEBUG           =

SRCS  		= i2c_main.c alg-illeg.c $(HW_SRCS)
INCLUDES	= i2c_priv.h
OBJS  		= $(SRCS:.c=.o)
HDRS		= i2c_priv.h
MOD   		= i2c

CFLAGS = $(DEBUG) $(WARNINGS) $(COMPILE) $(I2C_OPT) $(KERNEL_OPT)
SFLAGS = -fverbose-asm $(WARNINGS) $(COMPILE) $(PRT_OPT) $(KERNEL_OPT)

all: detect

clean:
	-rm -f detect 

