#!/usr/bin/perl

#
#    detect.pl - Detect PCI bus and chips
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


# A Perl wizard really ought to look upon this; the PCI and I2C stuff should
# each be put in a separate file, using modules and packages. That is beyond
# me.

use strict;

#########################
# CONSTANT DECLARATIONS #
#########################

use vars qw(@pci_adapters @chip_ids @undetectable_adapters);

@undetectable_adapters = ( "bit-lp", "bit-velle" );

# This is the list of SMBus or I2C adapters we recognize by their PCI
# signature. This is an easy and fast way to determine which SMBus or I2C
# adapters should be present.
# Each entry must have a vindid (Vendor ID), devid (Device ID), func (PCI
# Function) and procid (string as appears in /proc/pci; see linux/driver/pci,
# either pci.c or oldproc.c). If no driver is written yet, omit the 
# driver (Driver Name) field.
@pci_adapters = ( 
     { 
       vendid => 0x8086,
       devid  => 0x7113,
       func => 3,
       procid => "Intel 82371AB PIIX4 ACPI",
       driver => "i2c-piix4"
     } , 
     { 
       vendid => 0x1106,
       devid  => 0x3040,
       func => 3,
       procid => "VIA Technologies VT 82C586B Apollo ACPI",
       driver => "i2c-via"
     } ,
     {
       vendid => 0x1039,
       devid  => 0x0008,
       func => 0,
       procid => "Silicon Integrated Systems 85C503",
     } ,
     {
       vendid => 0x10b9,
       devid => 0x7101,
       funcid => 0,
       procid => "Acer Labs M7101",
       driver => "i2c-ali15x3"
     }
);

use subs qw(lm78_detect lm75_detect lm80_detect w83781d_detect
            gl518sm_detect gl520sm_detect adm9240_detect adm1021_detect);

# This is a list of all recognized chips. 
# Each entry must have a name (Full Chip Name), an array i2c_addrs (Valid
# I2C Addresses; may be omitted if this is a pure ISA chip), an array 
# isa_addrs (Valid ISA Addresses; may be omitted if this is a pure I2C chip),
# i2c_detect (I2C Detetction Routine, may be omitted if this is a pure ISA
# chip), ...
# If no driver is written yet, omit the driver (Driver Name) field.
@chip_ids = (
     {
       name => "National Semiconductors LM78",
       driver => "lm78",
       i2c_addrs => [0x00..0x7f], 
       i2c_detect => sub { lm78_detect 0, @_},
       isa_addrs => [0x290],  # Theoretically anyway, but this will do
     } ,
     {
       name => "National Semiconductors LM78-J",
       driver => "lm78",
       i2c_addrs => [0x00..0x7f],
       i2c_detect => sub { lm78_detect 1, @_ },
       isa_addrs => [0x290],  # Theoretically anyway, but this will do
     } ,
     {
       name => "National Semiconductors LM79",
       driver => "lm78",
       i2c_addrs => [0x00..0x7f],
       i2c_detect => sub { lm78_detect 2, @_ },
       isa_addrs => [0x290],  # Theoretically anyway, but this will do
     } ,
     {
       name => "National Semiconductors LM75",
       driver => "lm75",
       i2c_addrs => [0x48..0x4f],
       i2c_detect => sub { lm75_detect @_},
     } ,
     {
       name => "National Semiconductors LM80",
       driver => "lm80",
       i2c_addrs => [0x28..0x2f],
       i2c_detect => sub { lm80_detect @_} ,
     },
     {
       name => "Winbond W83781D",
       driver => "w83781d",
       i2c_addrs => [0x00..0x7f], 
       i2c_detect => sub { w83781d_detect 0, @_},
       isa_addrs => [0x290],  # Theoretically anyway, but this will do
     } ,
     {
       name => "Genesys Logic GL518SM Revision 0x00",
       driver => "gl518sm",
       i2c_addrs => [0x4c, 0x4d],
       i2c_detect => sub { gl518sm_detect 0, @_} ,
     },
     {
       name => "Genesys Logic GL518SM Revision 0x80",
       driver => "gl518sm",
       i2c_addrs => [0x4c, 0x4d],
       i2c_detect => sub { gl518sm_detect 1, @_} ,
     },
     {
       name => "Genesys Logic GL520SM",
       i2c_addrs => [0x4c, 0x4d],
       i2c_detect => sub { gl520sm_detect 1, @_} ,
     },
     {
       name => "Analog Devices ADM9240",
       driver => "adm9240",
       i2c_addrs => [0x2c..0x2f],
       i2c_detect => sub { adm9240_detect @_ }
     },
     {
       name => "Analog Devices ADM1021",
       driver => "adm9240",
       i2c_addrs => [0x18..0x1b,0x29..0x2b,0x4c..0x4e],
       i2c_detect => sub { adm1021_detect 0, @_ },
     },
     {
       name => "Maxim MAX1617",
       driver => "adm9240",
       i2c_addrs => [0x18..0x1b,0x29..0x2b,0x4c..0x4e],
       i2c_detect => sub { adm1021_detect 1, @_ },
     },
);


