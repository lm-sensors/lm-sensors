#!/usr/bin/perl

#    mkpatch - Create patches against the Linux kernel
#    Copyright (c) 1999  Frodo Looijaard <frodol@dds.nl>
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

use strict;

# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
# $_[2]: Name of the kernel file
# $_[3]: Name of the patched file
sub print_diff
{
  my ($package_root,$kernel_root,$kernel_file,$package_file) = @_;
  my ($diff_command,$dummy);

  $diff_command = "diff -u2 $kernel_root/$kernel_file ";
  $diff_command .= "$package_root/$package_file";
  open INPUT, "$diff_command|";
  $dummy = <INPUT>;
  $dummy = <INPUT>;
  print "--- linux-old/$kernel_file\t".`date`;
  print "+++ linux/$kernel_file\t".`date`;
    
  while (<INPUT>) {
    print;
  }
  close INPUT;
}

# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_Makefile
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "Makefile";
  my $package_file = "mkpatch/.temp";

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/mkpatch/.temp"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if (m@CONFIG_SENSORS@) {
      $_ = <INPUT> while not m@endif@;
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
    }
    if (m@include arch/\$\(ARCH\)/Makefile@) {
      print OUTPUT <<'EOF';
ifeq ($(CONFIG_SENSORS),y)
DRIVERS := $(DRIVERS) drivers/sensors/sensors.a
endif

EOF
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}

# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_Makefile
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/Makefile";
  my $package_file = "mkpatch/.temp";
  my $sensors_present;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/mkpatch/.temp"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if (m@^ALL_SUB_DIRS\s*:=@) {
      $sensors_present = 0;
      while (m@\\$@) {
        $sensors_present = 1 if m@sensors@;
        print OUTPUT;
        $_ = <INPUT>;
      }
      $sensors_present = 1 if m@sensors@;
      s@$@ sensors@ if (not $sensors_present);
    } 
    if (m@CONFIG_SENSORS@) {
      $_ = <INPUT> while not m@^endif@;
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
    } 
    if (m@^include \$\(TOPDIR\)/Rules.make$@) {
      print OUTPUT <<'EOF';
ifeq ($(CONFIG_SENSORS),y)
SUB_DIRS += sensors
MOD_SUB_DIRS += sensors
else
  ifeq ($(CONFIG_SENSORS),m)
  MOD_SUB_DIRS += sensors
  endif
endif

EOF
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}

# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_char_Config_in
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/char/Config.in";
  my $package_file = "mkpatch/.temp";
  my $ready = 0;
  my $done = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/mkpatch/.temp"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if (m@source drivers/i2c/Config.in@) {
      print OUTPUT;
      $_ = 'source drivers/sensors/Config.in';
    }
    if (m@sensors@) {
      $_ = <INPUT>;
      $_ = <INPUT> if (m@^$@);
    }
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}
 

# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_char_mem_c
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/char/mem.c";
  my $package_file = "mkpatch/.temp";
  my $right_place = 0;
  my $with_video = 0;
  my $done = 0;
  my $atstart = 1;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/mkpatch/.temp"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if ($atstart and m@#ifdef@) {
      print OUTPUT << 'EOF';
#ifdef CONFIG_I2C
extern void i2c_init_all(void);
#endif
EOF
      $atstart = 0;
    }
    if (not $right_place and m@CONFIG_I2C@) {
      $_ = <INPUT> while not m@#endif@;
      $_ = <INPUT>;
    }
    $with_video = 1 if m@CONFIG_VIDEO_BT848@;
    $right_place = 1 if (m@lp_init\(\);@);
    if ($right_place and not $done and
        (m@CONFIG_I2C@ or m@CONFIG_VIDEO_BT848@ or m@return 0;@)) {
      if (not m@return 0;@) {
        $_ = <INPUT> while not m@#endif@;
        $_ = <INPUT>;
        $_ = <INPUT> if m@^$@;
      }
      if ($with_video) {
        print OUTPUT '#if defined(CONFIG_I2C) || defined(CONFIG_VIDEO_BT848)';
      } else {
        print OUTPUT '#ifdef CONFIG_I2C';
      }
      print OUTPUT <<'EOF';

	i2c_init_all();
#endif

EOF
      $done = 1;
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}
 

sub main
{
  my ($package_root,$kernel_root,%files,%includes,$package_file,$kernel_file);
  my ($diff_command,$dummy,$data0,$data1,$sedscript);

  # --> Read the command-lineo
  $package_root = $ARGV[0];
  die "Package root `$package_root' is not found\n" 
        unless -d "$package_root/mkpatch";
  $kernel_root = $ARGV[1];
  die "Kernel root `$kernel_root' is not found\n" 
        unless -f "$kernel_root/Rules.make";

  # --> Read FILES
  open INPUT, "$package_root/mkpatch/FILES" 
        or die "Can't open `$package_root/mkpatch/FILES'";
  while (<INPUT>) {
    ($data0,$data1) = /(\S+)\s+(\S+)/;
    $files{$data0} = $data1;
  } 
  close INPUT;

  # --> Read INCLUDES
  open INPUT, "$package_root/mkpatch/INCLUDES" 
        or die "Can't open `$package_root/mkpatch/INCLUDES'";
  while (<INPUT>) {
    ($data0,$data1) = /(\S+)\s+(\S+)/;
    $includes{$data0} = $data1;
    $sedscript .= 's,(#\s*include\s*)'.$data0.'(\s*),\1'."$data1".'\2, ; ';
  } 
  close INPUT;

  # --> Start generating
  foreach $package_file (keys %files) {
    $kernel_file = $files{$package_file};
    $diff_command = "diff -u2 ";
    if ( -f "$kernel_root/$kernel_file") {
      $diff_command .= "$kernel_root/$kernel_file";
    } else {
      $diff_command .= "/dev/null";
    }
    $diff_command .= " $package_root/$package_file";
    open INPUT, "$diff_command|";
    $dummy = <INPUT>;
    $dummy = <INPUT>;
    print "--- linux-old/$kernel_file\t".`date`;
    print "+++ linux/$kernel_file\t".`date`;
    
    while (<INPUT>) {
      eval $sedscript;
      print;
    }
    close INPUT;
  }

  gen_Makefile $package_root, $kernel_root;
  gen_drivers_Makefile $package_root, $kernel_root;
  gen_drivers_char_Config_in $package_root, $kernel_root;
#  gen_drivers_char_mem_c $package_root, $kernel_root;
}

main;

