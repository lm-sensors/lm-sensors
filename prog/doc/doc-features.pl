#!/usr/bin/perl

#
#    doc-features.pl - Generate module/library documentation
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
#

use strict;

# Features is a hash of references (indexed by SENSORS_*_PREFIX names) to 
# a hash with these elements:
#   prefix: The prefix name
#   arrayname: The name of the c-array containing its data
#   features: A reference to an array of references to hashes, each 
#     containing the following fields:
#        number: The preprocessor symbol for the number of this feature
#        name: The name of this feature
#        logical_mapping: The preprocessor symbol for the logical mapping
#                         feature
#        compute_mapping: The preprocessor symbol for the compute mapping
#                         feature
#        mode_read: 1 if the feature is readable, 0 if not
#        mode_write: 1 if the feature is writable, 0 if not
#        sysctl: Preprocessor name of the sysctl file
#        offset: Offset (in longs) within the sysctl file
#        magnitude: Magnitude of this feature

use vars qw(%features %sysctls %feature_name);

# Scan chips.h to get all prefix values
# Store the results in features{...}->prefix
# $_[0]: Name of chips.h file
sub scan_chips_h
{
  my ($chips_h) = @_;
  my ($def,$val);
  open INPUTFILE,$chips_h or die "Can't open $chips_h!";
  while (<INPUTFILE>) {
    $features{$def}->{prefix} = $val 
        if ($def,$val) = /^\s*#\s*define\s+(SENSORS_\w+_PREFIX)\s+"(\S+)"/;

  }
  close INPUTFILE;
}

