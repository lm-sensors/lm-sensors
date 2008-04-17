#
# quick makefile for the FAQ.
# So that both the text and html FAQ are accessible on the web
# they are checked into SVN along with the .texi source.
# Don't forget to update version.texi.
# Run 'make' and then check in all 4 files after editing the .texi source.
#
all::  lm_sensors-FAQ.html FAQ

%.html: %.texi version.texi
	makeinfo --html --no-split --number-sections $<

FAQ: lm_sensors-FAQ.texi
	makeinfo --no-headers --number-sections -o FAQ $<
