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

use vars qw($temp);
$temp = "mkpatch/.temp";

# Generate a diff between the old kernel file and the new I2C file. We
# arrange the headers to tell us the old tree was under directory
# `linux-old', and the new tree under `linux'.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
# $_[2]: Name of the kernel file
# $_[3]: Name of the patched file
sub print_diff
{
  my ($package_root,$kernel_root,$kernel_file,$package_file) = @_;
  my ($diff_command,$dummy);

  $diff_command = "diff -u2";
  if ( -e "$kernel_root/$kernel_file") {
    $diff_command .= " $kernel_root/$kernel_file ";
  } else {
    $diff_command .= " /dev/null ";
  }
  if ( -e "$package_root/$package_file") {
    $diff_command .= " $package_root/$package_file ";
  } else {
    $diff_command .= " /dev/null";
  }
  open INPUT, "$diff_command|" or die "Can't execute `$diff_command'";
  $dummy = <INPUT>;
  $dummy = <INPUT>;
  print "--- linux-old/$kernel_file\t".`date`;
  print "+++ linux/$kernel_file\t".`date`;
    
  while (<INPUT>) {
    print;
  }
  close INPUT;
}

# This generates diffs for kernel file Documentation/Configure.help. This
# file contains the help texts that can be displayed during `make *config'
# for the kernel.
# The new texts are put at the end of the file, or just before the
# lm_sensors texts.
# Of course, care is taken old lines are removed.
# NOTE: MOST OF THE TEXTS ARE UNWRITTEN YET!
# $_[0]: i2c package root (like /tmp/i2c)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_Documentation_Configure_help
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "Documentation/Configure.help";
  my $package_file = $temp;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    while (m@I2C mainboard interfaces@ or 
           m@Acer Labs ALI 1533 and 1543C@ or
           m@Apple Hydra Mac I/O@ or
           m@Intel 82371AB PIIX4(E)@ or
           m@VIA Technologies, Inc. VT82C586B@ or
           m@Pseudo ISA adapter (for hardware sensors modules)@ or
           m@Analog Devices ADM1021 and compatibles@ or
           m@Analog Devices ADM9240 and compatibles@ or
           m@Genesys Logic GL518SM@ or
           m@National Semiconductors LM75@ or
           m@National Semiconductors LM78@ or
           m@National Semiconductors LM80@ or
           m@Silicon Integrated Systems Corp. SiS5595@ or
           m@Winbond W83781D, W83782D and W83783S@ or
           m@EEprom (DIMM) reader@ or
           m@Linear Technologies LTC1710@) {
      $_ = <INPUT>;
      $_ = <INPUT>;
      $_ = <INPUT> while not m@^\S$@ and not eof(INPUT);
    }
    if (eof(INPUT)) {
      print OUTPUT <<'EOF'
I2C mainboard interfaces
CONFIG_I2C_MAINBOARD
  No help yet

Acer Labs ALI 1533 and 1543C
CONFIG_I2C_ALI5X3
  No help yet

Apple Hydra Mac I/O
CONFIG_I2C_HYDRA
  No help yet

Intel 82371AB PIIX4(E)
CONFIG_I2C_PIIX4
  No help yet

VIA Technologies, Inc. VT82C586B
CONFIG_I2C_VIA
  No help yet

Pseudo ISA adapter (for hardware sensors modules)
CONFIG_I2C_ISA
  No help yet

Analog Devices ADM1021 and compatibles
CONFIG_SENSORS_ADM1021 
  No help yet

Analog Devices ADM9240 and compatibles
CONFIG_SENSORS_ADM9240
  No help yet

Genesys Logic GL518SM
CONFIG_SENSORS_GL518SM
  No help yet

National Semiconductors LM75
CONFIG_SENSORS_LM75 
  No help yet

National Semiconductors LM78
CONFIG_SENSORS_LM78
  No help yet

National Semiconductors LM80
CONFIG_SENSORS_LM80
  No help yet

Silicon Integrated Systems Corp. SiS5595
CONFIG_SENSORS_SIS5595
  No help yet

Winbond W83781D, W83782D and W83783S
CONFIG_SENSORS_W83781D
  No help yet

EEprom (DIMM) reader
CONFIG_SENSORS_EEPROM
  No help yet

Linear Technologies LTC1710
CONFIG_SENSORS_LTC1710
  No help yet

EOF
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}


# This generates diffs for the main Linux Makefile.
# Three lines which add drivers/sensors/sensors.a to the DRIVERS list are 
# put just before the place where the architecture Makefile is included.
# Of course, care is taken old lines are removed.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_Makefile
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "Makefile";
  my $package_file = $temp;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
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

