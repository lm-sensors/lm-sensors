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

use vars qw(@pci_list @pci_adapters);

#########################
# CONSTANT DECLARATIONS #
#########################

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


##############
# PCI ACCESS #
##############

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

use vars qw($IOCTL_I2C_RETRIES $IOCTL_I2C_TIMEOUT $IOCTL_I2C_UDELAY 
            $IOCTL_I2C_MDELAY $IOCTL_I2C_SLAVE $IOCTL_I2C_TENBIT 
            $IOCTL_I2C_SMBUS);

# These are copied from <linux/i2c.h> and <linux/smbus.h>

# For bit-adapters:
$IOCTL_I2C_RETRIES = 0x0701;
$IOCTL_I2C_TIMEOUT = 0x0702;
$IOCTL_I2C_UDELAY = 0x0705;
$IOCTL_I2C_MDELAY = 0x0706;

# General ones
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

sub i2c_set_slave_addr
{
  my ($file,$addr) = @_;
  ioctl $file, $IOCTL_I2C_SLAVE, $addr or return 0;
  return 1;
}

# i2c_smbus_access is based upon the corresponding C function (see 
# <linux/i2c-dev.h>). Note that 'data' is a reference to a list,
# while 'file' is a reference to a filehandle. Also, it seems that in
# a struct, chars are encoded as shorts. This is all very tricky and
# depends on C compiler internals...
sub i2c_smbus_access
{
  my ($file,$read_write,$command,$size,$data) = @_;
  my $data_array = pack "C32", @$data;
  my $ioctl_data = pack "SSIp", ($read_write,$command,$size,$data_array);
  ioctl $file, $IOCTL_I2C_SMBUS, $ioctl_data or return 0;
  $_[4] = [ unpack "C32",$data_array ];
  return 1;
}

sub i2c_read_byte_data
{
  my ($file,$command) = @_;
  my @data = (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
  i2c_smbus_access $file, $SMBUS_READ, $command, $SMBUS_BYTE_DATA, \@data 
         or return -1;
  printf "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", @data;
  return $data[0];
}
  
sub i2c_read_word_data
{
  my ($file,$command) = @_;
  my $data = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  i2c_smbus_access $file, $SMBUS_READ, $command, $SMBUS_WORD_DATA, $data 
         or return -1;
  return $$data[0] + 256 * $$data[1];
}
  

################
# MAIN PROGRAM #
################

intialize_proc_pci;
adapter_pci_detection;

# TEST!
#open FILE, "+>/dev/i2c-0" or die "Can't open /dev/i2c-0!";
#i2c_set_slave_addr \*FILE, 0x49 or die "Couldn't set slave addr!";
#print (i2c_read_word_data \*FILE, 0), "\n";
