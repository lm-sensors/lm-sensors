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