#######################
# AUXILIARY FUNCTIONS #
#######################

sub swap_bytes
{
  return (($_[0] & 0xff00) >> 8) + (($_[0] & 0x00ff) << 8)
}

# $_[0] is the sought value
# @_[1..] is the list to seek in
# Returns: 0 on failure, 1 if found.
sub contains
{
  my $sought = shift;
  foreach (@_) {
    return 1 if $sought eq $_;
  }
  return 0;
}


###########
# MODULES #
###########

use vars qw(@modules_list);

sub initialize_modules_list
{
  open INPUTFILE, "/proc/modules" or die "Can't access /proc/modules!";
  while (<INPUTFILE>) {
    push @modules_list, /^(\S*)/ ;
  }
  close INPUTFILE;
}

##############
# PCI ACCESS #
##############

use vars qw(@pci_list);

# This function returns a list of hashes. Each hash has some PCI information 
# (more than we will ever need, probably). The most important
# fields are 'bus', 'slot', 'func' (they uniquely identify a PCI device in 
# a computer) and 'vendid','devid' (they uniquely identify a type of device).
# /proc/bus/pci/devices is only available on late 2.1 and 2.2 kernels.
sub read_proc_dev_pci
{
  my ($dfn,$vend,@pci_list);
  open INPUTFILE, "/proc/bus/pci/devices" or return;
  while (<INPUTFILE>) {
    my $record = {};
    ($dfn,$vend,$record->{irq},$record->{base_addr0},$record->{base_addr1},
          $record->{base_addr2},$record->{base_addr3},$record->{base_addr4},
          $record->{base_addr5},$record->{rom_base_addr}) = 
          map { oct "0x$_" } split;
    $record->{bus} = $dfn >> 8;
    $record->{slot} = ($dfn & 0xf8) >> 3;
    $record->{func} = $dfn & 0x07;
    $record->{vendid} = $vend >> 16;
    $record->{devid} = $vend & 0xffff;
  push @pci_list,$record;
  }
  close INPUTFILE or return;
  return @pci_list;
}

# This function returns a list of hashes. Each hash has some PCI 
# information. The important fields here are 'bus', 'slot', 'func' (they
# uniquely identify a PCI device in a computer) and 'desc' (a functional
# description of the PCI device). If this is an 'unknown device', the
# vendid and devid fields are set instead.
sub read_proc_pci
{
  my @pci_list;
  open INPUTFILE, "/proc/pci" or return;
  while (<INPUTFILE>) {
    my $record = {};
    if (($record->{bus},$record->{slot},$record->{func}) = 
        /^\s*Bus\s*(\S)+\s*,\s*device\s*(\S+)\s*,\s*function\s*(\S+)\s*:\s*$/) {
      my $desc = <INPUTFILE>;
      unless (($desc =~ /Unknown device/) and
              (($record->{vendid},$record->{devid}) = 
                         /^\s*Vendor id=(\S+)\.\s*Device id=(\S+)\.$/)) {
        $record->{desc} = $desc;
      }
      push @pci_list,$record;
    }
  }
  close INPUTFILE or return;
  return @pci_list;
}

