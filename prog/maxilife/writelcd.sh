#! /bin/sh

usage() {
   echo "usage: $0 <string> <line>"
   echo "       <string> must be <= 16 characters"
   echo "       <line> lcd line number, between 1..4"
}

if [ $# != 2 ]; then
   usage
   exit 1
fi

str=$1
line=$2

if [ $[line < 1 || line > 4] = 1 ]; then
   usage
   exit 1
fi

sysctl=/proc/sys/dev/sensors/maxilife-nba-i2c-0-14/lcd

printf "%-16.16s" "$str" | od -A n -l > $sysctl$line

exit 0
