#!/usr/bin/perl

# This is a simple perl script to display a 'screen' 
# on a Matrix Orbital Display.  In this case, this
# script will read the first four lines (up to 20
# chars a line) and display them on the display.

# Written and copywritten (C) by Philip Edelbrock, 1999.


# Turn off the blinking cursor
$temp=`echo  "254 84" > /proc/sys/dev/sensors/matorb*/disp`;

$linenum=1;

while (<STDIN>) {
 # Reset the position of the cursor to the next line
 $temp=`echo "254 71 1 $linenum" > /proc/sys/dev/sensors/matorb*/disp`;
 chop;
 $_="$_                    ";
 if (/^(.{1,20})/) {
  $_=$1;
  $line="";
  while (/(.)/gc) {
   $temp=ord($1);
   $line="$line $temp";
  }
  $temp=`echo "$line" > /proc/sys/dev/sensors/matorb*/disp`;
 }
 $linenum+=1;
 if ($linenum > 4) { exit; }
}
