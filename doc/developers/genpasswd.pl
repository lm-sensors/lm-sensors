#!/usr/bin/perl
#
# Use this Perl script to generate a password
# for you to use for CVS access.  Then, send
# the output to phil@netroedge.com so Phil
# can add you to the CVS writers group
# (pending approval, of course).

$salt1=int(65 + rand(115 - 65));
$salt2=int(65 + rand(115 - 65));
if ($salt1 > 90) { $salt1+=7; }
if ($salt2 > 90) { $salt2+=7; }
$salt= pack("cc",$salt1, $salt2);                   
print "This program will generate an encrypted version of your CVS password.\n";
print "Enter your CVS password below.\n";
system "stty -echo";
print "Password: ";
chop($word = <STDIN>);
print "\n\n";
print "Please enter it again.\n";
print "Password: ";
chop($word2 = <STDIN>);
print "\n";
system "stty echo";
if ($word ne $word2) { print "Passwords do not match, action aborted!\n"; exit; }
$passwd=crypt($word, $salt);
print "Here is your encrypted password: $passwd\n";
print "Send this encrypted password to phil\@netroedge.com\n";
print "with your requested username.\n";
print "Please specify whether you want access to i2c, lm_sensors, or both.\n";
print "Please also indicate what area of the project you wish to work on.\n";
print "\n";
print "Please include your username in the CVS comments when you\n";
print "check in files like so: (username)\n";
