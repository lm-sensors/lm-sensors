#!/bin/bash

# Here you can set several defaults.

# The number of devices to create (max: 256)
# If not provided on the command line, default to 32
NUMBER=${1:-32}

# The owner and group of the devices
OUSER=root
OGROUP=root
# The mode of the devices
MODE=600

# This script doesn't need to be run if devfs is used
if [ -r /proc/mounts ] ; then
	if grep -q "/dev devfs" /proc/mounts ; then
		echo "You do not need to run this script as your system uses devfs."
		exit;
	fi
fi

i=0;

while [ $i -lt $NUMBER ] ; do
	echo /dev/i2c-$i
	mknod -m $MODE /dev/i2c-$i c 89 $i || exit
	chown "$OUSER:$OGROUP" /dev/i2c-$i || exit
	i=$[$i + 1]
done
