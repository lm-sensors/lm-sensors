#!/usr/bin/perl

# This is a simple perl script to display a 'screen' 
# on a Matrix Orbital Display.  In this case, this
# script will read the first four lines (up to 20
# chars a line) and display them on the display.

# Written and copywritten (C) by Philip Edelbrock, 1999.


# Clear the screen
$temp=`echo  "254 88" > /proc/sys/dev/sensors/matorb*/disp`;

# Turn off the blinking cursor
$temp=`echo  "254 84" > /proc/sys/dev/sensors/matorb*/disp`;

$line=1;

while (<STDIN>) {
# Reset the position of the cursor to the next line
$temp=`echo "254 71 1 $line" > /proc/sys/dev/sensors/matorb*/disp`;
 if (/^(.{1,20})/) {
  $_=$1;
  while (/(.)/gc) {
   $temp=ord($1);
   $temp=`echo "$temp" > /proc/sys/dev/sensors/matorb*/disp`;
  }
 }
 $line= $line + 1;
 if ($line > 4) { exit; }
}
