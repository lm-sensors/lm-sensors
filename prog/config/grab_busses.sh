#!/bin/bash

# This simple shell script grabs the contents of /proc/bus/i2c and outputs it
# in sensors.conf format through stdout. You can use it to generate those
# nasty 'bus' statements in config files.

if [ $# = 0 ] ; then
  file=/proc/bus/i2c
elif [ $# = 1 ] ; then
  file="$1"
else
  echo "Syntax: grab_busses [file]" >&2
  exit 1
fi

if [ ! -e "$file" ]; then
  echo "Can't find file \`$file';"\
       "try \`modprobe i2c-proc' for /proc/bus/i2c" >&2
  exit 1
fi

cat -- "$file" | awk \
	'	BEGIN	{ FS = "\t" }
			{ sub(" *$","",$3)
			  sub(" *$","",$4)
			  printf "bus \"%s\" \"%s\" \"%s\"\n",$1,$3,$4 }
			{ nrlines++ }
		END	{ printf "Total lines: %d\n",nrlines > "/dev/stderr"  }
	'
