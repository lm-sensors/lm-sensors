#
# quick makefile for the FAQ.
# So that both the text and html FAQ are accessible on the web
# (the website points into CVS for the FAQ),
# they are checked into CVS along with the .texi source.
# Don't forget to update version.texi.
# Run 'make' and then check in all 4 files after editing the .texi source.
# Or do 'make ci'
#
all::  lm_sensors-FAQ.html FAQ

%.html: %.texi version.texi
	makeinfo --html --no-split --number-sections $<

FAQ: lm_sensors-FAQ.texi
	makeinfo --no-headers --number-sections -o FAQ $<

ci: all
	cvs ci lm_sensors-FAQ.texi lm_sensors-FAQ.html FAQ version.texi
