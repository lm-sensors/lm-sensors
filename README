&=============================================================================

     FOR QUICK INSTALLATION INSTRUCTIONS SEE THE 'QUICKSTART' FILE.
     FOR FULL INSTALLATION INSTRUCTIONS SEE THE 'INSTALL' FILE.
     FOR THE FAQ SEE THE 'doc/FAQ' or 'doc/lm_sensors-FAQ.html' FILES.

=============================================================================

OVERVIEW OF THE LM_SENSORS PACKAGE AND SUMMARY OF SUPPORTED DEVICES

!!! This package is ONLY for 2.6, 2.5, and 2.4 kernels (2.4.10 or later) !!!
!!! THIS PACKAGE REQUIRES i2c-2.9.0 or later!!!

FOR 2.6/2.5 KERNELS, Use only the userspace tools in this package!
Build and install them with 'make user' and 'make user_install'.
The kernel modules in this package will not compile for 2.6/2.5;
use the drivers already in the 2.6 kernel.

=============================================================================

This is the completely rewritten version 2 of lm_sensors, a collection of
modules for general SMBus[1] access and hardware monitoring.
Version 1 is now officially unsupported.

WARNING! The drivers in this package will work on reasonably recent 2.4
kernels only (2.4.10 and later).
Use lm_sensors-2.4.5 for 2.0 kernels.
Use lm_sensors-2.7.0 for 2.2, 2.3, and 2.4.0 - 2.4.9 kernels.
Use the drivers already in the kernel for 2.6/2.5 kernels; if you need
additional drivers in 2.6 please port and submit them to us.

HOWEVER, the userspace tools in this package will work for
2.4, 2.5, and 2.6 kernels.

WARNING! You must have at least i2c-2.9.0.
EVEN IF your kernel does contain i2c support!

The I2C[2] package in existing 2.4 kernels is NOT sufficient
for compilation of this package.

ADDITIONALLY, i2c-2.9.0 is API compatible to i2c releases 2.7.0 and earlier,
but is not API compatible to i2c releases 2.8.0 - 2.8.8
due to struct changes. Versions 2.8.x of i2c are considered deprecated.

See the lm_sensors download page for further guidance:
  http://secure.netroedge.com/~lm78/download.html


WARNING! If you downloaded this package through our CVS archive, you walk
the cutting edge. Things may not even compile! On the other hand, you will
be the first to profit from new drivers and other changes. Have fun!

=============================================================================

At least the following I2C/SMBus adapters are supported:
  Acer Labs M1533, M1535, and M1543C
  AMD 756, 766, 768 and 8111
  AMD 8111 SMBus 2.0
  Apple Hydra (used on some PPC machines)
  DEC 21272/21274 (Tsunami/Typhoon - on Alpha boards)
  Intel I801 ICH/ICH0/ICH2/ICH3/ICH4/ICH5/ICH6 (82801xx), 6300ESB, ICH7
  Intel PIIX4 (used in many Intel chipsets)
  Intel I810/I815 GMCH
  Intel 82443MX (440MX)
  NVidia nForce, nForce2, nForce3, nForce4
  ServerWorks OSB4, CSB5, CSB6
  SiS 5595, 630, 645, 655, 730
  SMSC Victory66
  3Dfx Voodoo 3 and Banshee
  VIA Technologies VT82C586B, VT82C596A/B, VT82C686A/B, VT8231,
                   VT8233, VT8233A, VT8235 and VT8237


