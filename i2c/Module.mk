# Note that MODULE_DIR (the directory in which this file resides) is a
# 'simply expanded variable'. That means that its value is substituted
# verbatim in the rules, until it is redefined. 
MODULE_DIR := i2c

TARGETS := $(MODULE_DIR)/i2c-core.o  $(MODULE_DIR)/algo-bit.o \
           $(MODULE_DIR)/i2c-dev.o   $(MODULE_DIR)/bit-lp.o \
           $(MODULE_DIR)/bit-velle.o $(MODULE_DIR)/bit-mb.o

all :: $(TARGETS)

install :: $(TARGETS)
	$(MKDIR) $(MODDIR)
	install -o root -g root -m 644 $(MODDIR) $(TARGETS)

clean ::
	$(RM) $(TARGETS) 