sub initialize_proc_pci
{
  @pci_list = read_proc_dev_pci;
  @pci_list = read_proc_pci     if not defined @pci_list;
  die "Can't access either /proc/bus/pci/ or /proc/pci!" 
                                    if not defined @pci_list;
}

#####################
# ADAPTER DETECTION #
#####################

sub all_available_adapters
{
  my @res = ();
  my ($module,$adapter);
  MODULES:
  foreach $module (@modules_list) {
    foreach $adapter (@pci_adapters) {
      if (exists $adapter->{driver} and $module eq $adapter->{driver}) {
        push @res, $module;
        next MODULES;
      }
    }
  }
  return @res;
}

sub adapter_pci_detection
{
  my ($device,$try,@res);
  print "Probing for PCI bus adapters...\n";

  foreach $device (@pci_list) {
    foreach $try (@pci_adapters) {
      if ((defined($device->{vendid}) and 
           $try->{vendid} == $device->{vendid} and
           $try->{devid} == $device->{devid} and
           $try->{func} == $device->{func}) or
          (! defined($device->{vendid}) and
           $device->{desc} =~ /$try->{procid}/ and
           $try->{func} == $device->{func})) {
        printf "Use driver `%s' for device %02x:%02x.%x: %s\n",
               $try->{driver}?$try->{driver}:"<To Be Written>",
               $device->{bus},$device->{slot},$device->{func},$try->{procid};
        push @res,$try->{driver};
      }
    }
  }
  if (! defined @res) {
    print ("Sorry, no PCI bus adapters found.\n");
  } else {
    printf ("Probe succesfully concluded.\n");
  }
  return @res;
}

#############################
# I2C AND SMBUS /DEV ACCESS #
#############################

# This should really go into a separate module/package.

# To do: support i2c-level access (through sysread/syswrite, probably).
# I can't test this at all (PIIX4 does not support this), so I have not
# included it.

use vars qw($IOCTL_I2C_RETRIES $IOCTL_I2C_TIMEOUT $IOCTL_I2C_UDELAY 
            $IOCTL_I2C_MDELAY $IOCTL_I2C_SLAVE $IOCTL_I2C_TENBIT 
            $IOCTL_I2C_SMBUS);

# These are copied from <linux/i2c.h> and <linux/smbus.h>

# For bit-adapters:
$IOCTL_I2C_RETRIES = 0x0701;
$IOCTL_I2C_TIMEOUT = 0x0702;
$IOCTL_I2C_UDELAY = 0x0705;
$IOCTL_I2C_MDELAY = 0x0706;

# General ones:
$IOCTL_I2C_SLAVE = 0x0703;
$IOCTL_I2C_TENBIT = 0x0704;
$IOCTL_I2C_SMBUS = 0x0720;


use vars qw($SMBUS_READ $SMBUS_WRITE $SMBUS_QUICK $SMBUS_BYTE $SMBUS_BYTE_DATA 
            $SMBUS_WORD_DATA $SMBUS_PROC_CALL $SMBUS_BLOCK_DATA);

# These are copied from <linux/smbus.h>

$SMBUS_READ = 1;
$SMBUS_WRITE = 0;
$SMBUS_QUICK = 0;
$SMBUS_BYTE = 1;
$SMBUS_BYTE_DATA  = 2;
$SMBUS_WORD_DATA  = 3;
$SMBUS_PROC_CALL = 4;
$SMBUS_BLOCK_DATA = 5;

# Select the device to communicate with through its address.
# $_[0]: Reference to an opened filehandle
# $_[1]: Address to select
# Returns: 0 on failure, 1 on success.
sub i2c_set_slave_addr
{
  my ($file,$addr) = @_;
  ioctl $file, $IOCTL_I2C_SLAVE, $addr or return 0;
  return 1;
}