# Scan chips.c to get the features names
# Store the results in features{...}->arrayname
# $_[0]: Name of the chips.c file
sub scan_chips_c_1
{
  my ($chips_c) = @_;
  my ($def,$val);
  open INPUTFILE,$chips_c or die "Can't open $chips_c!";
  while (<INPUTFILE> !~ /^\s*sensors_chip_features/ ) {};
  while (<INPUTFILE>) {
    last if /^\s*{\s*0/;
    $features{$def}->{arrayname} = $val 
       if ($def,$val) = /^\s*{\s*(SENSORS_\w+_PREFIX)\s*,\s*(\w+)\s*}\s*,\s*$/;
  }
  close INPUTFILE;
}

# $_[0]: line to tokenize
# Returns the list of tokens
# Tokens are either '{', '}', ',', ';' or anything between them. Spaces on 
# each side of a token are removed; spaces inside a token are not.
sub tokenize
{
  my ($line) = @_;
  my (@res,$next);
  @res = ();
  $line =~ s/^\s*//;
  while (length $line) {
    if ($line =~ s/^\s*$//) {
      last;
    } elsif ($line =~ s/^{\s*//) {
      push @res, '{';
    } elsif ($line =~ s/^}\s*//) {
      push @res, '}';
    } elsif ($line =~ s/^;\s*//) {
      push @res, ';';
    } elsif ($line =~ s/^,\s*//) {
      push @res, ',';
    } else {
      ($next) = $line =~ /^(\S+?)(?=[{},;\s])\s*/;
      $line =~ s/^(\S+?)(?=[{},;\s])\s*//;
      push @res, $next;
    }
  }
  return @res;
}

# $_[0]: File to read from
# $_[1]: Feature to write to
sub scan_chip_c_entry
{
  my ($file,$feature) = @_;
  my ($line,@tokens,$next,$new);
  for (;;) {
    $line = <$file>;
    @tokens = (@tokens,tokenize($line));
    last if $line =~ /;/;
  }
  die "Parse error: initial '{' expected for $feature->arrayname" 
           if shift @tokens ne '{';
  $feature->{features} = [];
  while (shift @tokens eq '{') {
    $new = {};
    $new->{number} = shift @tokens;
    last if $new->{number} eq "0";
    die "Parse error: ',' expected for $feature->arrayname"
           if shift @tokens ne ',';
    $next = shift @tokens;
    ($new->{name}) = $next =~ /^"(.*)"$/;
    die "Parse error: ',' expected for $feature->arrayname"
           if shift @tokens ne ',';
    $new->{logical_mapping} = shift @tokens;
    die "Parse error: ',' expected for $feature->arrayname"
           if shift @tokens ne ',';
    $new->{compute_mapping} = shift @tokens;
    die "Parse error: ',' expected for $feature->arrayname"
           if shift @tokens ne ',';
    $next = shift @tokens;
    $new->{mode_read} = ($next eq "SENSORS_MODE_R" or 
                         $next eq "SENSORS_MODE_RW")?1:0;
    $new->{mode_write} = ($next eq "SENSORS_MODE_W" or 
                          $next eq "SENSORS_MODE_RW")?1:0;
    die "Parse error: ',' expected for $feature->arrayname"
           if shift @tokens ne ',';
    $new->{sysctl} = shift @tokens;
    die "Parse error: ',' expected for $feature->arrayname"
           if shift @tokens ne ',';
    $next = shift @tokens;
    ($new->{offset}) = $next =~ /^VALUE\s*\((\d+)\s*\)/
           or die "Parse error: 'VALUE(..)' expected for $feature->arrayname";
    die "Parse error: ',' expected for $feature->arrayname"
           if shift @tokens ne ',';
    $new->{magnitude} = shift @tokens;
    $next = shift @tokens;
    $next = shift @tokens if $next eq ',';
    push @{$feature->{features}},$new;
    $feature_name{$new->{number}} = $new->{name};
    die "Parse error: '}' expected for $feature->arrayname" if $next ne '}';
    shift @tokens;
    
  }
}

# Scan chips.c to get the features names
# Store the results in features{...}->feature
# $_[0]: Name of the chips.c file
sub scan_chips_c_2 
{
  my ($chips_c) = @_;
  my ($line,$name,$el,$feature);
  open INPUTFILE,$chips_c or die "Can't open $chips_c!";
  while ($line = <INPUTFILE>) {
    next if $line !~ /sensors_chip_feature /;
    ($name) = $line =~ /sensors_chip_feature\s*(\w+)\s*\[/;
    foreach $el (keys %features) {
      $feature = $features{$el};
      last if $feature->{arrayname} eq $name
    }
    die "Can't find feature belonging to $name!" if (! defined $feature);
    scan_chip_c_entry \*INPUTFILE,$feature;
  }
}

# $_[0]: Name of the kernels module
sub scan_kernel_module
{
  my ($module_name) = @_;
  my ($line,@tokens,$val,$name);
  open INPUTFILE,$module_name or die "Can't open $module_name!";
  while ($line = <INPUTFILE>) {
    next if $line !~ /^\s*static\s*ctl_table/;
    @tokens = ();
    for (;;) {
      $line = <INPUTFILE>;
      @tokens = (@tokens,tokenize($line));
      last if $line =~ /;/;
    }
    while (@tokens) {
      for (;;) {
        $val = shift @tokens;
        last if $val ne '{';
      }
      last if $val eq "0";
      die "Parse error: ',' expected within $module_name"
             if shift @tokens ne ',';
      $name = shift @tokens;
      ($sysctls{$val}) = $name =~ /^"(\S+)"$/;
      for (;;) {
        last if shift @tokens eq '{';
      }
    }
  }
  close INPUTFILE;
}

# $_[0]: Base directory
sub scan_all
{
  my ($prefix) = @_;
  my ($filename);
  scan_chips_h $prefix . "/lib/chips.h";
  scan_chips_c_1 $prefix . "/lib/chips.c";
  scan_chips_c_2 $prefix . "/lib/chips.c";
  foreach $filename (glob ($prefix . "/kernel/chips/*.c")) { 
    scan_kernel_module "$filename"
  }
}

# $_[0]: index in master hash
sub output_data
{
  my ($index) = @_;
  my ($feature,$data,$read_write);
  $feature = $features{$index};
  printf "Chip `%s'\n",$feature->{prefix};
  print "             LABEL        LABEL CLASS      COMPUTE CLASS ACCESS ".
        "MAGNITUDE\n";
  foreach $data (@{$feature->{features}}) {
    $read_write = $data->{mode_read} + 2 * $data->{mode_write};
    if ($read_write == 0) {
      $read_write = "--";
    } elsif ($read_write == 1) {
      $read_write = "R ";
    } elsif ($read_write == 2) {
      $read_write = " W";
    } else {
      $read_write = "RW";
    }
    printf "%18s %18s %18s     %2s   %2s\n",
           $data->{name}, $feature_name{$data->{logical_mapping}}, 
           $feature_name{$data->{compute_mapping}},
           $read_write,$data->{magnitude};
  }
  print "\n";
  print "             LABEL                          FEATURE SYMBOL       ".
        " SYSCTL FILE:NR\n";
  foreach $data (@{$feature->{features}}) {
    printf "%18s %39s %18s:%1d\n",$data->{name}, $data->{number},
           $sysctls{$data->{sysctl}}, $data->{offset}, $data->{magnitude};
  }
}

sub initialize
{
  $feature_name{SENSORS_NO_MAPPING} = "NONE";
}

sub help_message
{
  print STDERR "Syntax: doc-features PATH PREFIXES...\n";
  print STDERR "PATH is the path to the base of the lm_sensors tree.\n";
  print STDERR "PREFIXES are the chips that have to be output. If none ".
               "are specified,\n".
               "all chips are printed.\n";
}

# @_: @ARGV
# Returns ($base_dir,@modules)
sub scan_arguments 
{
  my ($base_dir,@chips);
  if (@_ < 1 or @_[0] =~ /^-/) {
    help_message;
    exit 0;
  }
  $base_dir = @_[0];
  if (@_ == 1) {
    @chips =  ();
  } else {
    @chips = @_; 
    splice(@chips,0,1);
  }
  return ($base_dir,@chips)
}

sub main {
  my ($el,$base_dir,@prefixes,$prefix);
  initialize;
  my ($base_dir,@prefixes) = scan_arguments @_;
  scan_all $base_dir;
  print "Chip Features\n";
  print "-------------\n\n";
  if (@prefixes) {
    foreach $prefix (@prefixes) {
      foreach $el (keys %features) {
        output_data ($el), print "\n\n"  
                 if ($features{$el}->{prefix} eq $prefix);
      }
    } 
  } else {
    foreach $el (keys %features) {
      output_data $el;
      print "\n\n";
    }
  }
}

main @ARGV;
