#!/usr/bin/perl

#
#    doc-insmod.pl - Generate documentation about insmod parameters.
#    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

# Use the following patch (apply by hand) if modinfo does not seem to terminate:
#
#  *** /tmp/modutils-2.1.121/insmod/modinfo.c      Mon Sep 14 20:55:28 1998
#  --- modutils-2.1.121/insmod/modinfo.c   Wed Apr 14 20:54:38 1999
#  ***************
#  *** 457,462 ****
#     error_file = "modinfo";
#   
# !   while (optind < argc)
#       show_module_info(argv[optind], fmtstr, do_parameters);
#   
#     return 0;
# --- 457,464 ----
#     error_file = "modinfo";
#   
# !   while (optind < argc) {
#       show_module_info(argv[optind], fmtstr, do_parameters);
# +     optind ++;
# +   }
#   
#     return 0;


use strict;

use Text::Wrap;

# @_[0]: name and path of the kernel module
sub print_info
{
  my ($modname) = @_;
  my (@lines,$line,$option,$type,$desc);
  print wrap("Author: ","        ",`modinfo -a $modname`), "\n\n";
  print "Module Parameters\n";
  print "-----------------\n";
  print "\n";
  open OPTIONS,"modinfo -p $modname|";
  @lines = <OPTIONS>;
  close OPTIONS;
  foreach $line ( sort { $a cmp $b } @lines ) {
    ($option,$type,$desc) = $line =~ /^(\S+) (.+), description "(.*)"$/;
    print "* $option: $type\n";
    print wrap("  ","  ",$desc),"\n";
  }
}

if (@ARGV != 1 or @ARGV[0] =~ /^-/) {
  print STDERR "Syntax: doc-insmod.pl MODULE\n";
  print STDERR "MODULE should be the full path to a module name.\n";
  print STDERR "This program is only tested with modutils-2,1.121.\n";
  print STDERR "If it does not seem to terminate, you need to patch modinfo. ".
               "Examine the source\n".
               "code of doc-modules.pl to find the patch.\n";
  exit 0;
}
print_info @ARGV