# i2c_smbus_access is based upon the corresponding C function (see 
# <linux/i2c-dev.h>). You should not need to call this directly.
# Exact calling conventions are intricate; read i2c-dev.c if you really need
# to know.
# $_[0]: Reference to an opened filehandle
# $_[1]: $SMBUS_READ for reading, $SMBUS_WRITE for writing
# $_[2]: Command (usually register number)
# $_[3]: Transaction kind ($SMBUS_BYTE, $SMBUS_BYTE_DATA, etc.)
# $_[4]: Reference to an array used for input/output of data
# Returns: 0 on failure, 1 on success.
# Note that we need to get back to Integer boundaries through the 'x2'
# in the pack. This is very compiler-dependent; I wish there was some other 
# way to do this.
sub i2c_smbus_access
{
  my ($file,$read_write,$command,$size,$data) = @_;
  my $data_array = pack "C32", @$data;
  my $ioctl_data = pack "C2x2Ip", ($read_write,$command,$size,$data_array);
  ioctl $file, $IOCTL_I2C_SMBUS, $ioctl_data or return 0;
  $_[4] = [ unpack "C32",$data_array ];
  return 1;
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Either 0 or 1
# Returns: -1 on failure, the 0 on success.
sub i2c_smbus_write_quick
{
  my ($file,$value) = @_;
  my $data = [];
  i2c_smbus_access $file, $value, 0, $SMBUS_QUICK, $data 
         or return -1;
  return 0;
}

# $_[0]: Reference to an opened filehandle
# Returns: -1 on failure, the read byte on success.
sub i2c_smbus_read_byte
{
  my ($file) = @_;
  my $data = [];
  i2c_smbus_access $file, $SMBUS_READ, 0, $SMBUS_BYTE, $data 
         or return -1;
  return $$data[0];
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Byte to write
# Returns: -1 on failure, 0 on success.
sub i2c_smbus_write_byte
{
  my ($file,$command) = @_;
  my $data = [$command];
  i2c_smbus_access $file, $SMBUS_WRITE, 0, $SMBUS_BYTE, $data 
         or return -1;
  return 0;
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Command byte (usually register number)
# Returns: -1 on failure, the read byte on success.
sub i2c_smbus_read_byte_data
{
  my ($file,$command) = @_;
  my $data = [];
  i2c_smbus_access $file, $SMBUS_READ, $command, $SMBUS_BYTE_DATA, $data 
         or return -1;
  return $$data[0];
}
  
# $_[0]: Reference to an opened filehandle
# $_[1]: Command byte (usually register number)
# $_[2]: Byte to write
# Returns: -1 on failure, 0 on success.
sub i2c_smbus_write_byte_data
{
  my ($file,$command,$value) = @_;
  my $data = [$value];
  i2c_smbus_access $file, $SMBUS_WRITE, $command, $SMBUS_BYTE_DATA, $data 
         or return -1;
  return 0;
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Command byte (usually register number)
# Returns: -1 on failure, the read word on success.
# Note: some devices use the wrong endiannes; use swap_bytes to correct for 
# this.
sub i2c_smbus_read_word_data
{
  my ($file,$command) = @_;
  my $data = [];
  i2c_smbus_access $file, $SMBUS_READ, $command, $SMBUS_WORD_DATA, $data 
         or return -1;
  return $$data[0] + 256 * $$data[1];
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Command byte (usually register number)
# $_[2]: Byte to write
# Returns: -1 on failure, 0 on success.
# Note: some devices use the wrong endiannes; use swap_bytes to correct for 
# this.
sub i2c_smbus_write_word_data
{
  my ($file,$command,$value) = @_;
  my $data = [$value & 0xff, $value >> 8];
  i2c_smbus_access $file, $SMBUS_WRITE, $command, $SMBUS_WORD_DATA, $data 
         or return -1;
  return 0;
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Command byte (usually register number)
# $_[2]: Word to write
# Returns: -1 on failure, read word on success.
# Note: some devices use the wrong endiannes; use swap_bytes to correct for 
# this.
sub i2c_smbus_process_call
{
  my ($file,$command,$value) = @_;
  my $data = [$value & 0xff, $value >> 8];
  i2c_smbus_access $file, $SMBUS_WRITE, $command, $SMBUS_PROC_CALL, $data 
         or return -1;
  return $$data[0] + 256 * $$data[1];
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Command byte (usually register number)
# Returns: Undefined on failure, a list of read bytes on success
# Note: some devices use the wrong endiannes; use swap_bytes to correct for 
# this.
sub i2c_smbus_read_block_data
{
  my ($file,$command) = @_;
  my $data = [];
  i2c_smbus_access $file, $SMBUS_READ, $command, $SMBUS_BLOCK_DATA, $data 
         or return;
  shift @$data;
  return @$data;
}

# $_[0]: Reference to an opened filehandle
# $_[1]: Command byte (usually register number)
# @_[2..]: List of values to write
# Returns: -1 on failure, 0 on success.
# Note: some devices use the wrong endiannes; use swap_bytes to correct for 
# this.
sub i2c_smbus_write_block_data
{
  my ($file,$command,@data) = @_;
  i2c_smbus_access $file, $SMBUS_WRITE, $command, $SMBUS_BLOCK_DATA, \@data 
         or return;
  return 0;
}

####################
# ADAPTER SCANNING #
####################

# $_[0]: The number of the adapter to scan
sub scan_adapter
{
  my ($chip, $addr, $conf,@chips);
  open FILE,"/dev/i2c-$_[0]" or print ("Can't open /dev/i2c-$_[0] ($!)\n"), 
                                return;
  foreach $addr (0..0x7f) {
    i2c_set_slave_addr(\*FILE,$addr) or print("Can't set address to $_?!?\n"), 
                                     next;
    next unless i2c_smbus_read_byte(\*FILE) >= 0;
    printf "Client found at address 0x%02x\n",$addr;
    foreach $chip (@chip_ids) {
      if (contains $addr, @{$$chip{i2c_addrs}}) {
        print "Probing for $$chip{name}... ";
        if (($conf,@chips) = &{$$chip{i2c_detect}} (\*FILE ,$addr)) {
          print "Success!\n",
                "    (confidence $conf, driver `$$chip{driver}'";
          if (@chips) {
            print ", other addresses: @chips)\n";
          } else {
            print ")\n";
          }
        } else {
          print "Failed!\n" 
        }
      }
    }
  }
}


##################
# CHIP DETECTION #
##################

# Each function returns a confidence value. The higher this value, the more
# sure we are about this chip. A Winbond W83781D, for example, will be
# detected as a LM78 too; but as the Winbond detection has a higher confidence 
# factor, you should identify it as a Winbond.

# Each function returns a list. The first element is the confidence value;
# Each element after it is an SMBus address. In this way, we can detect 
# chips with several SMBus addresses. The SMBus address for which the
# function was called is never returned.

# If there are devices which get confused if they are only read from, then
# this program will surely confuse them. But we guarantee never to write to
# any of these devices.


# $_[0]: Chip to detect (0 = LM78, 1 = LM78-J, 2 = LM79)
# $_[1]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[2]: Address
# Returns: undef if not detected, (7) if detected.
# Registers used:
#   0x40: Configuration
#   0x48: Full I2C Address
#   0x49: Device ID
# Note that this function is always called through a closure, so the
# arguments are shifted by one place.
sub lm78_detect
{
  my $reg;
  my ($chip,$file,$addr) = @_;
  return unless i2c_smbus_read_byte_data($file,0x48) == $addr;
  return unless (i2c_smbus_read_byte_data($file,0x40) & 0x80) == 0x00;
  $reg = i2c_smbus_read_byte_data($file,0x49);
  return unless ($chip == 0 and $reg == 0x00) or
                    ($chip == 1 and $reg == 0x40) or
                    ($chip == 2 and ($reg & 0xfe) == 0xc0);
  return (7);
}

# $_[0]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[1]: Address
# Returns: undef if not detected, (3) if detected.
# Registers used:
#   0x01: Configuration
#   0x02: Hysteris
#   0x03: Overtemperature Shutdown
# Detection really sucks! It is only based on the fact that the LM75 has only
# four registers. Any other chip in the valid address range with only four
# registers will be detected too.
# Note that register $00 may change, so we can't use the modulo trick on it.
sub lm75_detect
{
  my $i;
  my ($file,$addr) = @_;
  my $conf = i2c_smbus_read_byte_data($file,0x01);
  my $hyst = i2c_smbus_read_word_data($file,0x02);
  my $os = i2c_smbus_read_word_data($file,0x03);
  for ($i = 0x00; $i <= 0xff; $i += 4) {
    return if i2c_smbus_read_byte_data($file,$i + 0x01) != $conf;
    return if i2c_smbus_read_word_data($file,$i + 0x02) != $hyst;
    return if i2c_smbus_read_word_data($file,$i + 0x03) != $os;
  }
  return (3);
}
  

# $_[0]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[1]: Address
# Returns: undef if not detected, (3) if detected.
# Registers used:
# Registers used:
#   0x02: Interrupt state register
# How to detect this beast? 
sub lm80_detect
{
  my $i;
  my ($file,$addr) = @_;
  return if (i2c_smbus_read_byte_data($file,$0x02) & 0xc0) != 0;
  for ($i = 0x2a; $i <= 0x3d; $i++) {
    my $reg = i2c_smbus_read_byte_data($file,$i);
    return if i2c_smbus_read_byte_data($file,$i+0x40) != $reg;
    return if i2c_smbus_read_byte_data($file,$i+0x80) != $reg;
    return if i2c_smbus_read_byte_data($file,$i+0xc0) != $reg;
  }
  return (3);
}
  
# $_[0]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[1]: Address
# Returns: undef if not detected, (8,addr1,addr2) if detected, but only
#          if the LM75 chip emulation is enabled.
# Registers used:
#   0x48: Full I2C Address
#   0x4a: I2C addresses of emulated LM75 chips
#   0x4e: Vendor ID byte selection, and bank selection
#   0x4f: Vendor ID
#   0x58: Device ID (only when in bank 0)
# Note: Fails if the W83781D is not in bank 0 (may succeed for bank 2; bank 1
# leaves almost all registers in an undefined state, too bad).
# Note: Detection overrules a previous LM78 detection
sub w83781d_detect
{
  my ($reg1,$reg2,@res);
  my ($chip,$file,$addr) = @_;
  return unless i2c_smbus_read_byte_data($file,0x48) == $addr;
  $reg1 = i2c_smbus_read_byte_data($file,0x4e);
  $reg2 = i2c_smbus_read_byte_data($file,0x4f);
  return unless (($reg1 & 0x80) == 0x00 and $reg2 == 0xa3) or 
                (($reg1 & 0x80) == 0x80 and $reg2 == 0x5c);
  return if ($reg1 & 0x07) == 0x00 and 
            i2c_smbus_read_byte_data($file,0x58) != 0x10;
  $reg1 = i2c_smbus_read_byte_data($file,0x4a);
  @res = (8);
  push @res, ($reg1 & 0x07) + 0x48 if $reg1 & 0x08;
  push @res, (($reg1 & 0x80) >> 4) + 0x48 if $reg1 & 0x80;
  return @res;
}

# $_[0]: Chip to detect (0 = Revision 0x00, 1 = Revision 0x80)
# $_[1]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[2]: Address
# Returns: undef if not detected, (6) if detected.
# Registers used:
#   0x00: Device ID
#   0x01: Revision ID
#   0x03: Configuration 
# Mediocre detection
sub gl518sm_detect
{
  my $reg;
  my ($chip,$file,$addr) = @_;
  return unless i2c_smbus_read_byte_data($file,0x00) == 0x80;
  return unless (i2c_smbus_read_byte_data($file,0x03) & 0x80) == 0x00;
  $reg = i2c_smbus_read_byte_data($file,0x01);
  return unless ($chip == 0 and $reg == 0x00) or
                ($chip == 1 and $reg == 0x80);
  return (6);
}

# $_[0]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[1]: Address
# Returns: undef if not detected, (5) if detected.
# Registers used:
#   0x00: Device ID
#   0x01: Revision ID
#   0x03: Configuration 
# Mediocre detection
sub gl520sm_detect
{
  my ($file,$addr) = @_;
  return unless i2c_smbus_read_byte_data($file,0x00) == 0x20;
  return unless (i2c_smbus_read_byte_data($file,0x03) & 0x80) == 0x00;
  # The line below must be better checked before I dare to use it.
  # return unless i2c_smbus_read_byte_data($file,0x01) == 0x00;
  return (5);
}

# $_[0]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[1]: Address
# Returns: undef if not detected, (5) if detected.
# Registers used:
#   0x3e: Company ID
#   0x40: Configuration
#   0x48: Full I2C Address
# Note: Detection overrules a previous LM78 detection
sub adm9240_detect
{
  my ($file,$addr) = @_;
  return unless i2c_smbus_read_byte_data($file,0x3e) == 0x23;
  return unless (i2c_smbus_read_byte_data($file,0x40) & 0x80) == 0x00;
  return unless i2c_smbus_read_byte_data($file,0x48) == $addr;
  
  return (8);
}

# $_[0]: Chip to detect (0 = ADM1021, 1 = MAX1617)
# $_[1]: A reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was already done.
# $_[2]: Address
# Returns: undef if not detected, (5) or (3) if detected.
# Registers used:
#   0xfe: Company ID
#   0x02: Status
# Note: Especially the Maxim has very bad detection; we give it a low 
# confidence value.
sub adm1021_detect
{
  my $reg;
  my ($chip, $file,$addr) = @_;
  return if $chip == 0 and i2c_smbus_read_byte_data($file,0xfe) != 0x41;
  # The remaining things are flaky at best. Perhaps something can be done
  # with the fact that some registers are unreadable?
  return if (i2c_smbus_read_byte_data($file,0x02) & 0x03) != 0;
  if ($chip == 0) {
    return (6);
  } else {
    return (3);
  }
}

################
# MAIN PROGRAM #
################

sub main
{
  my (@adapters,$res,$did_adapter_detection,$detect_others,$adapter);

  initialize_proc_pci;
  initialize_modules_list;

  print " This program will help you to determine which I2C/SMBus modules you ",
        "need to\n",
        " load to use lm_sensors most effectively.\n";
  print " You need to have done a `make install', issued a `depmod -a' and ",
        "made sure\n",
        " `/etc/conf.modules' (or `/etc/modules.conf') contains the ",
        "appropriate\n",
        " module path before you can use some functions of this utility. ",
        "Read\n",
        " doc/modules for more information.\n";
  print " Also, you need to be `root', or at least have access to the ",
        "/dev/i2c-* files\n",
        " for some things. You can use prog/mkdev/mkdev.sh to create these ",
        "/dev files\n",
        " if you do not have them already.\n\n";

  print " We can start with probing for (PCI) I2C or SMBus adapters.\n";
  print " You do not need any special privileges for this.\n";
  print " Do you want to probe now? (YES/no): ";
  @adapters = adapter_pci_detection 
                        if ($did_adapter_detection = not <STDIN> =~ /\s*[Nn]/);

  print "\n";

  if (not $did_adapter_detection) {
    print " As you skipped adapter detection, we will only scan already ",
          "loaded adapter\n",
          " modules. You can still be prompted for non-detectable adapters.\n",
          " Do you want to? (yes/NO): ";
    $detect_others = <STDIN> =~ /^\s*[Yy]/;
  } elsif ($> != 0) {
    print " As you are not root, we can't load adapter modules. We will only ",
          "scan\n",
          " already loaded adapters.\n";
    $detect_others = 0;
  } else {
    print " We will now try to load each adapter module in turn.\n";
    foreach $adapter (@adapters) {
      if (contains $adapter, @modules_list) {
        print "Module `$adapter' already loaded.\n";
      } else {
        print "Load `$adapter'? (YES/no): ";
        unless (<STDIN> =~ /^\s*[Nn]/) {
          if (system ("modprobe", $adapter)) {
            print "Loading failed ($!)... skipping.\n";
          } else {
            print "Module loaded succesfully.\n";
          }
        }
      }
    }
    print " Do you now want to be prompted for non-detectable adapters? ",
          "(YES/no): ";
    $detect_others = not <STDIN> =~ /^\s*[Nn]/ ;
  }

  if ($detect_others) {
    foreach $adapter (@undetectable_adapters) {
      print "Load `$adapter'? (YES/no): ";
      unless (<STDIN> =~ /^\s*[Nn]/) {
        if (system ("modprobe", $adapter)) {
          print "Loading failed ($!)... skipping.\n";
        } else {
          print "Module loaded succesfully.\n";
        }
      }
    }
  }

  print " To continue, we need modules `i2c-proc' and `i2c-dev' to be ",
        "loaded.\n";
  if (contains "i2c-proc", @modules_list) {
    print "i2c-proc is already loaded.\n";
  } else {
    if ($> != 0) {
      print " i2c-proc is not loaded, and you are not root. I can't ",
            "continue.\n";
      exit;
    } else {
      print " i2c-proc is not loaded. May I load it now? (YES/no): ";
      if (<STDIN> =~ /^\s*[Nn]/) {
        print " Sorry, in that case I can't continue.\n";
        exit;
      } elsif (system "modprobe","i2c-proc") {
        print " Loading failed ($!), aborting.\n";
        exit;
      } else {
        print " Module loaded succesfully.\n";
      }
    }
  }
  if (contains "i2c-dev", @modules_list) {
    print "i2c-dev is already loaded.\n";
  } else {
    if ($> != 0) {
      print " i2c-dev is not loaded. As you are not root, we will just hope ",
            "you edited\n",
            " `/etc/conf.modules' (or `/etc/modules.conf') for automatic ",
            "loading of\n",
            " this module. If not, you won't be able to open any /dev/i2c-* ",
            "file.\n";
    } else {
      print " i2c-dev is not loaded. Do you want to load it now? (YES/no): ";
      if (<STDIN> =~ /^\s*[Nn]/) {
        print " Well, you will know best. We will just hope you edited ",
              "`/etc/conf.modules'\n",
              " (or `/etc/modules.conf') for automatic loading of this ",
              "module. If not,",
              " you won't be able to open any /dev/i2c-* file.\n";
      } elsif (system "modprobe","i2c-proc") {
        print " Loading failed ($!), aborting.\n";
        exit;
      } else {
        print " Module loaded succesfully.\n";
      }
    }
  }

  print " \n We are now going to do the adapter probings. Some adapters may ",
        "hang halfway\n",
        " through; we can't really help that. Also, some chips will be double ",
        "detected;\n",
        " choose the one with the highest confidence value in that case.\n";

  open INPUTFILE,"/proc/bus/i2c" or die "Couldn't open /proc/bus/i2c?!?";
  while (<INPUTFILE>) {
    print "\n";
    my ($dev_nr,$description) = /^i2c-(\S+)\s+\S+\s+(.*)$/;
    print "Next adapter: $description\n";
    print "Do you want to scan it? (YES/no):";
    scan_adapter $dev_nr unless <STDIN> =~ /^\s*[Nn]/;
  }
}

main;


      


# TEST!
# scan_adapter 0;
#}

#
