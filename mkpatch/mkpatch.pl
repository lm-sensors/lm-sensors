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

# Generate a diff between the old kernel file and the new lm_sensors file. We
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
  MAIN: while(<INPUT>) {
    if (m@I2C mainboard interfaces@ or
           m@Acer Labs ALI 1535@ or
           m@Acer Labs ALI 1533 and 1543C@ or
           m@AMD 756/766@ or
           m@Apple Hydra Mac I/O@ or
           m@Intel I801@ or
           m@Intel I810/I815 based Mainboard@ or
           m@Intel 82371AB PIIX4\(E\)@ or
           m@Silicon Integrated Systems Corp. SiS5595 based Mainboard@ or
           m@VIA Technologies, Inc. VT82C586B@ or
           m@VIA Technologies, Inc. VT82C596, 596B, 686A/B, 8233@ or
           m@3DFX Banshee / Voodoo3@ or
           m@DEC Tsunami 21272@ or
           m@Pseudo ISA adapter \(for hardware sensors modules\)@ or
           m@Analog Devices ADM1021 and compatibles@ or
           m@Analog Devices ADM1024@ or
           m@Analog Devices ADM1025@ or
           m@Analog Devices ADM9240 and compatibles@ or
           m@Dallas DS1621 and DS1625@ or
           m@Fujitsu-Siemens Poseidon@ or
           m@Fujitsu-Siemens Scylla@ or
           m@Genesys Logic GL518SM@ or
           m@Genesys Logic GL520SM@ or
           m@HP Maxilife@ or
           m@ITE 8705, 8712, Sis950@ or
           m@Myson MTP008@ or
           m@National Semiconductors LM75 and compatibles@ or
           m@National Semiconductors LM78@ or
           m@National Semiconductors LM80@ or
           m@National Semiconductors LM87@ or
           m@Silicon Integrated Systems Corp. SiS5595 Sensor@ or
           m@Texas Instruments THMC50 / Analog Devices ADM1022@ or
           m@Via VT82C686A/B@ or
           m@Winbond W83781D, W83782D, W83783S, W83627HF, AS99127F@ or
           m@EEprom \(DIMM\) reader@) {
      $_ = <INPUT>;
      $_ = <INPUT>;
      $_ = <INPUT> while not m@^\S@ and not eof(INPUT);
      redo MAIN;
    }
    if (eof(INPUT)) {
      print OUTPUT <<'EOF'
I2C mainboard interfaces
CONFIG_I2C_MAINBOARD
  Many modern mainboards have some kind of I2C interface integrated. This
  is often in the form of a SMBus, or System Management Bus, which is
  basically the same as I2C but which uses only a subset of the I2C
  protocol.

  You will also want the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Acer Labs ALI 1535
CONFIG_I2C_ALI1535
  If you say yes to this option, support will be included for the Acer
  Labs ALI 1535 mainboard I2C interface. This can also be 
  built as a module.

Acer Labs ALI 1533 and 1543C
CONFIG_I2C_ALI15X3
  If you say yes to this option, support will be included for the Acer
  Labs ALI 1533 and 1543C mainboard I2C interfaces. This can also be 
  built as a module which can be inserted and removed while the kernel
  is running.

AMD 756/766/768
CONFIG_I2C_AMD756
  If you say yes to this option, support will be included for the AMD
  756/766/768 mainboard I2C interfaces. This can also be 
  built as a module which can be inserted and removed while the kernel
  is running.

Apple Hydra Mac I/O
CONFIG_I2C_HYDRA
  If you say yes to this option, support will be included for the 
  Hydra mainboard I2C interface. This can also be built as a module 
  which can be inserted and removed while the kernel is running.

Intel I801
CONFIG_I2C_I801
  If you say yes to this option, support will be included for the 
  Intel I801 mainboard I2C interfaces. "I810" mainboard sensor chips are
  generally located on the I801's I2C bus. This can also be
  built as a module which can be inserted and removed while the kernel
  is running.

Intel I810/I815 based Mainboard
CONFIG_I2C_I810
  If you say yes to this option, support will be included for the 
  Intel I810/I815 mainboard I2C interfaces. The I2C busses these chips
  are generally used only for video devices. For "810" mainboard sensor
  chips, use the I801 I2C driver instead. This can also be
  built as a module which can be inserted and removed while the kernel
  is running.

Intel 82371AB PIIX4(E) / ServerWorks OSB4 and CSB5
CONFIG_I2C_PIIX4
  If you say yes to this option, support will be included for the 
  Intel PIIX4, PIIX4E, and 443MX, Serverworks OSB4/CSB5,
  and SMSC Victory66 mainboard
  I2C interfaces. This can also be
  built as a module which can be inserted and removed while the kernel
  is running.

Silicon Integrated Systems Corp. SiS5595 based Mainboard
CONFIG_I2C_SIS5595
  If you say yes to this option, support will be included for the 
  SiS5595 mainboard I2C interfaces. For integrated sensors on the
  Sis5595, use CONFIG_SENSORS_SIS5595. This can also be
  built as a module which can be inserted and removed while the kernel
  is running.

VIA Technologies, Inc. VT82C586B
CONFIG_I2C_VIA
  If you say yes to this option, support will be included for the VIA
  Technologies I2C adapter found on some motherboards. This can also 
  be built as a module which can be inserted and removed while the 
  kernel is running.

VIA Technologies, Inc. VT82C596, 596B, 686A/B, 8233
CONFIG_I2C_VIAPRO
  If you say yes to this option, support will be included for the VIA
  Technologies I2C adapter on these chips. For integrated sensors on the
  Via 686A/B, use CONFIG_SENSORS_VIA686A. This can also be
  be built as a module which can be inserted and removed while the 
  kernel is running.

3DFX Banshee / Voodoo3
CONFIG_I2C_VOODOO3
  If you say yes to this option, support will be included for the 
  3DFX Banshee and Voodoo3 I2C interfaces. The I2C busses on the these
  chips are generally used only for video devices.
  This can also be
  built as a module which can be inserted and removed while the kernel
  is running.

DEC Tsunami 21272
CONFIG_I2C_TSUNAMI
  If you say yes to this option, support will be included for the DEC
  Tsunami chipset I2C adapter. Requires the Alpha architecture;
  do not enable otherwise. This can also be built as a module which
  can be inserted and removed while the kernel is running.

Pseudo ISA adapter (for hardware sensors modules)
CONFIG_I2C_ISA
  This provides support for accessing some hardware sensor chips over
  the ISA bus rather than the I2C or SMBus. If you want to do this, 
  say yes here. This feature can also be built as a module which can 
  be inserted and removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Analog Devices ADM1021 and compatibles
CONFIG_SENSORS_ADM1021 
  If you say yes here you get support for Analog Devices ADM1021 
  and ADM1023 sensor chips and clones: Maxim MAX1617 and MAX1617A,
  Genesys Logic GL523SM, National Semi LM84, TI THMC10,
  and the XEON processor built-in sensor. This can also 
  be built as a module which can be inserted and removed while the 
  kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Analog Devices ADM1024
CONFIG_SENSORS_ADM1024
  If you say yes here you get support for Analog Devices ADM1024 sensor
  chips.  This can also be built as a module.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Analog Devices ADM1025
CONFIG_SENSORS_ADM1025
  If you say yes here you get support for Analog Devices ADM1025 sensor
  chips.  This can also be built as a module which can be inserted and
  removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Analog Devices ADM9240 and compatibles
CONFIG_SENSORS_ADM9240
  If you say yes here you get support for Analog Devices ADM9240 
  sensor chips and clones: the Dallas Semiconductor DS1780 and
  the National Semiconductor LM81. This can also be built as a 
  module which can be inserted and removed while the kernel is
  running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Dallas DS1621 and DS1625
CONFIG_SENSORS_DS1621
  If you say yes here you get support for the Dallas DS1621 and DS1625x
  sensor chips.  This can also be built as a module.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Fujitsu-Siemens Poseidon
CONFIG_SENSORS_FSCPOS
  If you say yes here you get support for the Fujitsu-Siemens Poseidon
  sensor chip.  This can also be built as a module.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Fujitsu-Siemens Scylla
CONFIG_SENSORS_FSCSCY
  If you say yes here you get support for the Fujitsu-Siemens Scylla
  sensor chip.  This can also be built as a module. This driver may/should
  also work with the following Fujitsu-Siemens chips: "Poseidon",
  "Poseidon II" and "Hydra". You may have to force loading of the module
  for motherboards in these cases. Be careful - those motherboards have
  not been tested with this driver.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Genesys Logic GL518SM
CONFIG_SENSORS_GL518SM
  If you say yes here you get support for Genesys Logic GL518SM sensor
  chips.  This can also be built as a module which can be inserted and
  removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Genesys Logic GL520SM
CONFIG_SENSORS_GL520SM
  If you say yes here you get support for Genesys Logic GL518SM sensor
  chips.  This can also be built as a module which can be inserted and
  removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

HP Maxilife
CONFIG_SENSORS_MAXILIFE
  If you say yes here you get support for the HP Maxilife
  sensor chip.  This can also be built as a module.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

ITE 8705, 8712, Sis950
CONFIG_SENSORS_IT87
  If you say yes here you get support for the ITE 8705 and 8712 and
  SiS950 sensor chips.  This can also be built as a module.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Myson MTP008
CONFIG_SENSORS_MTP008
  If you say yes here you get support for the Myson MTP008
  sensor chip.  This can also be built as a module.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

National Semiconductors LM75 and compatibles
CONFIG_SENSORS_LM75 
  If you say yes here you get support for National Semiconductor LM75
  sensor chips and clones: Dallas Semi DS75 and DS1775, TelCon
  TCN75, and National Semi LM77. This can also be built as a module which
  can be inserted and removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

National Semiconductors LM78
CONFIG_SENSORS_LM78
  If you say yes here you get support for National Semiconductor LM78
  sensor chips family: the LM78-J and LM79. Many clone chips will
  also work at least somewhat with this driver. This can also be built
  as a module which can be inserted and removed while the kernel is 
  running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

National Semiconductors LM80
CONFIG_SENSORS_LM80
  If you say yes here you get support for National Semiconductor LM80
  sensor chips. This can also be built as a module which can be 
  inserted and removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

National Semiconductors LM87
CONFIG_SENSORS_LM87
  If you say yes here you get support for National Semiconductor LM87
  sensor chips. This can also be built as a module which can be 
  inserted and removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Silicon Integrated Systems Corp. SiS5595 Sensor
CONFIG_SENSORS_SIS5595
  If you say yes here you get support for the integrated sensors in 
  SiS5595 South Bridges. This can also be built as a module 
  which can be inserted and removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Texas Instruments THMC50 / Analog Devices ADM1022
CONFIG_SENSORS_THMC50
  If you say yes here you get support for Texas Instruments THMC50
  sensor chips and clones: the Analog Devices ADM1022.
  This can also be built as a module which
  can be inserted and removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Via VT82C686A/B
CONFIG_SENSORS_VIA686A
  If you say yes here you get support for the integrated sensors in 
  Via 686A/B South Bridges. This can also be built as a module 
  which can be inserted and removed while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

Winbond W83781D, W83782D, W83783S, W83627HF, AS99127F
CONFIG_SENSORS_W83781D
  If you say yes here you get support for the Winbond W8378x series 
  of sensor chips: the W83781D, W83782D, W83783S and W83682HF,
  and the similar Asus AS99127F. This
  can also be built as a module which can be inserted and removed
  while the kernel is running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

EEprom (DIMM) reader
CONFIG_SENSORS_EEPROM
  If you say yes here you get read-only access to the EEPROM data 
  available on modern memory DIMMs, and which could theoretically
  also be available on other devices. This can also be built as a 
  module which can be inserted and removed while the kernel is 
  running.

  You will also need the latest user-space utilties: you can find them
  in the lm_sensors package, which you can download at 
  http://www.lm-sensors.nu

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
  my $type = 0;
  my $pr1 = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  `grep -q -s 'i2c\.o' "$kernel_root/$kernel_file"`;
  $type = 2 if ! $?;
  MAIN: while(<INPUT>) {
    $type = 1 if !$type and (m@^DRIVERS-\$@);
    if (m@DRIVERS-\$\(CONFIG_SENSORS\)@) {
      $_ = <INPUT>;
      redo MAIN;
    } elsif (m@CONFIG_SENSORS@) {
      $_ = <INPUT> while not m@endif@;
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
      redo MAIN;
    }
    if ($type == 1 and m@^DRIVERS \+= \$\(DRIVERS-y\)@) {
      print OUTPUT <<'EOF';
DRIVERS-$(CONFIG_SENSORS) += drivers/sensors/sensors.a
EOF
      $pr1 = 1;
    }
    if ($type == 2 and m@^DRIVERS .*= \$\(DRIVERS-y\)@) {
      print OUTPUT <<'EOF';
DRIVERS-$(CONFIG_SENSORS) += drivers/sensors/sensor.o
EOF
      $pr1 = 1;
    }
    if ($type == 0 and m@include arch/\$\(ARCH\)/Makefile@) {
      print OUTPUT <<'EOF';
ifeq ($(CONFIG_SENSORS),y)
DRIVERS := $(DRIVERS) drivers/sensors/sensors.a
endif

EOF
      $pr1 = 1;
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  die "Automatic patch generation for `Makefile' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $pr1 == 0;
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
  my $pr1 = 0;
  my $pr2 = 0;
  my $new_style = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  MAIN: while(<INPUT>) {
    if (m@^mod-subdirs\s*:=@) {
      $new_style = 1;
    }
    if ((! $new_style and m@^ALL_SUB_DIRS\s*:=@) or m@^mod-subdirs\s*:=@) {
      $pr1 = 1;
      $sensors_present = 0;
      while (m@\\$@) {
        $sensors_present = 1 if m@sensors@;
        print OUTPUT;
        $_ = <INPUT>;
      }
      $sensors_present = 1 if m@sensors@;
      s@$@ sensors@ if (not $sensors_present);
      print OUTPUT;
      $_ = <INPUT>;
      redo MAIN;
    } 
    if (m@^ifeq.*CONFIG_SENSORS@) {
      $_ = <INPUT> while not m@^endif@;
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
      redo MAIN;
    } 
    if (m@^subdir.*CONFIG_SENSORS@) {
      $_ = <INPUT>;
      redo MAIN;
    }
    if (!$pr2 and (m@^include \$\(TOPDIR\)/Rules.make$@ or m@^subdir-\$\(CONFIG_ACPI@)) {
      $pr2 = 1;
      if ($new_style) {
      print OUTPUT <<'EOF';
subdir-$(CONFIG_SENSORS) 	+= sensors
EOF
      } else {
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
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  die "Automatic patch generation for `drivers/Makefile' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $pr1 == 0 or $pr2 == 0;
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
  my $pr1 = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  MAIN: while(<INPUT>) {
    if (m@source drivers/i2c/Config.in@) {
      $pr1 = 1;
      print OUTPUT;
      print OUTPUT "\nsource drivers/sensors/Config.in\n";
      $_ = <INPUT>;
      redo MAIN;
    }
    if (m@sensors@) {
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
      redo MAIN;
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  die "Automatic patch generation for `drivers/Makefile' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $pr1 == 0;
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
  my $pr1 = 0;
  my $pr2 = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  MAIN: while(<INPUT>) {
    if ($atstart and m@#ifdef@) {
      $pr1 = 1;
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
      redo MAIN;
    }
    $right_place = 1 if (m@lp_m68k_init\(\);@);
    if ($right_place and not $done and
        m@CONFIG_SENSORS@) {
      $_ = <INPUT> while not m@#endif@;
      $_ = <INPUT>;
      $_ = <INPUT> if m@^$@;
      redo MAIN;
    }
    if ($right_place and not $done and m@return 0;@) {
      $pr2 = 1;
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
  die "Automatic patch generation for `drivers/char/mem.c' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $pr1 == 0 or $pr2 == 0;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}


# This generates diffs for drivers/i2c/Config.in
# Several adapter drivers that are included in the lm_sensors package are
# added at the first and onlu sensors marker.
# Of course, care is taken old lines are removed.
# $_[0]: sensors package root (like /tmp/sensors)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_drivers_i2c_Config_in
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/i2c/Config.in";
  my $package_file = "$temp";
  my $pr1 = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if (m@sensors code starts here@) {
      $pr1++;
      print OUTPUT;
      while (<INPUT>) {
        last if m@sensors code ends here@;
      }
      print OUTPUT << 'EOF';
  bool 'I2C mainboard interfaces' CONFIG_I2C_MAINBOARD 
  if [ "$CONFIG_I2C_MAINBOARD" = "y" ]; then
    tristate '  Acer Labs ALI 1535' CONFIG_I2C_ALI1535 
    tristate '  Acer Labs ALI 1533 and 1543C' CONFIG_I2C_ALI15X3 
    dep_tristate '  Apple Hydra Mac I/O' CONFIG_I2C_HYDRA $CONFIG_I2C_ALGOBIT
    tristate '  AMD 756/766/768' CONFIG_I2C_AMD756
    dep_tristate '  DEC Tsunami I2C interface' CONFIG_I2C_TSUNAMI $CONFIG_I2C_ALGOBIT
    tristate '  Intel 82801AA, 82801AB and 82801BA' CONFIG_I2C_I801
    dep_tristate '  Intel i810AA/AB/E and i815' CONFIG_I2C_I810 $CONFIG_I2C_ALGOBIT
    tristate '  Intel 82371AB PIIX4(E), 443MX, ServerWorks OSB4/CSB5, SMSC Victory66' CONFIG_I2C_PIIX4
    tristate '  SiS 5595' CONFIG_I2C_SIS5595
    dep_tristate '  VIA Technologies, Inc. VT82C586B' CONFIG_I2C_VIA $CONFIG_I2C_ALGOBIT
    tristate '  VIA Technologies, Inc. VT596A/B, 686A/B, 8233' CONFIG_I2C_VIAPRO
    dep_tristate '  Voodoo3 I2C interface' CONFIG_I2C_VOODOO3 $CONFIG_I2C_ALGOBIT
    tristate '  Pseudo ISA adapter (for some hardware sensors)' CONFIG_I2C_ISA 
  fi

EOF
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  die "Automatic patch generation for `drivers/i2c/Config.in' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $pr1 != 1;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}

sub gen_drivers_sensors_Makefile
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "drivers/sensors/Makefile";
  my $package_file = $temp;
  my $use_new_format;
  `grep -q -s 'i2c\.o' "$kernel_root/drivers/i2c/Makefile"`;
  $use_new_format = ! $?;

  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  if ($use_new_format) {
    print OUTPUT <<'EOF';
#
# Makefile for the kernel hardware sensors drivers.
#

MOD_LIST_NAME := SENSORS_MODULES
O_TARGET := sensor.o

export-objs	:= sensors.o

obj-$(CONFIG_SENSORS)		+= sensors.o
obj-$(CONFIG_SENSORS_ADM1021)	+= adm1021.o
obj-$(CONFIG_SENSORS_ADM1024)	+= adm1024.o
obj-$(CONFIG_SENSORS_ADM1025)	+= adm1025.o
obj-$(CONFIG_SENSORS_ADM9240)	+= adm9240.o
obj-$(CONFIG_SENSORS_BT869)	+= bt869.o
obj-$(CONFIG_SENSORS_DDCMON)	+= ddcmon.o
obj-$(CONFIG_SENSORS_DS1621)	+= ds1621.o
obj-$(CONFIG_SENSORS_EEPROM)	+= eeprom.o
obj-$(CONFIG_SENSORS_FSCPOS)	+= fscpos.o
obj-$(CONFIG_SENSORS_FSCSCY)	+= fscscy.o
obj-$(CONFIG_SENSORS_GL518SM)	+= gl518sm.o
obj-$(CONFIG_SENSORS_GL520SM)	+= gl520sm.o
obj-$(CONFIG_SENSORS_IT87)	+= it87.o
obj-$(CONFIG_SENSORS_LM75)	+= lm75.o
obj-$(CONFIG_SENSORS_LM78)	+= lm78.o
obj-$(CONFIG_SENSORS_LM80)	+= lm80.o
obj-$(CONFIG_SENSORS_LM87)	+= lm87.o
obj-$(CONFIG_SENSORS_MAXILIFE)	+= maxilife.o
obj-$(CONFIG_SENSORS_MTP008)	+= mtp008.o
obj-$(CONFIG_SENSORS_SIS5595)	+= sis5595.o
obj-$(CONFIG_SENSORS_THMC50)	+= thmc50.o
obj-$(CONFIG_SENSORS_VIA686A)	+= via686a.o
obj-$(CONFIG_SENSORS_W83781D)	+= w83781d.o

include $(TOPDIR)/Rules.make

EOF
  } else {
    print OUTPUT <<'EOF';
#
# Makefile for the kernel hardware sensors drivers.
#

SUB_DIRS     :=
MOD_SUB_DIRS := $(SUB_DIRS)
ALL_SUB_DIRS := $(SUB_DIRS)
MOD_LIST_NAME := SENSORS_MODULES

L_TARGET := sensors.a
MX_OBJS :=  
M_OBJS  := 
LX_OBJS :=
L_OBJS  := 

# -----
# i2c core components
# -----

ifeq ($(CONFIG_SENSORS),y)
  LX_OBJS += sensors.o
else
  ifeq ($(CONFIG_SENSORS),m)
    MX_OBJS += sensors.o
  endif
endif

ifeq ($(CONFIG_SENSORS_ADM1021),y)
  L_OBJS += adm1021.o
else
  ifeq ($(CONFIG_SENSORS_ADM1021),m)
    M_OBJS += adm1021.o
  endif
endif

ifeq ($(CONFIG_SENSORS_ADM1024),y)
  L_OBJS += adm1024.o
else
  ifeq ($(CONFIG_SENSORS_ADM1024),m)
    M_OBJS += adm1024.o
  endif
endif

ifeq ($(CONFIG_SENSORS_ADM1025),y)
  L_OBJS += adm1025.o
else
  ifeq ($(CONFIG_SENSORS_ADM1025),m)
    M_OBJS += adm1025.o
  endif
endif

ifeq ($(CONFIG_SENSORS_ADM9240),y)
  L_OBJS += adm9240.o
else
  ifeq ($(CONFIG_SENSORS_ADM9240),m)
    M_OBJS += adm9240.o
  endif
endif

ifeq ($(CONFIG_SENSORS_DDCMON),y)
  L_OBJS += ddcmon.o
else
  ifeq ($(CONFIG_SENSORS_DDCMON),m)
    M_OBJS += ddcmon.o
  endif
endif

ifeq ($(CONFIG_SENSORS_DS1621),y)
  L_OBJS += ds1621.o
else
  ifeq ($(CONFIG_SENSORS_DS1621),m)
    M_OBJS += ds1621.o
  endif
endif

ifeq ($(CONFIG_SENSORS_EEPROM),y)
  L_OBJS += eeprom.o
else
  ifeq ($(CONFIG_SENSORS_EEPROM),m)
    M_OBJS += eeprom.o
  endif
endif

ifeq ($(CONFIG_SENSORS_FSCPOS),y)
  L_OBJS += fscpos.o
else
  ifeq ($(CONFIG_SENSORS_FSCPOS),m)
    M_OBJS += fscpos.o
  endif
endif

ifeq ($(CONFIG_SENSORS_FSCSCY),y)
  L_OBJS += fscscy.o
else
  ifeq ($(CONFIG_SENSORS_FSCSCY),m)
    M_OBJS += fscscy.o
  endif
endif

ifeq ($(CONFIG_SENSORS_GL518SM),y)
  L_OBJS += gl518sm.o
else
  ifeq ($(CONFIG_SENSORS_GL518SM),m)
    M_OBJS += gl518sm.o
  endif
endif

ifeq ($(CONFIG_SENSORS_GL520SM),y)
  L_OBJS += gl520sm.o
else
  ifeq ($(CONFIG_SENSORS_GL520SM),m)
    M_OBJS += gl520sm.o
  endif
endif

ifeq ($(CONFIG_SENSORS_IT87),y)
  L_OBJS += it87.o
else
  ifeq ($(CONFIG_SENSORS_IT87),m)
    M_OBJS += it87.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LM75),y)
  L_OBJS += lm75.o
else
  ifeq ($(CONFIG_SENSORS_LM75),m)
    M_OBJS += lm75.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LM78),y)
  L_OBJS += lm78.o
else
  ifeq ($(CONFIG_SENSORS_LM78),m)
    M_OBJS += lm78.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LM80),y)
  L_OBJS += lm80.o
else
  ifeq ($(CONFIG_SENSORS_LM80),m)
    M_OBJS += lm80.o
  endif
endif

ifeq ($(CONFIG_SENSORS_LM87),y)
  L_OBJS += lm87.o
else
  ifeq ($(CONFIG_SENSORS_LM87),m)
    M_OBJS += lm87.o
  endif
endif

ifeq ($(CONFIG_SENSORS_MATORB),y)
  L_OBJS += matorb.o
else
  ifeq ($(CONFIG_SENSORS_MATORB),m)
    M_OBJS += matorb.o
  endif
endif

ifeq ($(CONFIG_SENSORS_MAXILIFE),y)
  L_OBJS += maxilife.o
else
  ifeq ($(CONFIG_SENSORS_MAXILIFE),m)
    M_OBJS += maxilife.o
  endif
endif

ifeq ($(CONFIG_SENSORS_MTP008),y)
  L_OBJS += mtp008.o
else
  ifeq ($(CONFIG_SENSORS_MTP008),m)
    M_OBJS += mtp008.o
  endif
endif

ifeq ($(CONFIG_SENSORS_SIS5595),y)
  L_OBJS += sis5595.o
else
  ifeq ($(CONFIG_SENSORS_SIS5595),m)
    M_OBJS += sis5595.o
  endif
endif

ifeq ($(CONFIG_SENSORS_THMC50),y)
  L_OBJS += thmc50.o
else
  ifeq ($(CONFIG_SENSORS_THMC50),m)
    M_OBJS += thmc50.o
  endif
endif

ifeq ($(CONFIG_SENSORS_VIA686A),y)
  L_OBJS += via686a.o
else
  ifeq ($(CONFIG_SENSORS_VIA686A),m)
    M_OBJS += via686a.o
  endif
endif

ifeq ($(CONFIG_SENSORS_W83781D),y)
  L_OBJS += w83781d.o
else
  ifeq ($(CONFIG_SENSORS_W83781D),m)
    M_OBJS += w83781d.o
  endif
endif

include $(TOPDIR)/Rules.make
EOF
  }  
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
  my $pr1 = 0;
  my $new_format = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    $new_format = 1 if m@i2c\.o@;
    if (m@sensors code starts here@) {
      $pr1 ++;
      print OUTPUT;
      while (<INPUT>) {
        last if m@sensors code ends here@;
      }
      if ($new_format) {
        print OUTPUT << 'EOF';
obj-$(CONFIG_I2C_ALI1535)		+= i2c-ali1535.o
obj-$(CONFIG_I2C_ALI15X3)		+= i2c-ali15x3.o
obj-$(CONFIG_I2C_AMD756)		+= i2c-amd756.o
obj-$(CONFIG_I2C_HYDRA)			+= i2c-hydra.o
obj-$(CONFIG_I2C_I801)			+= i2c-i801.o
obj-$(CONFIG_I2C_I810)			+= i2c-i810.o
obj-$(CONFIG_I2C_ISA)			+= i2c-isa.o
obj-$(CONFIG_I2C_PIIX4)			+= i2c-piix4.o
obj-$(CONFIG_I2C_SIS5595)		+= i2c-sis5595.o
obj-$(CONFIG_I2C_TSUNAMI)		+= i2c-tsunami.o
obj-$(CONFIG_I2C_VIA)			+= i2c-via.o
obj-$(CONFIG_I2C_VIAPRO)		+= i2c-viapro.o
obj-$(CONFIG_I2C_VOODOO3)		+= i2c-voodoo3.o
EOF
      } else {
        print OUTPUT << 'EOF';
ifeq ($(CONFIG_I2C_ALI1535),y)
  L_OBJS += i2c-ali1535.o
else 
  ifeq ($(CONFIG_I2C_ALI1535),m)
    M_OBJS += i2c-ali1535.o
  endif
endif

ifeq ($(CONFIG_I2C_ALI15X3),y)
  L_OBJS += i2c-ali15x3.o
else 
  ifeq ($(CONFIG_I2C_ALI15X3),m)
    M_OBJS += i2c-ali15x3.o
  endif
endif

ifeq ($(CONFIG_I2C_AMD756),y)
  L_OBJS += i2c-amd756.o
else 
  ifeq ($(CONFIG_I2C_AMD756),m)
    M_OBJS += i2c-amd756.o
  endif
endif

ifeq ($(CONFIG_I2C_HYDRA),y)
  L_OBJS += i2c-hydra.o
else 
  ifeq ($(CONFIG_I2C_HYDRA),m)
    M_OBJS += i2c-hydra.o
  endif
endif

ifeq ($(CONFIG_I2C_I801),y)
  L_OBJS += i2c-i801.o
else 
  ifeq ($(CONFIG_I2C_I801),m)
    M_OBJS += i2c-i801.o
  endif
endif

ifeq ($(CONFIG_I2C_I810),y)
  L_OBJS += i2c-i810.o
else 
  ifeq ($(CONFIG_I2C_I810),m)
    M_OBJS += i2c-i810.o
  endif
endif

ifeq ($(CONFIG_I2C_ISA),y)
  L_OBJS += i2c-isa.o
else 
  ifeq ($(CONFIG_I2C_ISA),m)
    M_OBJS += i2c-isa.o
  endif
endif

ifeq ($(CONFIG_I2C_PIIX4),y)
  L_OBJS += i2c-piix4.o
else 
  ifeq ($(CONFIG_I2C_PIIX4),m)
    M_OBJS += i2c-piix4.o
  endif
endif

ifeq ($(CONFIG_I2C_SIS5595),y)
  L_OBJS += i2c-sis5595.o
else 
  ifeq ($(CONFIG_I2C_SIS5595),m)
    M_OBJS += i2c-sis5595.o
  endif
endif

ifeq ($(CONFIG_I2C_TSUNAMI),y)
  L_OBJS += i2c-tsunami.o
else 
  ifeq ($(CONFIG_I2C_TSUNAMI),m)
    M_OBJS += i2c-tsunami.o
  endif
endif

ifeq ($(CONFIG_I2C_VIA),y)
  L_OBJS += i2c-via.o
else 
  ifeq ($(CONFIG_I2C_VIA),m)
    M_OBJS += i2c-via.o
  endif
endif

ifeq ($(CONFIG_I2C_VIAPRO),y)
  L_OBJS += i2c-viapro.o
else 
  ifeq ($(CONFIG_I2C_VIAPRO),m)
    M_OBJS += i2c-viapro.o
  endif
endif

ifeq ($(CONFIG_I2C_VOODOO3),y)
  L_OBJS += i2c-voodoo3.o
else 
  ifeq ($(CONFIG_I2C_VOODOO3),m)
    M_OBJS += i2c-voodoo3.o
  endif
endif

EOF
      }
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  die "Automatic patch generation for `drivers/i2c/Makefile' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $pr1 != 1;
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
  my $patch_nr = 1;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  while(<INPUT>) {
    if (m@sensors code starts here@) {
      print OUTPUT;
      while (<INPUT>) {
        last if m@sensors code ends here@;
      }
      if ($patch_nr == 1) {
        print OUTPUT << 'EOF';
#ifdef CONFIG_I2C_ALI1535
	extern int i2c_ali1535_init(void);
#endif
#ifdef CONFIG_I2C_ALI15X3
	extern int i2c_ali15x3_init(void);
#endif
#ifdef CONFIG_I2C_AMD756
	extern int i2c_amd756_init(void);
#endif
#ifdef CONFIG_I2C_HYDRA
	extern int i2c_hydra_init(void);
#endif
#ifdef CONFIG_I2C_I801
	extern int i2c_i801_init(void);
#endif
#ifdef CONFIG_I2C_I810
	extern int i2c_i810_init(void);
#endif
#ifdef CONFIG_I2C_ISA
	extern int i2c_isa_init(void);
#endif
#ifdef CONFIG_I2C_PIIX4
	extern int i2c_piix4_init(void);
#endif
#ifdef CONFIG_I2C_SIS5595
	extern int i2c_sis5595_init(void);
#endif
#ifdef CONFIG_I2C_TSUNAMI
	extern int i2c_tsunami_init(void);
#endif
#ifdef CONFIG_I2C_VIA
	extern int i2c_via_init(void);
#endif
#ifdef CONFIG_I2C_VIAPRO
	extern int i2c_vt596_init(void);
#endif
#ifdef CONFIG_I2C_VOODOO3
	extern int i2c_voodoo3_init(void);
#endif
EOF
      } elsif ($patch_nr == 2) {
      print OUTPUT << 'EOF';
#ifdef CONFIG_I2C_ALI1535
	i2c_ali1535_init();
#endif
#ifdef CONFIG_I2C_ALI15X3
	i2c_ali15x3_init();
#endif
#ifdef CONFIG_I2C_AMD756
	i2c_amd756_init();
#endif
#ifdef CONFIG_I2C_HYDRA
	i2c_hydra_init();
#endif
#ifdef CONFIG_I2C_I801
	i2c_i801_init();
#endif
#ifdef CONFIG_I2C_I810
	i2c_i810_init();
#endif
#ifdef CONFIG_I2C_PIIX4
	i2c_piix4_init();
#endif
#ifdef CONFIG_I2C_SIS5595
	i2c_sis5595_init();
#endif
#ifdef CONFIG_I2C_TSUNAMI
	i2c_tsunami_init();
#endif
#ifdef CONFIG_I2C_VIA
	i2c_via_init();
#endif
#ifdef CONFIG_I2C_VIAPRO
	i2c_vt596_init();
#endif
#ifdef CONFIG_I2C_VOODOO3
	i2c_voodoo3_init();
#endif
#ifdef CONFIG_I2C_ISA
	i2c_isa_init();
#endif
EOF
      }
      $patch_nr ++;
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  die "Automatic patch generation for `drivers/i2c/i2c-core.c' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $patch_nr != 3;
  print_diff $package_root,$kernel_root,$kernel_file,$package_file;
}

# Generate the diffs for the list of MAINTAINERS
# $_[0]: i2c package root (like /tmp/i2c)
# $_[1]: Linux kernel tree (like /usr/src/linux)
sub gen_MAINTAINERS
{
  my ($package_root,$kernel_root) = @_;
  my $kernel_file = "MAINTAINERS";
  my $package_file = $temp;
  my $done = 0;

  open INPUT,"$kernel_root/$kernel_file"
        or die "Can't open `$kernel_root/$kernel_file'";
  open OUTPUT,">$package_root/$package_file"
        or die "Can't open $package_root/$package_file";
  MAIN: while(<INPUT>) {
    if (m@SENSORS DRIVERS@) {
       $_=<INPUT> while not m@^$@;
       $_=<INPUT>;
       redo MAIN;
    }
    if (not $done and (m@SGI VISUAL WORKSTATION 320 AND 540@)) {
      print OUTPUT <<'EOF';
SENSORS DRIVERS
P:      Frodo Looijaard
M:      frodol@dds.nl
P:      Philip Edelbrock
M:      phil@netroedge.com
L:      sensors@stimpy.netroedge.com
W:      http://www.lm-sensors.nu/
S:      Maintained

EOF
      $done = 1;
    }
    print OUTPUT;
  }
  close INPUT;
  close OUTPUT;
  die "Automatic patch generation for `MAINTAINERS' failed.\n".
      "See our home page http://www.lm-sensors.nu for assistance!" if $done == 0;
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

  die "First apply the i2c patches to `$kernel_root'!" 
       if ! -d "$kernel_root/drivers/i2c";

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
  gen_drivers_sensors_Makefile $package_root, $kernel_root;
  gen_drivers_char_Config_in $package_root, $kernel_root;
  gen_drivers_char_mem_c $package_root, $kernel_root;
  gen_drivers_i2c_Config_in $package_root, $kernel_root;
  gen_drivers_i2c_Makefile $package_root, $kernel_root;
  gen_drivers_i2c_i2c_core_c $package_root, $kernel_root;
  gen_Documentation_Configure_help $package_root, $kernel_root;
  gen_MAINTAINERS $package_root, $kernel_root;
}

main;

