#!/usr/bin/perl

use strict;

# This function returns a reference to an array of hashes. Each hash has some
# PCI information (more than we will ever need, probably). The most important
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
  return \@pci_list;
}

# This function returns a reference to an array of hashes. Each hash has some
# PCI information. The important fields here are 'bus', 'slot', 'func' (they
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
  return \@pci_list;
}

sub adapter_pci_detection
{
  my @pci_adapters = ( 
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

  my $pci_list_ref = read_proc_dev_pci;
  my $old_pci_interface = 0;
  my ($device,$try,@res);

  print "Probing for PCI bus adapters...\n";

  if (! defined @$pci_list_ref) {
    $pci_list_ref = read_proc_pci;
    die "Can't access either /proc/bus/pci/ or /proc/pci!" 
                                                  if ! defined @$pci_list_ref;
    $old_pci_interface = 1;
  }

  foreach $device (@$pci_list_ref) {
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

adapter_pci_detection;
