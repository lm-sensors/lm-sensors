#
# Makefile for the kernel hardware sensors drivers.
#

SUB_DIRS     :=
MOD_SUB_DIRS := $(SUB_DIRS)
ALL_SUB_DIRS := $(SUB_DIRS)
MOD_LIST_NAME := SENSORS_MODULES

L_TARGET := sensors.a
MX_OBJS :=  
M_OBJS  := 
LX_OBJS :=
L_OBJS  := 

# -----
# i2c core components
# -----

ifeq ($(CONFIG_SENSORS),y)
  LX_OBJS += sensors.o
else
  ifeq ($(CONFIG_SENSORS),m)
    MX_OBJS += sensors.o
  endif
endif

ifeq ($(CONFIG_SENSORS_ADM1021),y)
  L_OBJS += adm1021.o
else
  ifeq ($(CONFIG_SENSORS_ADM1021),m)
    M_OBJS += adm1021.o
  endif
endif

ifeq ($(CONFIG_SENSORS_ADM9024),y)
  L_OBJS += adm9240.o
else
  ifeq ($(CONFIG_SENSORS_ADM9024),m)
    M_OBJS += adm9240.o
  endif
endif

ifeq ($(CONFIG_SENSORS_EEPROM),y)
  L_OBJS += eeprom.o
else
  ifeq ($(CONFIG_SENSORS_EEPROM),m)
    M_OBJS += eeprom.o
  endif
endif

ifeq ($(CONFIG_SENSORS_GL518SM),y)
  L_OBJS += gl518sm.o
else
  ifeq ($(CONFIG_SENSORS_GL518SM),m)
    M_OBJS += gl518sm.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LM75),y)
  L_OBJS += lm75.o
else
  ifeq ($(CONFIG_SENSORS_LM75),m)
    M_OBJS += lm75.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LM78),y)
  L_OBJS += lm78.o
else
  ifeq ($(CONFIG_SENSORS_LM78),m)
    M_OBJS += lm78.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LM80),y)
  L_OBJS += lm80.o
else
  ifeq ($(CONFIG_SENSORS_LM80),m)
    M_OBJS += lm80.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LTC1710),y)
  L_OBJS += ltc1710.o
else
  ifeq ($(CONFIG_SENSORS_LTC1710),m)
    M_OBJS += ltc1710.o
  endif
endif

ifeq ($(CONFIG_SENSORS_SIS5595),y)
  L_OBJS += sis5595.o
else
  ifeq ($(CONFIG_SENSORS_SIS5595),m)
    M_OBJS += sis5595.o
  endif
endif

ifeq ($(CONFIG_SENSORS_W83781D),y)
  L_OBJS += w83781d.o
else
  ifeq ($(CONFIG_SENSORS_W83781D),m)
    M_OBJS += w83781d.o
  endif
endif

include $(TOPDIR)/Rules.make

