INCLUDEDIR = /usr/include
CFLAGS = -D__KERNEL__ -DMODULE -DLINUX -DMODVERSION -O2 -Wall -I$(INCLUDEDIR)

# Compile with Debugging turned on - watch out, lots of pci device infos.
#CFLAGS = -D__KERNEL__ -DMODULE -DLINUX -DMODVERSION -DM7101_DEBUG -O2 -Wall -I$(INCLUDEDIR)
LD = ld

# extract versionnumber from version.h
VER = $(shell awk -F\" '/REL/ {print $$2}' $(INCLUDEDIR)/linux/version.h)

OBJS = m7101.o

all: $(OBJS)
