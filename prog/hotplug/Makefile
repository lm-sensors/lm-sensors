INCLUDEDIR = /usr/include
#CFLAGS := -D__KERNEL__ -DMODULE -DLINUX -O2 -Wall -I$(INCLUDEDIR)
CC := gcc

# Compile with Debugging turned on - watch out, lots of pci device infos.
CFLAGS = -D__KERNEL__ -DMODULE -DLINUX -DM7101_DEBUG -O2 -Wall -I$(INCLUDEDIR)
LD := ld

# extract versionnumber from version.h
VER = $(shell awk -F\" '/REL/ {print $$2}' $(INCLUDEDIR)/linux/version.h)

OBJS = m7101.o

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	install -o root -g root -m 644 $(OBJS) /lib/modules/$(VER)/misc

clean:
	rm -f $(OBJS)