# This generates diffs for drivers/Makefile
# First, `sensors' is added to the ALL_SUB_DIRS list. Next, a couple of lines
# to add sensors to the SUB_DIRS and/or MOD_SUB_DIRS lists is put right before
# Rules.make is included.
# Of course, care is taken old lines are removed.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_Makefile
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/Makefile";
  my $package_file = $temp;
  my $sensors_present;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
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

# This generates diffs for drivers/char/Config.in
# It adds a line just before CONFIG_APM or main_menu_option lines to include
# the sensors Config.in.
# Of course, care is taken old lines are removed.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_char_Config_in
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/char/Config.in";
  my $package_file = $temp;
  my $ready = 0;
  my $done = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if (m@source drivers/i2c/Config.in@) {
      print OUTPUT;
      print OUTPUT "source drivers/sensors/Config.in\n";
      $_ = <INPUT>;
    }
    if (m@sensors@) {
      $_ = <INPUT>;
      $_ = <INPUT> if (m@^$@);
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}
 

# This generates diffs for drivers/char/mem.c They are a bit intricate.
# Lines are generated at the beginning to declare sensors_init_all
# At the bottom, a call to sensors_init_all is added when the
# new lm_sensors stuff is configured in.
# Of course, care is taken old lines are removed.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_char_mem_c
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/char/mem.c";
  my $package_file = $temp;
  my $right_place = 0;
  my $done = 0;
  my $atstart = 1;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if ($atstart and m@#ifdef@) {
      print OUTPUT << 'EOF';
#ifdef CONFIG_SENSORS
extern void sensors_init_all(void);
#endif
EOF
      $atstart = 0;
    }
    if (not $right_place and m@CONFIG_SENSORS@) {
      $_ = <INPUT> while not m@#endif@;
      $_ = <INPUT>;
    }
    $right_place = 1 if (m@lp_init\(\);@);
    if ($right_place and not $done and
        (m@CONFIG_SENSORS@ or m@return 0;@)) {
      if (not m@return 0;@) {
        $_ = <INPUT> while not m@#endif@;
        $_ = <INPUT>;
        $_ = <INPUT> if m@^$@;
      }
      print OUTPUT <<'EOF';
#ifdef CONFIG_SENSORS
	sensors_init_all();
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


# This generates diffs for drivers/i2c/Config.in
# Several adapter drivers that are included in the lm_sensors package are
# added at the bottom.
# Of course, care is taken old lines are removed.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_i2c_Config_in
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/i2c/Config.in";
  my $package_file = "$temp";

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if (m@CONFIG_I2C_MAINBOARD@) {
      $_ = <INPUT> while not m@^  fi$@;
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
    }
    if (m@^fi@) {
      print OUTPUT << 'EOF'
  bool 'I2C mainboard interfaces' CONFIG_I2C_MAINBOARD 
  if [ "$CONFIG_I2C_MAINBOARD" = "y" ]; then
    dep_tristate '  Acer Labs ALI 1533 and 1543C' CONFIG_I2C_ALI5X3 $CONFIG_I2C_MAINBOARD
    dep_tristate '  Apple Hydra Mac I/O' CONFIG_I2C_HYDRA $CONFIG_I2C_MAINBOARD
    dep_tristate '  Intel 82371AB PIIX4(E)' CONFIG_I2C_PIIX4 $CONFIG_I2C_MAINBOARD
    dep_tristate '  VIA Technologies, Inc. VT82C586B' CONFIG_I2C_VIA $CONFIG_I2C_MAINBOARD
    dep_tristate '  Pseudo ISA adapter (for hardware sensors modules)' CONFIG_I2C_ISA $CONFIG_I2C_MAINBOARD
  fi

EOF
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}

# This generates diffs for drivers/i2c/Makefile.
# Lines to add correct files to M_OBJS and/or L_OBJS are added just before
# Rules.make is included
# Of course, care is taken old lines are removed.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_i2c_Makefile
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/i2c/Makefile";
  my $package_file = $temp;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    while (m@CONFIG_I2C_ALI5X3@ or m@CONFIG_I2C_HYDRA@ or m@CONFIG_I2C_PIIX4@ or
        m@CONFIG_I2C_VIA@ or m@CONFIG_I2C_ISA@) {
      $_ = <INPUT> while not m@^endif@;
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
    }
    if (m@Rules.make@) {
      print OUTPUT << 'EOF'
ifeq ($(CONFIG_I2C_ALI5X3),y)
  L_OBJS += i2c-ali5x3.o
else 
  ifeq ($(CONFIG_I2C_ALI5X3),m)
    M_OBJS += i2c-ali5x3.o
  endif
endif

ifeq ($(CONFIG_I2C_HYDRA),y)
  L_OBJS += i2c-hydra.o
else 
  ifeq ($(CONFIG_I2C_HYDRA),m)
    M_OBJS += i2c-hydra.o
  endif
endif

ifeq ($(CONFIG_I2C_PIIX4),y)
  L_OBJS += i2c-piix4.o
else 
  ifeq ($(CONFIG_I2C_PIIX4),m)
    M_OBJS += i2c-piix4.o
  endif
endif

ifeq ($(CONFIG_I2C_VIA),y)
  L_OBJS += i2c-via.o
else 
  ifeq ($(CONFIG_I2C_VIA),m)
    M_OBJS += i2c-via.o
  endif
endif

ifeq ($(CONFIG_I2C_ISA),y)
  L_OBJS += i2c-isa.o
else 
  ifeq ($(CONFIG_I2C_ISA),m)
    M_OBJS += i2c-isa.o
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

# This generates diffs for drivers/i2c/i2c-core.c
# Lines are generated at the beginning to declare several *_init functions.
# At the bottom, calls to them are added when the sensors stuff is configured
# in.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_i2c_i2c_core_c
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/i2c/i2c-core.c";
  my $package_file = $temp;
  my $right_place = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    while (m@CONFIG_I2C_ALI5X3@ or m@CONFIG_I2C_HYDRA@ or m@CONFIG_I2C_PIIX4@ or
        m@CONFIG_I2C_VIA@ or m@CONFIG_I2C_ISA@) {
      $_ = <INPUT> while not m@#endif@;
      $_ = <INPUT>;
    }
    if (m@^int __init i2c_init_all@) {
      $right_place = 1;
      print OUTPUT << 'EOF';
#ifdef CONFIG_I2C_ALI5X3
	extern int i2c_ali5x3_init(void);
#endif
#ifdef CONFIG_I2C_HYDRA
	extern int i2c_hydra_init(void);
#endif
#ifdef CONFIG_I2C_PIIX4
	extern int i2c_piix4_init(void);
#endif
#ifdef CONFIG_I2C_VIA
	extern int i2c_via_init(void);
#endif
#ifdef CONFIG_I2C_ISA
	extern int i2c_isa_init(void);
#endif
EOF
    }
    if ($right_place and m@return 0;@) {
      print OUTPUT << 'EOF';
#ifdef CONFIG_I2C_ALI5X3
	i2c_ali5x3_init();
#endif
#ifdef CONFIG_I2C_HYDRA
	i2c_hydra_init();
#endif
#ifdef CONFIG_I2C_PIIX4
	i2c_piix4_init();
#endif
#ifdef CONFIG_I2C_VIA
	i2c_via_init();
#endif
#ifdef CONFIG_I2C_ISA
	i2c_isa_init();
#endif
EOF
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}

# Main function
sub main
{
  my ($package_root,$kernel_root,%files,%includes,$package_file,$kernel_file);
  my ($diff_command,$dummy,$data0,$data1,$sedscript,$version_string);

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

  # --> Read "version.h"
  open INPUT, "$package_root/version.h"
        or die "Can't open `$package_root/version.h'";
  $version_string .= $_ while <INPUT>;
  close INPUT;
 
  # --> Start generating
  foreach $package_file (sort keys %files) {
    open INPUT,"$package_root/$package_file" 
          or die "Can't open `$package_root/$package_file'";
    open OUTPUT,">$package_root/$temp"
          or die "Can't open `$package_root/$temp'";
    while (<INPUT>) {
      eval $sedscript;
      if (m@#\s*include\s*"version.h"@) {
        print OUTPUT $version_string;
      } elsif (m@#\s*include\s*"compat.h"@) {
        print OUTPUT << 'EOF';

/* --> COMPATIBILITY SECTION FOR OLD (2.0, 2.1) KERNELS */

#ifdef MODULE
#include <linux/module.h>
#ifndef MODULE_AUTHOR
#define MODULE_AUTHOR(whatever)
#endif
#ifndef MODULE_DESCRIPTION
#define MODULE_DESCRIPTION(whatever)
#endif
#endif /* def MODULE */

EOF
        if (`grep KERNEL_VERSION "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) | ((b) << 8) | (c))
#endif

EOF
        }
        if (`grep 'copy_from_user\\|copy_to_user\\|get_user_data' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
/* copy_from/to_usr is called memcpy_from/to_fs in 2.0 kernels 
   get_user was redefined in 2.1 kernels to use two arguments, and returns
   an error code */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,4))
#define copy_from_user memcpy_fromfs
#define copy_to_user memcpy_tofs
#define get_user_data(to,from) ((to) = get_user(from),0)
#else
#include <asm/uaccess.h>
#define get_user_data(to,from) get_user(to,from)
#endif

EOF
        }
        if (`grep 'schedule_timeout' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
/* Add a scheduling fix for the new code in kernel 2.1.127 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,127))
#define schedule_timeout(x) ( current->timeout = jiffies + (x), schedule() )
#endif

EOF
        }
        if (`grep 'pci_' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
/* If the new PCI interface is not present, fall back on the old PCI BIOS
   interface. We also define some things to unite both interfaces. Not
   very nice, but it works like a charm. 
   device is the 2.1 struct pci_dev, bus is the 2.0 bus number, dev is the
   2.0 device/function code, com is the PCI command, and res is the result. */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,54))
#define pci_present pcibios_present
#define pci_read_config_byte_united(device,bus,dev,com,res) \
                            pcibios_read_config_byte(bus,dev,com,res)
#define pci_read_config_word_united(device,bus,dev,com,res) \
                            pcibios_read_config_word(bus,dev,com,res)
#define pci_write_config_byte_united(device,bus,dev,com,res) \
                            pcibios_write_config_byte(bus,dev,com,res)
#define pci_write_config_word_united(device,bus,dev,com,res) \
                            pcibios_write_config_word(bus,dev,com,res)
#else
#define pci_read_config_byte_united(device,bus,dev,com,res) \
                            pci_read_config_byte(device,com,res)
#define pci_read_config_word_united(device,bus,dev,com,res) \
                            pci_read_config_word(device,com,res)
#define pci_write_config_byte_united(device,bus,dev,com,res) \
                            pci_write_config_byte(device,com,res)
#define pci_write_config_word_united(device,bus,dev,com,res) \
                            pci_write_config_word(device,com,res)
#endif

EOF
        }
        if (`grep 'ioremap\|iounmap' "$package_root/$package_file"`) {
          print OUTPUT  << 'EOF';
/* I hope this is always correct, even for the PPC, but I really think so.
   And yes, the kernel version is exactly correct */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0))
#include <linux/mm.h>
#define ioremap vremap
#define iounmap vfree
#endif

EOF
        }
        if (`grep 'init_MUTEX' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,1))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif
EOF
        }
	if (`grep '__init' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,53)