At least the following hardware sensor chips are supported:
  Analog Devices ADM1021, ADM1021A, ADM1022, ADM1023, ADM1024,
                 ADM1025, ADM1026, ADM1027, ADM1030, ADM1031,
                 ADM1032, ADM9240, ADT7461 and ADT7463
  Asus AS99127F, ASB100 Bach
  Dallas Semiconductor DS75, DS1621, DS1625, DS1775, and DS1780
  Hewlett Packard Maxilife (several revisions including '99 NBA)
  Fujitsu Siemens Poseidon, Scylla, Hermes
  Genesys Logic GL518SM, GL520SM, GL523SM
  Intel Xeon processor embedded sensors
  ITE IT8705F, IT8712F embedded sensors
  Maxim MAX1617, MAX1617A, MAX1619, MAX6650, MAX6651,
        MAX6633, MAX6634, MAX6635, MAX6657, MAX6658
  Microchip TC1068, TCM1617, TCN75
  Myson MTP008
  National Semiconductor LM75, LM76, LM78, LM78-J, LM79,
                         LM80, LM81, LM83, LM84, LM85, LM86, LM87,
                         LM89, LM90, LM92, LM93, LM99, PC87360,
                         PC87363, PC87364, PC87365, PC87366
  Philips NE1617, NE1617A, NE1619
  SiS 5595, 950 embedded sensors
  SMSC 47M1xx embedded sensors, EMC6D100, EMC6D101, EMC6D102
  TI THMC10 and THMC50
  VIA Technologies VT1211, VT8231 and VT82C686A/B embedded sensors
  Winbond W83781D, W83782D, W83783S, W83791D, W83792D,
          W83627HF, W83627THF, W83637HF, and W83697HF


We also support some miscellaneous chips:
  Dallas DS1307 real time clock
  Intel Xeon processor embedded EEPROMs
  Linear Technologies LTC1710
  Philips Semiconductors PCF8574, PCF8591  
  DDC Monitor embedded EEPROMs
  SDRAM Dimms with Serial Presence Detect EEPROMs
  Smart Battery sensors
  IPMI-BMC sensors
  Philips Semiconductors SAA1064


The list above may be out of date;
see our New Drivers page http://www.lm-sensors.nu/~lm78/newdrivers.html
for the latest information on supported devices.
You may also refer to doc/chips/SUMMARY for details on what each chip
can monitor.


We always appreciate testers. If you own a specific monitoring chip listed
on our 'new drivers' page, and are willing to help us out, please contact
us. Even if you have no programming knowledge, you could help us by running
new modules and reporting on the results and output. If you want to offer
more substantial help, this is very welcome too, of course.


Don't ask us whether we support a particular mainboard; we do not know.
We *do* know what hardware we support, but usually, it is easier to
install everything and run sensors-detect. It will tell you what hardware
you have (and incidentally, what corresponding drivers are needed). You
could also take a look at http://mbm.livewiredev.com/
(this lists chips found on many mainboard, but regrettable, not the adapters
on them) or http://web01.fureai.or.jp/~hirobo/project/reserch_project.html
(yes, it is japanese; you want the ninth column, and it again lists only
chips, not adapters).


SMBus, also known as System Management Bus, is a protocol for communicating
through a I2C ('I squared C') bus. Many modern mainboards have a System
Management Bus. There are a lot of devices which can be connected to a
SMBus; the most notable are modern memory chips with EEPROM memories and
chips for hardware monitoring.

Most modern mainboards incorporate some form of hardware monitoring chips.
These chips read things like chip temperatures, fan rotation speeds and
voltage levels. There are quite a few different chips which can be used by
mainboard builders for approximately the same results.

Hardware monitoring chips are often connected to the SMBus, but often they
can also be connected to the ISA bus. The modules in this package usually
support both ways of accessing them.

Because the SMBus is just a special case of the generalized I2C bus, we can
simulate the SMBus protocol on plain I2C busses. These busses are sometimes
used in other parts of your computer. If a supported chip is attached to
one of these additional busses, they can be used too.

Please read INSTALL before trying to compile and install these modules.
There is a lot of additional documentation in the doc/ subdirectory.
Amnong these is a list of supported busses and chips. Regrettably, there
are too many mainboards to keep a list of busses and chips used on them.
On the other hand, we provide a program called 'sensors-detect' which
tries to figure out what hardware is available on your system.

The developers of this package can be reached through the mailing-list
<sensors@stimpy.netroedge.com>. Do not hesitate to mail us if you have 
questions, suggestions, problems, want to contribute, or just want to 
report it works for you. But please try to read the documentation and
FAQ before you ask any questions!

The latest version of this package can always be found on our homepage:
http://secure.netroedge.com/~lm78/. Pre-release versions can be retrieved
through anonymous CVS; see doc/cvs for more information.

This package may be distributed according to the GNU General Public
License (GPL), as included in the file COPYING.

Note that libsensors falls under the GPL, not the LGPL.  In more human
language, that means it is FORBIDDEN to link any application to the
library, even to the shared version, if the application itself does not
fall under the GPL.


-----
[1] SMBus is a trademark of Intel Corporation
[2] I2C is a trademark of Philips Corporation
