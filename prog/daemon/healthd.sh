#!/bin/bash
#
# healthd -- 	This is a simple daemon which can be used to alert you in the
#		event of a hardware health monitoring alarm by sending an 
#		email to the value of ADMIN_EMAIL (defined below).
#
# To Use  --	Simply start the daemon from a shell (may be backgrounded)
#
# Other details -- Checks status every 15 seconds.  Sends warning emails every
#		   ten minutes during alarm until the alarm is cleared.
#		   It won't start up if there is a pending alarm on startup.
#		   Very low loading on the machine (sleeps almost all the time).
#		   This is just an example.  It works, but hopefully we can
#		   get something better written. :')
#
# Requirements -- grep, mail, sensors, bash
#		  (You may need to alter the PATH, but probably not.)
#
# Written & Copyrighten by Philip Edelbrock, 1999.
#
# Version: 1.0.0
#

PATH="/bin:/usr/bin:/usr/local/bin:${PATH}"

ADMIN_EMAIL="root@localhost"

if [ -n "`sensors | grep ALARM`" ]
then
        echo "Pending Alarms on start up!  Exiting!"
        exit
fi

while true
do
 sleep 15
 if [ -n "`sensors | grep ALARM`" ]
 then
        sensors | mail -s "**** Hardware Health Warning ****"  $ADMIN_EMAIL
        sleep 600
 fi
done
