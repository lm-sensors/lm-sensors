#! /bin/sh

lnxvers="`uname -s` `uname -r`"
#host="`hostname -s`"
host="`hostname`"
blink="o"

writelcd=/home/rdm/src/lm_sensors-2.5.0/prog/maxilife/writelcd.sh

$writelcd "$host" 3

i=0
while true; do
   $writelcd "$lnxvers $blink" 2
   if [ $[i == 5] = 1 ]; then
      $writelcd "`awk '{ printf("%s %s %s", $1, $2, $3) }' < /proc/loadavg`" 4
      i=0
   fi
   if [ $blink = "o" ]; then
      blink=O
   else
      blink=o
   fi
   sleep 1
   i=$[i+1]
done