#include <linux/init.h>
#else
#define __init
#define __initdata
#endif
EOF
	}
        if (`grep 'PCI_DEVICE_ID_VIA_82C586_3' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,0,34))
#define PCI_DEVICE_ID_VIA_82C586_3  0x3040
#endif
EOF

        }
        if (`grep 'PCI_DEVICE_ID_AL_M7101' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,0,34))
#define PCI_DEVICE_ID_AL_M7101 0x7101
#endif
EOF
        }
        if (`grep 'PCI_DEVICE_ID_INTEL_82371AB_3' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,0,31))
#define PCI_DEVICE_ID_INTEL_82371AB_3  0x7113
#endif
EOF
        }
        if (`grep 'PCI_VENDOR_ID_APPLE' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,0,31))
#define PCI_VENDOR_ID_APPLE            0x106b
#endif
EOF
        }
        if (`grep 'PCI_DEVICE_ID_APPLE_HYDRA' "$package_root/$package_file"`) {
          print OUTPUT << 'EOF';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,0,31))
#define PCI_DEVICE_ID_APPLE_HYDRA      0x000e
#endif
EOF
        }
	if (`grep 'PCI_DEVICE_ID_INTEL_82801AA_3\|PCI_DEVICE_ID_INTEL_82801AB_3' "$package_root/$package_file"`) {
	  print OUTPUT << 'EOF';
#ifndef PCI_DEVICE_ID_INTEL_82801AA_3
#define PCI_DEVICE_ID_INTEL_82801AA_3  0x2413
#endif
#ifndef PCI_DEVICE_ID_INTEL_82801AB_3
#define PCI_DEVICE_ID_INTEL_82801AB_3  0x2423
#endif
EOF
        }
        print OUTPUT << 'EOF';

/* --> END OF COMPATIBILITY SECTION */

EOF
      } else {
        print OUTPUT;
      }
    }
    close INPUT;
    close OUTPUT;

    $kernel_file = $files{$package_file};
    print_diff $package_root,$kernel_root,$kernel_file,$temp;
  }

  gen_Makefile $package_root, $kernel_root;
  gen_drivers_Makefile $package_root, $kernel_root;
  gen_drivers_char_Config_in $package_root, $kernel_root;
  gen_drivers_char_mem_c $package_root, $kernel_root;
  gen_drivers_i2c_Config_in $package_root, $kernel_root;
  gen_drivers_i2c_Makefile $package_root, $kernel_root;
  gen_drivers_i2c_i2c_core_c $package_root, $kernel_root;
}

main;

