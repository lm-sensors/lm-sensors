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

use vars qw(@pci_adapters @chip_ids);

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
       driver => "i2c-ali15x3"
     } ,
     {
       vendid => 0x10b9,
       devid => 0x7101,
       funcid => 0,
       procid => "Acer Labs M7101",
     }
);


# This is a list of all recognized chips. 
# Each entry must have a name (Full Chip Name), an array i2c_addrs (Valid
# I2C Addresses; may be omitted if this is a pure ISA chip), an array 
# isa_addrs (Valid ISA Addresses; may be omitted if this is a pure I2C chip)
# ,...
# If no driver is written yet, omit the driver (Driver Name) field.
@chip_ids = (
     {
       name => "National Semiconductors LM78",
       driver => "lm78",
       i2c_addrs => (0x00..0x7f), 
       isa_addrs => (0x290),  # Theoretically anyway, but this will do
     } ,
     {
       name => "National Semiconductors LM78-J",
       driver => "lm78",
       i2c_addrs => (0x00..0x7f),
       isa_addrs => (0x290),  # Theoretically anyway, but this will do
     } ,
     {
       name => "National Semiconductors LM79",
       driver => "lm78",
       i2c_addrs => (0x00..0x7f),
       isa_addrs => (0x290),  # Theoretically anyway, but this will do
     } ,
     {
       name => "National Semiconductors LM75",
       driver => "lm75",
       i2c_addrs => (0x48..0x4f),
     } ,
     {
       name => "National Semiconductors LM80",
       driver => "lm80",
       i2c_addrs => (0x28..0x2f),
     }
);


#######################
# AUXILIARY FUNCTIONS #
#######################

sub swap_bytes
{
  return (($_[0] & 0xff00) >> 8) + (($_[0] & 0x00ff) << 7)
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

sub intialize_proc_pci
{
  @pci_list = read_proc_dev_pci;
  @pci_list = read_proc_pci     if not defined @pci_list;
  die "Can't access either /proc/bus/pci/ or /proc/pci!" 
                                    if not defined @pci_list;
}

#####################
# ADAPTER DETECTION #
#####################
  
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
  open FILE,"/dev/i2c-".$_[0] or die "Can't open /dev/i2c-".$_[0];
  foreach (0..0x7f) {
    i2c_set_slave_addr(\*FILE,$_) or print("Can't set address to $_?!?\n"), 
                                     next;
    printf ("Client found at address 0x%02x\n",$_) 
                                  if i2c_smbus_read_byte(\*FILE) >= 0;
  }
}

##################
# CHIP DETECTION #
##################

# $_[0]: 0 for ISA, 1 for I2C
# $_[1]: Address
# $_[2]: For I2C, a reference to the file descriptor to access this chip.
#        We may assume an i2c_set_slave_addr was aleady done.
#sub lm78_detect
#{
#  $@lm78_read = 

################
# MAIN PROGRAM #
################

intialize_proc_pci;
adapter_pci_detection;


# TEST!
#scan_adapter 0;
