#!/bin/bash

# Here you can set several defaults.

# The number of devices to create (max: 256)
NUMBER=32

# The owner and group of the devices
OUSER=root
OGROUP=root
# The mode of the devices
MODE=600


i=0;

while [ $i -lt $NUMBER ] ; do
  echo /dev/i2c-$i
  mknod -m 000 /dev/i2c-$i c 89 $i
  chown "$OUSER:$OGROUP" /dev/i2c-$i
  chmod $MODE /dev/i2c-$i
  i=$[$i + 1]
done
