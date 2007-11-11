#!/usr/bin/perl -w
#
# Copyright 1998, 1999 Philip Edelbrock <phil@netroedge.com>
# modified by Christian Zuckschwerdt <zany@triq.net>
# modified by Burkart Lingner <burkart@bollchen.de>
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
# Version 0.4  1999  Philip Edelbrock <phil@netroedge.com>
# Version 0.5  2000-03-30  Christian Zuckschwerdt <zany@triq.net>
#  html output (selectable by commandline switches)
# Version 0.6  2000-09-16  Christian Zuckschwerdt <zany@triq.net>
#  updated according to SPD Spec Rev 1.2B
#  see http://developer.intel.com/technology/memory/pc133sdram/spec/Spdsd12b.htm
# Version 0.7  2002-11-08  Jean Delvare <khali@linux-fr.org>
#  pass -w and use strict
#  valid HTML 3.2 output (--format mode)
#  miscellaneous formatting enhancements and bug fixes
#  clearer HTML output (original patch by Nick Kurshev <nickols_k@mail.ru>)
#  stop decoding on checksum error by default (--checksum option forces)
# Version 0.8  2005-06-20  Burkart Lingner <burkart@bollchen.de>
#  adapted to Kernel 2.6's /sys filesystem
# Version 0.9  2005-07-15  Jean Delvare <khali@linux-fr.org>
#  fix perl warning
#  fix typo
#  refactor some code
# Version 1.0  2005-09-18  Jean Delvare <khali@linux-fr.org>
#  add large lookup tables for manufacturer names, based on data
#  provided by Rudolf Marek, taken from:
#  http://www.jedec.org/download/search/JEP106r.pdf
# Version 1.1  2006-01-22  Jean Delvare <khali@linux-fr.org>
#  improve the text output, making it hopefully clearer
#  read eeprom by 64-byte blocks, this allows some code cleanups
#  use sysopen/sysread instead of open/read for better performance
#  verify checksum before decoding anything
# Version 1.2  2006-05-15  Jean Delvare <khali@linux-fr.org>
#  implement per-memory-type decoding
#  don't decode revision code, manufacturing date and assembly serial
#  number where not set
#  decode the manufacturing date to an ISO8601 date
# Version 1.3  2006-05-21  Jean Delvare <khali@linux-fr.org>
#  detect undefined manufacturer code and handle it properly
#  round up timing data
#  minor display adjustments
#  group cycle and access times, display the CAS value for each (SDRAM)
#  refactor some bitfield tests into loops (SDRAM)
#  display latencies and burst length on a single line (SDRAM)
#  don't display manufacturing location when undefined
#  check that the manufacturing date is proper BCD, else fall back to
#  hexadecimal display
# Version 1.4  2006-05-26  Jean Delvare <khali@linux-fr.org>
#  fix latencies decoding (SDRAM)
#  fix CAS latency decoding (DDR SDRAM)
#  decode latencies, timings and module height (DDR SDRAM)
#  decode size (Direct Rambus, Rambus)
#  decode latencies and timings (DDR2 SDRAM)
#  SPD revision decoding depends on memory type
#  use more user-friendly labels
#  fix HTML formatted output on checksum error
# Version 1.5  2007-11-08  Jean Delvare <khali@linux-fr.org>
#  fix module speed (DDR2 SDRAM)
#
#
# EEPROM data decoding for SDRAM DIMM modules. 
#
# The eeprom driver must be loaded. For kernels older than 2.6.0, the
# eeprom driver can be found in the lm-sensors package.
#
# use the following command line switches
#  -f, --format            print nice html output
#  -b, --bodyonly          don't print html header
#                          (useful for postprocessing the output)
#  -c, --checksum          decode completely even if checksum fails
#  -h, --help              display this usage summary
#
# References: 
# PC SDRAM Serial Presence 
# Detect (SPD) Specification, Intel, 
# 1997,1999, Rev 1.2B
#
# Jedec Standards 4.1.x & 4.5.x
# http://www.jedec.org
#

require 5.004;

use strict;
use POSIX;
use Fcntl qw(:DEFAULT :seek);
use vars qw($opt_html $opt_body $opt_bodyonly $opt_igncheck $use_sysfs
	    @vendors %decode_callback);

@vendors = (
["AMD", "AMI", "Fairchild", "Fujitsu",
 "GTE", "Harris", "Hitachi", "Inmos",
 "Intel", "I.T.T.", "Intersil", "Monolithic Memories",
 "Mostek", "Freescale (formerly Motorola)", "National", "NEC",
 "RCA", "Raytheon", "Conexant (Rockwell)", "Seeq",
 "Philips Semi. (Signetics)", "Synertek", "Texas Instruments", "Toshiba",
 "Xicor", "Zilog", "Eurotechnique", "Mitsubishi",
 "Lucent (AT&T)", "Exel", "Atmel", "SGS/Thomson",
 "Lattice Semi.", "NCR", "Wafer Scale Integration", "IBM",
 "Tristar", "Visic", "Intl. CMOS Technology", "SSSI",
 "MicrochipTechnology", "Ricoh Ltd.", "VLSI", "Micron Technology",
 "Hyundai Electronics", "OKI Semiconductor", "ACTEL", "Sharp",
 "Catalyst", "Panasonic", "IDT", "Cypress",
 "DEC", "LSI Logic", "Zarlink (formerly Plessey)", "UTMC",
 "Thinking Machine", "Thomson CSF", "Integrated CMOS (Vertex)", "Honeywell",
 "Tektronix", "Sun Microsystems", "SST", "ProMos/Mosel Vitelic",
 "Infineon (formerly Siemens)", "Macronix", "Xerox", "Plus Logic",
 "SunDisk", "Elan Circuit Tech.", "European Silicon Str.", "Apple Computer",
 "Xilinx", "Compaq", "Protocol Engines", "SCI",
 "Seiko Instruments", "Samsung", "I3 Design System", "Klic",
 "Crosspoint Solutions", "Alliance Semiconductor", "Tandem", "Hewlett-Packard",
 "Intg. Silicon Solutions", "Brooktree", "New Media", "MHS Electronic",
 "Performance Semi.", "Winbond Electronic", "Kawasaki Steel", "Bright Micro",
 "TECMAR", "Exar", "PCMCIA", "LG Semi (formerly Goldstar)",
 "Northern Telecom", "Sanyo", "Array Microsystems", "Crystal Semiconductor",
 "Analog Devices", "PMC-Sierra", "Asparix", "Convex Computer",
 "Quality Semiconductor", "Nimbus Technology", "Transwitch", "Micronas (ITT Intermetall)",
 "Cannon", "Altera", "NEXCOM", "QUALCOMM",
 "Sony", "Cray Research", "AMS(Austria Micro)", "Vitesse",
 "Aster Electronics", "Bay Networks (Synoptic)", "Zentrum or ZMD", "TRW",
 "Thesys", "Solbourne Computer", "Allied-Signal", "Dialog",
 "Media Vision", "Level One Communication"],
["Cirrus Logic", "National Instruments", "ILC Data Device", "Alcatel Mietec",
 "Micro Linear", "Univ. of NC", "JTAG Technologies", "BAE Systems",
 "Nchip", "Galileo Tech", "Bestlink Systems", "Graychip",
 "GENNUM", "VideoLogic", "Robert Bosch", "Chip Express",
 "DATARAM", "United Microelec Corp.", "TCSI", "Smart Modular",
 "Hughes Aircraft", "Lanstar Semiconductor", "Qlogic", "Kingston",
 "Music Semi", "Ericsson Components", "SpaSE", "Eon Silicon Devices",
 "Programmable Micro Corp", "DoD", "Integ. Memories Tech.", "Corollary Inc.",
 "Dallas Semiconductor", "Omnivision", "EIV(Switzerland)", "Novatel Wireless",
 "Zarlink (formerly Mitel)", "Clearpoint", "Cabletron", "Silicon Technology",
 "Vanguard", "Hagiwara Sys-Com", "Vantis", "Celestica",
 "Century", "Hal Computers", "Rohm Company Ltd.", "Juniper Networks",
 "Libit Signal Processing", "Mushkin Enhanced Memory", "Tundra Semiconductor", "Adaptec Inc.",
 "LightSpeed Semi.", "ZSP Corp.", "AMIC Technology", "Adobe Systems",
 "Dynachip", "PNY Electronics", "Newport Digital", "MMC Networks",
 "T Square", "Seiko Epson", "Broadcom", "Viking Components",
 "V3 Semiconductor", "Flextronics (formerly Orbit)", "Suwa Electronics", "Transmeta",
 "Micron CMS", "American Computer & Digital Components Inc", "Enhance 3000 Inc", "Tower Semiconductor",
 "CPU Design", "Price Point", "Maxim Integrated Product", "Tellabs",
 "Centaur Technology", "Unigen Corporation", "Transcend Information", "Memory Card Technology",
 "CKD Corporation Ltd.", "Capital Instruments, Inc.", "Aica Kogyo, Ltd.", "Linvex Technology",
 "MSC Vertriebs GmbH", "AKM Company, Ltd.", "Dynamem, Inc.", "NERA ASA",
 "GSI Technology", "Dane-Elec (C Memory)", "Acorn Computers", "Lara Technology",
 "Oak Technology, Inc.", "Itec Memory", "Tanisys Technology", "Truevision",
 "Wintec Industries", "Super PC Memory", "MGV Memory", "Galvantech",
 "Gadzoox Nteworks", "Multi Dimensional Cons.", "GateField", "Integrated Memory System",
 "Triscend", "XaQti", "Goldenram", "Clear Logic",
 "Cimaron Communications", "Nippon Steel Semi. Corp.", "Advantage Memory", "AMCC",
 "LeCroy", "Yamaha Corporation", "Digital Microwave", "NetLogic Microsystems",
 "MIMOS Semiconductor", "Advanced Fibre", "BF Goodrich Data.", "Epigram",
 "Acbel Polytech Inc.", "Apacer Technology", "Admor Memory", "FOXCONN",
 "Quadratics Superconductor", "3COM"],
["Camintonn Corporation", "ISOA Incorporated", "Agate Semiconductor", "ADMtek Incorporated",
 "HYPERTEC", "Adhoc Technologies", "MOSAID Technologies", "Ardent Technologies",
 "Switchcore", "Cisco Systems, Inc.", "Allayer Technologies", "WorkX AG",
 "Oasis Semiconductor", "Novanet Semiconductor", "E-M Solutions", "Power General",
 "Advanced Hardware Arch.", "Inova Semiconductors GmbH", "Telocity", "Delkin Devices",
 "Symagery Microsystems", "C-Port Corporation", "SiberCore Technologies", "Southland Microsystems",
 "Malleable Technologies", "Kendin Communications", "Great Technology Microcomputer", "Sanmina Corporation",
 "HADCO Corporation", "Corsair", "Actrans System Inc.", "ALPHA Technologies",
 "Silicon Laboratories, Inc. (Cygnal)", "Artesyn Technologies", "Align Manufacturing", "Peregrine Semiconductor",
 "Chameleon Systems", "Aplus Flash Technology", "MIPS Technologies", "Chrysalis ITS",
 "ADTEC Corporation", "Kentron Technologies", "Win Technologies", "Tachyon Semiconductor (formerly ASIC Designs Inc.)",
 "Extreme Packet Devices", "RF Micro Devices", "Siemens AG", "Sarnoff Corporation",
 "Itautec Philco SA", "Radiata Inc.", "Benchmark Elect. (AVEX)", "Legend",
 "SpecTek Incorporated", "Hi/fn", "Enikia Incorporated", "SwitchOn Networks",
 "AANetcom Incorporated", "Micro Memory Bank", "ESS Technology", "Virata Corporation",
 "Excess Bandwidth", "West Bay Semiconductor", "DSP Group", "Newport Communications",
 "Chip2Chip Incorporated", "Phobos Corporation", "Intellitech Corporation", "Nordic VLSI ASA",
 "Ishoni Networks", "Silicon Spice", "Alchemy Semiconductor", "Agilent Technologies",
 "Centillium Communications", "W.L. Gore", "HanBit Electronics", "GlobeSpan",
 "Element 14", "Pycon", "Saifun Semiconductors", "Sibyte, Incorporated",
 "MetaLink Technologies", "Feiya Technology", "I & C Technology", "Shikatronics",
 "Elektrobit", "Megic", "Com-Tier", "Malaysia Micro Solutions",
 "Hyperchip", "Gemstone Communications", "Anadigm (formerly Anadyne)", "3ParData",
 "Mellanox Technologies", "Tenx Technologies", "Helix AG", "Domosys",
 "Skyup Technology", "HiNT Corporation", "Chiaro", "MCI Computer GMBH",
 "Exbit Technology A/S", "Integrated Technology Express", "AVED Memory", "Legerity",
 "Jasmine Networks", "Caspian Networks", "nCUBE", "Silicon Access Networks",
 "FDK Corporation", "High Bandwidth Access", "MultiLink Technology", "BRECIS",
 "World Wide Packets", "APW", "Chicory Systems", "Xstream Logic",
 "Fast-Chip", "Zucotto Wireless", "Realchip", "Galaxy Power",
 "eSilicon", "Morphics Technology", "Accelerant Networks", "Silicon Wave",
 "SandCraft", "Elpida"],
["Solectron", "Optosys Technologies", "Buffalo (Formerly Melco)", "TriMedia Technologies",
 "Cyan Technologies", "Global Locate", "Optillion", "Terago Communications",
 "Ikanos Communications", "Princeton Technology", "Nanya Technology", "Elite Flash Storage",
 "Mysticom", "LightSand Communications", "ATI Technologies", "Agere Systems",
 "NeoMagic", "AuroraNetics", "Golden Empire", "Mushkin",
 "Tioga Technologies", "Netlist", "TeraLogic", "Cicada Semiconductor",
 "Centon Electronics", "Tyco Electronics", "Magis Works", "Zettacom",
 "Cogency Semiconductor", "Chipcon AS", "Aspex Technology", "F5 Networks",
 "Programmable Silicon Solutions", "ChipWrights", "Acorn Networks", "Quicklogic",
 "Kingmax Semiconductor", "BOPS", "Flasys", "BitBlitz Communications",
 "eMemory Technology", "Procket Networks", "Purple Ray", "Trebia Networks",
 "Delta Electronics", "Onex Communications", "Ample Communications", "Memory Experts Intl",
 "Astute Networks", "Azanda Network Devices", "Dibcom", "Tekmos",
 "API NetWorks", "Bay Microsystems", "Firecron Ltd", "Resonext Communications",
 "Tachys Technologies", "Equator Technology", "Concept Computer", "SILCOM",
 "3Dlabs", "c't Magazine", "Sanera Systems", "Silicon Packets",
 "Viasystems Group", "Simtek", "Semicon Devices Singapore", "Satron Handelsges",
 "Improv Systems", "INDUSYS GmbH", "Corrent", "Infrant Technologies",
 "Ritek Corp", "empowerTel Networks", "Hypertec", "Cavium Networks",
 "PLX Technology", "Massana Design", "Intrinsity", "Valence Semiconductor",
 "Terawave Communications", "IceFyre Semiconductor", "Primarion", "Picochip Designs Ltd",
 "Silverback Systems", "Jade Star Technologies", "Pijnenburg Securealink",
 "TakeMS International AG", "Cambridge Silicon Radio",
 "Swissbit", "Nazomi Communications", "eWave System",
 "Rockwell Collins", "Picocel Co., Ltd.", "Alphamosaic Ltd", "Sandburst",
 "SiCon Video", "NanoAmp Solutions", "Ericsson Technology", "PrairieComm",
 "Mitac International", "Layer N Networks", "MtekVision", "Allegro Networks",
 "Marvell Semiconductors", "Netergy Microelectronic", "NVIDIA", "Internet Machines",
 "Peak Electronics", "Litchfield Communication", "Accton Technology", "Teradiant Networks",
 "Europe Technologies", "Cortina Systems", "RAM Components", "Raqia Networks",
 "ClearSpeed", "Matsushita Battery", "Xelerated", "SimpleTech",
 "Utron Technology", "Astec International", "AVM gmbH", "Redux Communications",
 "Dot Hill Systems", "TeraChip"],
["T-RAM Incorporated", "Innovics Wireless", "Teknovus", "KeyEye Communications",
 "Runcom Technologies", "RedSwitch", "Dotcast", "Silicon Mountain Memory",
 "Signia Technologies", "Pixim", "Galazar Networks", "White Electronic Designs",
 "Patriot Scientific", "Neoaxiom Corporation", "3Y Power Technology", "Europe Technologies",
 "Potentia Power Systems", "C-guys Incorporated", "Digital Communications Technology Incorporated", "Silicon-Based Technology",
 "Fulcrum Microsystems", "Positivo Informatica Ltd", "XIOtech Corporation", "PortalPlayer",
 "Zhiying Software", "Direct2Data", "Phonex Broadband", "Skyworks Solutions",
 "Entropic Communications", "Pacific Force Technology", "Zensys A/S", "Legend Silicon Corp.",
 "sci-worx GmbH", "Oasis Silicon Systems", "Renesas Technology", "Raza Microelectronics",
 "Phyworks", "MediaTek", "Non-cents Productions", "US Modular",
 "Wintegra Ltd", "Mathstar", "StarCore", "Oplus Technologies",
 "Mindspeed", "Just Young Computer", "Radia Communications", "OCZ",
 "Emuzed", "LOGIC Devices", "Inphi Corporation", "Quake Technologies",
 "Vixel", "SolusTek", "Kongsberg Maritime", "Faraday Technology",
 "Altium Ltd.", "Insyte", "ARM Ltd.", "DigiVision",
 "Vativ Technologies", "Endicott Interconnect Technologies", "Pericom", "Bandspeed",
 "LeWiz Communications", "CPU Technology", "Ramaxel Technology", "DSP Group",
 "Axis Communications", "Legacy Electronics", "Chrontel", "Powerchip Semiconductor",
 "MobilEye Technologies", "Excel Semiconductor", "A-DATA Technology", "VirtualDigm",
 "G Skill Intl", "Quanta Computer", "Yield Microelectronics", "Afa Technologies",
 "KINGBOX Technology Co. Ltd.", "Ceva", "iStor Networks", "Advance Modules",
 "Microsoft", "Open-Silicon", "Goal Semiconductor", "ARC International",
 "Simmtec", "Metanoia", "Key Stream", "Lowrance Electronics",
 "Adimos", "SiGe Semiconductor", "Fodus Communications", "Credence Systems Corp.",
 "Genesis Microchip Inc.", "Vihana, Inc.", "WIS Technologies", "GateChange Technologies",
 "High Density Devices AS", "Synopsys", "Gigaram", "Enigma Semiconductor Inc.",
 "Century Micro Inc.", "Icera Semiconductor", "Mediaworks Integrated Systems", "O'Neil Product Development",
 "Supreme Top Technology Ltd.", "MicroDisplay Corporation", "Team Group Inc.", "Sinett Corporation",
 "Toshiba Corporation", "Tensilica", "SiRF Technology", "Bacoc Inc.",
 "SMaL Camera Technologies", "Thomson SC", "Airgo Networks", "Wisair Ltd.",
 "SigmaTel", "Arkados", "Compete IT gmbH Co. KG", "Eudar Technology Inc.",
 "Focus Enhancements", "Xyratex"],
["Specular Networks", "Patriot Memory", "U-Chip Technology Corp.", "Silicon Optix",
 "Greenfield Networks", "CompuRAM GmbH", "Stargen, Inc.", "NetCell Corporation",
 "Excalibrus Technologies Ltd", "SCM Microsystems", "Xsigo Systems, Inc.", "CHIPS & Systems Inc",
 "Tier 1 Multichip Solutions", "CWRL Labs", "Teradici", "Gigaram, Inc.",
 "g2 Microsystems", "PowerFlash Semiconductor", "P.A. Semi, Inc.", "NovaTech Solutions, S.A.",
 "c2 Microsystems, Inc.", "Level5 Networks", "COS Memory AG", "Innovasic Semiconductor",
 "02IC Co. Ltd", "Tabula, Inc.", "Crucial Technology", "Chelsio Communications",
 "Solarflare Communications", "Xambala Inc.", "EADS Astrium", "ATO Semicon Co. Ltd.",
 "Imaging Works, Inc.", "Astute Networks, Inc.", "Tzero", "Emulex",
 "Power-One", "Pulse~LINK Inc.", "Hon Hai Precision Industry", "White Rock Networks Inc.",
 "Telegent Systems USA, Inc.", "Atrua Technologies, Inc.", "Acbel Polytech Inc.",
 "eRide Inc.","ULi Electronics Inc.", "Magnum Semiconductor Inc.", "neoOne Technology, Inc.",
 "Connex Technology, Inc.", "Stream Processors, Inc.", "Focus Enhancements", "Telecis Wireless, Inc.",
 "uNav Microelectronics", "Tarari, Inc.", "Ambric, Inc.", "Newport Media, Inc.", "VMTS",
 "Enuclia Semiconductor, Inc.", "Virtium Technology Inc.", "Solid State System Co., Ltd.", "Kian Tech LLC",
 "Artimi", "Power Quotient International", "Avago Technologies", "ADTechnology", "Sigma Designs",
 "SiCortex, Inc.", "Ventura Technology Group", "eASIC", "M.H.S. SAS", "Micro Star International", 
 "Rapport Inc.", "Makway International", "Broad Reach Engineering Co.", 
 "Semiconductor Mfg Intl Corp", "SiConnect", "FCI USA Inc.", "Validity Sensors", 
 "Coney Technology Co. Ltd.", "Spans Logic", "Neterion Inc.", "Qimonda",
 "New Japan Radio Co. Ltd.", "Velogix", "Montalvo Systems", "iVivity Inc.", "Walton Chaintech",
 "AENEON", "Lorom Industrial Co. Ltd.", "Radiospire Networks", "Sensio Technologies, Inc.",
 "Nethra Imaging", "Hexon Technology Pte Ltd", "CompuStocx (CSX)", "Methode Electronics, Inc.",
 "Connect One Ltd.", "Opulan Technologies", "Septentrio NV", "Goldenmars Technology Inc.",
 "Kreton Corporation", "Cochlear Ltd.", "Altair Semiconductor", "NetEffect, Inc.",
 "Spansion, Inc.", "Taiwan Semiconductor Mfg", "Emphany Systems Inc.",
 "ApaceWave Technologies", "Mobilygen Corporation", "Tego", "Cswitch Corporation",
 "Haier (Beijing) IC Design Co.", "MetaRAM", "Axel Electronics Co. Ltd."]);

$use_sysfs = -d '/sys/bus';

# We consider that no data was written to this area of the SPD EEPROM if
# all bytes read 0x00 or all bytes read 0xff
sub spd_written(@)
{
	my $all_00 = 1;
	my $all_ff = 1;
	
	foreach my $b (@_) {
		$all_00 = 0 unless $b == 0x00;
		$all_ff = 0 unless $b == 0xff;
		return 1 unless $all_00 or $all_ff;
	}

	return 0;
}

sub parity($)
{
	my $n = shift;
	my $parity = 0;

	while ($n) {
		$parity++ if ($n & 1);
		$n >>= 1;
	}

	return ($parity & 1);
}

sub manufacturer(@)
{
	my @bytes = @_;
	my $ai = 0;
	my $first;

	return ("Undefined", []) unless spd_written(@bytes);
	
	while (defined($first = shift(@bytes)) && $first == 0x7F) {
		$ai++;
	}

	return ("Invalid", []) unless defined $first;
	return ("Invalid", [$first, @bytes]) if parity($first) != 1;
	return ("Unknown", \@bytes) unless (($first & 0x7F) - 1 <= $vendors[$ai]);

	return ($vendors[$ai][($first & 0x7F) - 1], \@bytes);
}

sub manufacturer_data(@)
{
	my $hex = "";
	my $asc = "";

	return unless spd_written(@_);

	foreach my $byte (@_) {
		$hex .= sprintf("\%02X ", $byte);
		$asc .= ($byte >= 32 && $byte < 127) ? chr($byte) : '?';
	}

	return "$hex(\"$asc\")";
}

sub part_number(@)
{
	my $asc = "";
	my $byte;

	while (defined ($byte = shift) && $byte >= 32 && $byte < 127) {
		$asc .= chr($byte);
	}

	return ($asc eq "") ? "Undefined" : $asc;
}

sub printl ($$) # print a line w/ label and value
{
	my ($label, $value) = @_;
	if ($opt_html) {
		$label =~ s/</\&lt;/sg;
		$label =~ s/>/\&gt;/sg;
		$label =~ s/\n/<br>\n/sg;
		$value =~ s/</\&lt;/sg;
		$value =~ s/>/\&gt;/sg;
		$value =~ s/\n/<br>\n/sg;
		print "<tr><td valign=top>$label</td><td>$value</td></tr>\n";
	} else {
		my @values = split /\n/, $value;
		printf "%-47s %s\n", $label, shift @values;
		printf "%-47s %s\n", "", $_ foreach (@values);
	}
}

sub printl2 ($$) # print a line w/ label and value (outside a table)
{
	my ($label, $value) = @_;
	if ($opt_html) {
		$label =~ s/</\&lt;/sg;
		$label =~ s/>/\&gt;/sg;
		$label =~ s/\n/<br>\n/sg;
		$value =~ s/</\&lt;/sg;
		$value =~ s/>/\&gt;/sg;
		$value =~ s/\n/<br>\n/sg;
	}
	print "$label: $value\n";
}

sub prints ($) # print seperator w/ given text
{
	my ($label) = @_;
	if ($opt_html) {
		$label =~ s/</\&lt;/sg;
		$label =~ s/>/\&gt;/sg;
		$label =~ s/\n/<br>\n/sg;
		print "<tr><td align=center colspan=2><b>$label</b></td></tr>\n";
	} else {
		print "\n---=== $label ===---\n";
	}
}

sub printh ($$) # print header w/ given text
{
	my ($header, $sub) = @_;
	if ($opt_html) {
		$header =~ s/</\&lt;/sg;
		$header =~ s/>/\&gt;/sg;
		$header =~ s/\n/<br>\n/sg;
		$sub =~ s/</\&lt;/sg;
		$sub =~ s/>/\&gt;/sg;
		$sub =~ s/\n/<br>\n/sg;
		print "<h1>$header</h1>\n";
		print "<p>$sub</p>\n";
	} else {
		print "\n$header\n$sub\n";
	}
}

# Parameter: bytes 0-63
sub decode_sdr_sdram($)
{
	my $bytes = shift;
	my ($l, $temp);

# SPD revision
	printl "SPD Revision", $bytes->[62];

#size computation

	prints "Memory Characteristics";

	my $k=0;
	my $ii=0;
	
	$ii = ($bytes->[3] & 0x0f) + ($bytes->[4] & 0x0f) - 17;
	if (($bytes->[5] <= 8) && ($bytes->[17] <= 8)) {
		 $k = $bytes->[5] * $bytes->[17];
	}
	
	if($ii > 0 && $ii <= 12 && $k > 0) {
		printl "Size", ((1 << $ii) * $k) . " MB"; }
	else { 
		printl "INVALID SIZE", $bytes->[3] . "," . $bytes->[4] . "," .
				       $bytes->[5] . "," . $bytes->[17];
	}

	my @cas;
	for ($ii = 0; $ii < 7; $ii++) {
		push(@cas, $ii + 1) if ($bytes->[18] & (1 << $ii));
	}

	my $trcd;
	my $trp;
	my $tras;
	my $ctime = ($bytes->[9] >> 4) + ($bytes->[9] & 0xf) * 0.1;

	$trcd =$bytes->[29];
	$trp =$bytes->[27];;
	$tras =$bytes->[30];

	printl "tCL-tRCD-tRP-tRAS",
		$cas[$#cas] . "-" .
		ceil($trcd/$ctime) . "-" .
		ceil($trp/$ctime) . "-" .
		ceil($tras/$ctime);

	$l = "Number of Row Address Bits";
	if ($bytes->[3] == 0) { printl $l, "Undefined!"; }
	elsif ($bytes->[3] == 1) { printl $l, "1/16"; }
	elsif ($bytes->[3] == 2) { printl $l, "2/17"; }
	elsif ($bytes->[3] == 3) { printl $l, "3/18"; }
	else { printl $l, $bytes->[3]; }

	$l = "Number of Col Address Bits";
	if ($bytes->[4] == 0) { printl $l, "Undefined!"; }
	elsif ($bytes->[4] == 1) { printl $l, "1/16"; }
	elsif ($bytes->[4] == 2) { printl $l, "2/17"; }
	elsif ($bytes->[4] == 3) { printl $l, "3/18"; }
	else { printl $l, $bytes->[4]; }

	$l = "Number of Module Rows";
	if ($bytes->[5] == 0 ) { printl $l, "Undefined!"; }
	else { printl $l, $bytes->[5]; }

	$l = "Data Width";
	if ($bytes->[7] > 1) {
		printl $l, "Undefined!"
	} else {
		$temp = ($bytes->[7] * 256) + $bytes->[6];
		printl $l, $temp;
	}

	$l = "Module Interface Signal Levels";
	if ($bytes->[8] == 0) { printl $l, "5.0 Volt/TTL"; }
	elsif ($bytes->[8] == 1) { printl $l, "LVTTL"; }
	elsif ($bytes->[8] == 2) { printl $l, "HSTL 1.5"; }
	elsif ($bytes->[8] == 3) { printl $l, "SSTL 3.3"; }
	elsif ($bytes->[8] == 4) { printl $l, "SSTL 2.5"; }
	elsif ($bytes->[8] == 255) { printl $l, "New Table"; }
	else { printl $l, "Undefined!"; }

	$l = "Module Configuration Type";
	if ($bytes->[11] == 0) { printl $l, "No Parity"; }
	elsif ($bytes->[11] == 1) { printl $l, "Parity"; }
	elsif ($bytes->[11] == 2) { printl $l, "ECC"; }
	else { printl $l, "Undefined!"; }

	$l = "Refresh Type";
	if ($bytes->[12] > 126) { printl $l, "Self Refreshing"; }
	else { printl $l, "Not Self Refreshing"; }

	$l = "Refresh Rate";
	$temp = $bytes->[12] & 0x7f;
	if ($temp == 0) { printl $l, "Normal (15.625 us)"; }
	elsif ($temp == 1) { printl $l, "Reduced (3.9 us)"; }
	elsif ($temp == 2) { printl $l, "Reduced (7.8 us)"; }
	elsif ($temp == 3) { printl $l, "Extended (31.3 us)"; }
	elsif ($temp == 4) { printl $l, "Extended (62.5 us)"; }
	elsif ($temp == 5) { printl $l, "Extended (125 us)"; }
	else { printl $l, "Undefined!"; }

	$l = "Primary SDRAM Component Bank Config";
	if ($bytes->[13] > 126) { printl $l, "Bank2 = 2 x Bank1"; }
	else { printl $l, "No Bank2 OR Bank2 = Bank1 width"; }

	$l = "Primary SDRAM Component Widths";
	$temp = $bytes->[13] & 0x7f;
	if ($temp == 0) { printl $l, "Undefined!\n"; }
	else { printl $l, $temp; }

	$l = "Error Checking SDRAM Component Bank Config";
	if ($bytes->[14] > 126) { printl $l, "Bank2 = 2 x Bank1"; }
	else { printl $l, "No Bank2 OR Bank2 = Bank1 width"; }

	$l = "Error Checking SDRAM Component Widths";
	$temp = $bytes->[14] & 0x7f;
	if ($temp == 0) { printl $l, "Undefined!"; }
	else { printl $l, $temp; }

	$l = "Min Clock Delay for Back to Back Random Access";
	if ($bytes->[15] == 0) { printl $l, "Undefined!"; }
	else { printl $l, $bytes->[15]; }

	$l = "Burst lengths supported";
	my @array;
	for ($ii = 0; $ii < 4; $ii++) {
		push(@array, 1 << $ii) if ($bytes->[16] & (1 << $ii));
	}
	push(@array, "Page") if ($bytes->[16] & 128);
	if (@array) { $temp = join ', ', @array; }
	else { $temp = "None"; }
	printl $l, $temp;

	$l = "Number of Device Banks";
	if ($bytes->[17] == 0) { printl $l, "Undefined/Reserved!"; }
	else { printl $l, $bytes->[17]; }

	$l = "Supported CAS Latencies";
	if (@cas) { $temp = join ', ', @cas; }
	else { $temp = "None"; }
	printl $l, $temp;

	$l = "Supported CS Latencies";
	@array = ();
	for ($ii = 0; $ii < 7; $ii++) {
		push(@array, $ii) if ($bytes->[19] & (1 << $ii));
	}
	if (@array) { $temp = join ', ', @array; }
	else { $temp = "None"; }
	printl $l, $temp;

	$l = "Supported WE Latencies";
	@array = ();
	for ($ii = 0; $ii < 7; $ii++) {
		push(@array, $ii) if ($bytes->[20] & (1 << $ii));
	}
	if (@array) { $temp = join ', ', @array; }
	else { $temp = "None"; }
	printl $l, $temp;

	if (@cas >= 1) {
		$l = "Cycle Time (CAS ".$cas[$#cas].")";
		printl $l, "$ctime ns";

		$l = "Access Time (CAS ".$cas[$#cas].")";
		$temp = ($bytes->[10] >> 4) + ($bytes->[10] & 0xf) * 0.1;
		printl $l, "$temp ns";
	}

	if (@cas >= 2 && spd_written(@$bytes[23..24])) {
		$l = "Cycle Time (CAS ".$cas[$#cas-1].")";
		$temp = $bytes->[23] >> 4;
		if ($temp == 0) { printl $l, "Undefined!"; }
		else {
			if ($temp < 4 ) { $temp=$temp + 15; }
			printl $l, $temp + (($bytes->[23] & 0xf) * 0.1) . " ns";
		}

		$l = "Access Time (CAS ".$cas[$#cas-1].")";
		$temp = $bytes->[24] >> 4;
		if ($temp == 0) { printl $l, "Undefined!"; }
		else {
			if ($temp < 4 ) { $temp=$temp + 15; }
			printl $l, $temp + (($bytes->[24] & 0xf) * 0.1) . " ns";
		}
	}

	if (@cas >= 3 && spd_written(@$bytes[25..26])) {
		$l = "Cycle Time (CAS ".$cas[$#cas-2].")";
		$temp = $bytes->[25] >> 2;
		if ($temp == 0) { printl $l, "Undefined!"; }
		else { printl $l, $temp + ($bytes->[25] & 0x3) * 0.25 . " ns"; }

		$l = "Access Time (CAS ".$cas[$#cas-2].")";
		$temp = $bytes->[26] >> 2;
		if ($temp == 0) { printl $l, "Undefined!"; }
		else { printl $l, $temp + ($bytes->[26] & 0x3) * 0.25 . " ns"; }
	}

	$l = "SDRAM Module Attributes";
	$temp = "";
	if ($bytes->[21] & 1) { $temp .= "Buffered Address/Control Inputs\n"; }
	if ($bytes->[21] & 2) { $temp .= "Registered Address/Control Inputs\n"; }
	if ($bytes->[21] & 4) { $temp .= "On card PLL (clock)\n"; }
	if ($bytes->[21] & 8) { $temp .= "Buffered DQMB Inputs\n"; }
	if ($bytes->[21] & 16) { $temp .= "Registered DQMB Inputs\n"; }
	if ($bytes->[21] & 32) { $temp .= "Differential Clock Input\n"; }
	if ($bytes->[21] & 64) { $temp .= "Redundant Row Address\n"; }
	if ($bytes->[21] & 128) { $temp .= "Undefined (bit 7)\n"; }
	if ($bytes->[21] == 0) { $temp .= "(None Reported)\n"; }
	printl $l, $temp;

	$l = "SDRAM Device Attributes (General)";
	$temp = "";
	if ($bytes->[22] & 1) { $temp .= "Supports Early RAS# Recharge\n"; }
	if ($bytes->[22] & 2) { $temp .= "Supports Auto-Precharge\n"; }
	if ($bytes->[22] & 4) { $temp .= "Supports Precharge All\n"; }
	if ($bytes->[22] & 8) { $temp .= "Supports Write1/Read Burst\n"; }
	if ($bytes->[22] & 16) { $temp .= "Lower VCC Tolerance: 5%\n"; }
	else { $temp .= "Lower VCC Tolerance: 10%\n"; }
	if ($bytes->[22] & 32) { $temp .= "Upper VCC Tolerance: 5%\n"; }
	else { $temp .= "Upper VCC Tolerance: 10%\n"; }
	if ($bytes->[22] & 64) { $temp .= "Undefined (bit 6)\n"; }
	if ($bytes->[22] & 128) { $temp .= "Undefined (bit 7)\n"; }
	printl $l, $temp;

	$l = "Minimum Row Precharge Time";
	if ($bytes->[27] == 0) { printl $l, "Undefined!"; }
	else { printl $l, "$bytes->[27] ns"; }

	$l = "Row Active to Row Active Min";
	if ($bytes->[28] == 0) { printl $l, "Undefined!"; }
	else { printl $l, "$bytes->[28] ns"; }

	$l = "RAS to CAS Delay";
	if ($bytes->[29] == 0) { printl $l, "Undefined!"; }
	else { printl $l, "$bytes->[29] ns"; }

	$l = "Min RAS Pulse Width";
	if ($bytes->[30] == 0) { printl $l, "Undefined!"; }
	else { printl $l, "$bytes->[30] ns"; }

	$l = "Row Densities";
	$temp = "";
	if ($bytes->[31] & 1) { $temp .= "4 MByte\n"; }
	if ($bytes->[31] & 2) { $temp .= "8 MByte\n"; }
	if ($bytes->[31] & 4) { $temp .= "16 MByte\n"; }
	if ($bytes->[31] & 8) { $temp .= "32 MByte\n"; }
	if ($bytes->[31] & 16) { $temp .= "64 MByte\n"; }
	if ($bytes->[31] & 32) { $temp .= "128 MByte\n"; }
	if ($bytes->[31] & 64) { $temp .= "256 MByte\n"; }
	if ($bytes->[31] & 128) { $temp .= "512 MByte\n"; }
	if ($bytes->[31] == 0) { $temp .= "(Undefined! -- None Reported!)\n"; }
	printl $l, $temp;

	if (($bytes->[32] & 0xf) <= 9) {
		$l = "Command and Address Signal Setup Time";
		$temp = (($bytes->[32] & 0x7f) >> 4) + ($bytes->[32] & 0xf) * 0.1;
		printl $l, (($bytes->[32] >> 7) ? -$temp : $temp) . " ns";
	}

	if (($bytes->[33] & 0xf) <= 9) {
		$l = "Command and Address Signal Hold Time";
		$temp = (($bytes->[33] & 0x7f) >> 4) + ($bytes->[33] & 0xf) * 0.1;
		printl $l, (($bytes->[33] >> 7) ? -$temp : $temp) . " ns";
	}

	if (($bytes->[34] & 0xf) <= 9) {
		$l = "Data Signal Setup Time";
		$temp = (($bytes->[34] & 0x7f) >> 4) + ($bytes->[34] & 0xf) * 0.1;
		printl $l, (($bytes->[34] >> 7) ? -$temp : $temp) . " ns";
	}

	if (($bytes->[35] & 0xf) <= 9) {
		$l = "Data Signal Hold Time";
		$temp = (($bytes->[35] & 0x7f) >> 4) + ($bytes->[35] & 0xf) * 0.1;
		printl $l, (($bytes->[35] >> 7) ? -$temp : $temp) . " ns";
	}
}

# Parameter: bytes 0-63
sub decode_ddr_sdram($)
{
	my $bytes = shift;
	my ($l, $temp);

# SPD revision
	if ($bytes->[62] != 0xff) {
		printl "SPD Revision", ($bytes->[62] >> 4) . "." .
				       ($bytes->[62] & 0xf);
	}

# speed
	prints "Memory Characteristics";

	$l = "Maximum module speed";
	$temp = ($bytes->[9] >> 4) + ($bytes->[9] & 0xf) * 0.1;
	my $ddrclk = 2 * (1000 / $temp);
	my $tbits = ($bytes->[7] * 256) + $bytes->[6];
	if (($bytes->[11] == 2) || ($bytes->[11] == 1)) { $tbits = $tbits - 8; }
	my $pcclk = int ($ddrclk * $tbits / 8);
	$pcclk += 100 if ($pcclk % 100) >= 50; # Round properly
	$pcclk = $pcclk - ($pcclk % 100);
	$ddrclk = int ($ddrclk);
	printl $l, "${ddrclk}MHz (PC${pcclk})";

#size computation
	my $k=0;
	my $ii=0;
	
	$ii = ($bytes->[3] & 0x0f) + ($bytes->[4] & 0x0f) - 17;
	if (($bytes->[5] <= 8) && ($bytes->[17] <= 8)) {
		 $k = $bytes->[5] * $bytes->[17];
	}
	
	if($ii > 0 && $ii <= 12 && $k > 0) {
		printl "Size", ((1 << $ii) * $k) . " MB"; }
	else { 
		printl "INVALID SIZE", $bytes->[3] . ", " . $bytes->[4] . ", " .
				       $bytes->[5] . ", " . $bytes->[17];
	}

	my $highestCAS = 0;
	my %cas;
	for ($ii = 0; $ii < 7; $ii++) {
		if ($bytes->[18] & (1 << $ii)) {
			$highestCAS = 1+$ii*0.5;
			$cas{$highestCAS}++;
		}
	}

	my $trcd;
	my $trp;
	my $tras;
	my $ctime = ($bytes->[9] >> 4) + ($bytes->[9] & 0xf) * 0.1;

	$trcd =($bytes->[29] >> 2)+(($bytes->[29] & 3)*0.25);
	$trp =($bytes->[27] >> 2)+(($bytes->[27] & 3)*0.25);
	$tras = $bytes->[30];

	printl "tCL-tRCD-tRP-tRAS",
		$highestCAS . "-" .
		ceil($trcd/$ctime) . "-" .
		ceil($trp/$ctime) . "-" .
		ceil($tras/$ctime);

# latencies
	if (keys %cas) { $temp = join ', ', sort { $b <=> $a } keys %cas; }
	else { $temp = "None"; }
	printl "Supported CAS Latencies", $temp;

	my @array;
	for ($ii = 0; $ii < 7; $ii++) {
		push(@array, $ii) if ($bytes->[19] & (1 << $ii));
	}
	if (@array) { $temp = join ', ', @array; }
	else { $temp = "None"; }
	printl "Supported CS Latencies", $temp;

	@array = ();
	for ($ii = 0; $ii < 7; $ii++) {
		push(@array, $ii) if ($bytes->[20] & (1 << $ii));
	}
	if (@array) { $temp = join ', ', @array; }
	else { $temp = "None"; }
	printl "Supported WE Latencies", $temp;

# timings
	if (exists $cas{$highestCAS}) {
		printl "Minimum Cycle Time (CAS $highestCAS)",
		       "$ctime ns";

		printl "Maximum Access Time (CAS $highestCAS)",
		       (($bytes->[10] >> 4) * 0.1 + ($bytes->[10] & 0xf) * 0.01) . " ns";
	}

	if (exists $cas{$highestCAS-0.5} && spd_written(@$bytes[23..24])) {
		printl "Minimum Cycle Time (CAS ".($highestCAS-0.5).")",
		       (($bytes->[23] >> 4) + ($bytes->[23] & 0xf) * 0.1) . " ns";

		printl "Maximum Access Time (CAS ".($highestCAS-0.5).")",
		       (($bytes->[24] >> 4) * 0.1 + ($bytes->[24] & 0xf) * 0.01) . " ns";
	}

	if (exists $cas{$highestCAS-1} && spd_written(@$bytes[25..26])) {
		printl "Minimum Cycle Time (CAS ".($highestCAS-1).")",
		       (($bytes->[25] >> 4) + ($bytes->[25] & 0xf) * 0.1) . " ns";

		printl "Maximum Access Time (CAS ".($highestCAS-1).")",
		       (($bytes->[26] >> 4) * 0.1 + ($bytes->[26] & 0xf) * 0.01) . " ns";
	}

# module attributes
	if ($bytes->[47] & 0x03) {
		if (($bytes->[47] & 0x03) == 0x01) { $temp = "1.125\" to 1.25\""; }
		elsif (($bytes->[47] & 0x03) == 0x02) { $temp = "1.7\""; }
		elsif (($bytes->[47] & 0x03) == 0x03) { $temp = "Other"; }
		printl "Module Height", $temp;
	}
}

sub ddr2_sdram_ctime($)
{
	my $byte = shift;
	my $ctime;
	
	$ctime = $byte >> 4;
	if (($byte & 0xf) <= 9) { $ctime += ($byte & 0xf) * 0.1; }
	elsif (($byte & 0xf) == 10) { $ctime += 0.25; }
	elsif (($byte & 0xf) == 11) { $ctime += 0.33; }
	elsif (($byte & 0xf) == 12) { $ctime += 0.66; }
	elsif (($byte & 0xf) == 13) { $ctime += 0.75; }

	return $ctime;
}

sub ddr2_sdram_atime($)
{
	my $byte = shift;
	my $atime;
	
	$atime = ($byte >> 4) * 0.1 + ($byte & 0xf) * 0.01;

	return $atime;
}

# Parameter: bytes 0-63
sub decode_ddr2_sdram($)
{
	my $bytes = shift;
	my ($l, $temp);
	my $ctime;

# SPD revision
	if ($bytes->[62] != 0xff) {
		printl "SPD Revision", ($bytes->[62] >> 4) . "." .
				       ($bytes->[62] & 0xf);
	}

# speed
	prints "Memory Characteristics";

	$l = "Maximum module speed";
	$ctime = ddr2_sdram_ctime($bytes->[9]);
	my $ddrclk = 2 * (1000 / $ctime);
	my $tbits = ($bytes->[7] * 256) + $bytes->[6];
	if ($bytes->[11] & 0x03) { $tbits = $tbits - 8; }
	my $pcclk = int ($ddrclk * $tbits / 8);
	# Round down to comply with Jedec
	$pcclk = $pcclk - ($pcclk % 100);
	$ddrclk = int ($ddrclk);
	printl $l, "${ddrclk}MHz (PC2-${pcclk})";

#size computation
	my $k=0;
	my $ii=0;

	$ii = ($bytes->[3] & 0x0f) + ($bytes->[4] & 0x0f) - 17;
	$k = (($bytes->[5] & 0x7) + 1) * $bytes->[17];
	
	if($ii > 0 && $ii <= 12 && $k > 0) {
		printl "Size", ((1 << $ii) * $k) . " MB"; 
	} else {
		printl "INVALID SIZE", $bytes->[3] . "," . $bytes->[4] . "," .
				       $bytes->[5] . "," . $bytes->[17];
	}

	my $highestCAS = 0;
	my %cas;
	for ($ii = 2; $ii < 7; $ii++) {
		if ($bytes->[18] & (1 << $ii)) {
			$highestCAS = $ii;
			$cas{$highestCAS}++;
		}
	}

	my $trcd;
	my $trp;
	my $tras;
	
	$trcd =($bytes->[29] >> 2)+(($bytes->[29] & 3)*0.25);
	$trp =($bytes->[27] >> 2)+(($bytes->[27] & 3)*0.25);
	$tras =$bytes->[30];

	printl "tCL-tRCD-tRP-tRAS",
		$highestCAS . "-" .
		ceil($trcd/$ctime) . "-" .
		ceil($trp/$ctime) . "-" .
		ceil($tras/$ctime);

# latencies
	if (keys %cas) { $temp = join ', ', sort { $b <=> $a } keys %cas; }
	else { $temp = "None"; }
	printl "Supported CAS Latencies", $temp;

# timings
	if (exists $cas{$highestCAS}) {
		printl "Minimum Cycle Time (CAS $highestCAS)",
		       "$ctime ns";
		printl "Maximum Access Time (CAS $highestCAS)",
		       ddr2_sdram_atime($bytes->[10]) . " ns";
	}

	if (exists $cas{$highestCAS-1} && spd_written(@$bytes[23..24])) {
		printl "Minimum Cycle Time (CAS ".($highestCAS-1).")",
		       ddr2_sdram_ctime($bytes->[23]) . " ns";
		printl "Maximum Access Time (CAS ".($highestCAS-1).")",
		       ddr2_sdram_atime($bytes->[24]) . " ns";
	}

	if (exists $cas{$highestCAS-2} && spd_written(@$bytes[25..26])) {
		printl "Minimum Cycle Time (CAS ".($highestCAS-2).")",
		       ddr2_sdram_ctime($bytes->[25]) . " ns";
		printl "Maximum Access Time (CAS ".($highestCAS-2).")",
		       ddr2_sdram_atime($bytes->[26]) . " ns";
	}
}

# Parameter: bytes 0-63
sub decode_direct_rambus($)
{
	my $bytes = shift;

#size computation
	prints "Memory Characteristics";

	my $ii;

	$ii = ($bytes->[4] & 0x0f) + ($bytes->[4] >> 4) + ($bytes->[5] & 0x07) - 13;
	
	if ($ii > 0 && $ii < 16) {
		printl "Size", (1 << $ii) . " MB";
	} else {
		printl "INVALID SIZE", sprintf("0x%02x, 0x%02x",
					       $bytes->[4], $bytes->[5]);
	}
}

# Parameter: bytes 0-63
sub decode_rambus($)
{
	my $bytes = shift;

#size computation
	prints "Memory Characteristics";

	my $ii;
	
	$ii = ($bytes->[3] & 0x0f) + ($bytes->[3] >> 4) + ($bytes->[5] & 0x07) - 13;
	
	if ($ii > 0 && $ii < 16) {
		printl "Size", (1 << $ii) . " MB";
	} else {
		printl "INVALID SIZE", sprintf("0x%02x, 0x%02x",
					       $bytes->[3], $bytes->[5]);
	}
}

%decode_callback = (
	"SDR SDRAM"	=> \&decode_sdr_sdram,
	"DDR SDRAM"	=> \&decode_ddr_sdram,
	"DDR2 SDRAM"	=> \&decode_ddr2_sdram,
	"Direct Rambus"	=> \&decode_direct_rambus,
	"Rambus"	=> \&decode_rambus,
);

# Parameter: bytes 64-127
sub decode_intel_spec_freq($)
{
	my $bytes = shift;
	my ($l, $temp);

	prints "Intel Specification";

	$l = "Frequency";
	if ($bytes->[62] == 0x66) { $temp = "66MHz\n"; }
	elsif ($bytes->[62] == 100) { $temp = "100MHz or 133MHz\n"; }
	elsif ($bytes->[62] == 133) { $temp = "133MHz\n"; }
	else { $temp = "Undefined!\n"; }
	printl $l, $temp;

	$l = "Details for 100MHz Support";
	$temp="";
	if ($bytes->[63] & 1) { $temp .= "Intel Concurrent Auto-precharge\n"; }
	if ($bytes->[63] & 2) { $temp .= "CAS Latency = 2\n"; }
	if ($bytes->[63] & 4) { $temp .= "CAS Latency = 3\n"; }
	if ($bytes->[63] & 8) { $temp .= "Junction Temp A (100 degrees C)\n"; }
	else { $temp .= "Junction Temp B (90 degrees C)\n"; }
	if ($bytes->[63] & 16) { $temp .= "CLK 3 Connected\n"; }
	if ($bytes->[63] & 32) { $temp .= "CLK 2 Connected\n"; }
	if ($bytes->[63] & 64) { $temp .= "CLK 1 Connected\n"; }
	if ($bytes->[63] & 128) { $temp .= "CLK 0 Connected\n"; }
	if (($bytes->[63] & 192) == 192) { $temp .= "Double-sided DIMM\n"; }
	elsif (($bytes->[63] & 192) != 0) { $temp .= "Single-sided DIMM\n"; }
	printl $l, $temp;
}

sub readspd64 ($$) { # reads 64 bytes from SPD-EEPROM
	my ($offset, $dimm_i) = @_;
	my @bytes;
	if ($use_sysfs) {
		# Kernel 2.6 with sysfs
		sysopen(HANDLE, "/sys/bus/i2c/drivers/eeprom/$dimm_i/eeprom", O_RDONLY)
			or die "Cannot open /sys/bus/i2c/drivers/eeprom/$dimm_i/eeprom";
		binmode HANDLE;
		sysseek(HANDLE, $offset, SEEK_SET);
		sysread(HANDLE, my $eeprom, 64);
		close HANDLE;
		@bytes = unpack("C64", $eeprom);
	} else {
		# Kernel 2.4 with procfs
		for my $i (0 .. 3) {
			my $hexoff = sprintf('%02x', $offset + $i * 16);
			push @bytes, split(" ", `cat /proc/sys/dev/sensors/$dimm_i/$hexoff`);
		}
	}
	return @bytes;
}

for (@ARGV) {
    if (/-h/) {
		print "Usage: $0 [-c] [-f [-b]]\n",
			"       $0 -h\n\n",
			"  -f, --format            print nice html output\n",
			"  -b, --bodyonly          don't print html header\n",
			"                          (useful for postprocessing the output)\n",
			"  -c, --checksum          decode completely even if checksum fails\n",
			"  -h, --help              display this usage summary\n";
		exit;
    }
    $opt_html = 1 if (/-f/);
    $opt_bodyonly = 1 if (/-b/);
    $opt_igncheck = 1 if (/-c/);
}
$opt_body = $opt_html && ! $opt_bodyonly;

if ($opt_body)
{
	print "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n",
	      "<html><head>\n",
		  "\t<meta HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">\n",
		  "\t<title>PC DIMM Serial Presence Detect Tester/Decoder Output</title>\n",
		  "</head><body>\n";
}

printh 'Memory Serial Presence Detect Decoder',
'By Philip Edelbrock, Christian Zuckschwerdt, Burkart Lingner,
Jean Delvare and others
Version 2.10.2';


my $dimm_count=0;
my @dimm_list;
my $dir;
if ($use_sysfs) { $dir = '/sys/bus/i2c/drivers/eeprom'; }
else { $dir = '/proc/sys/dev/sensors'; }
if (-d $dir) {
	@dimm_list = split(/\s+/, `ls $dir`);
} elsif (! -d '/sys/module/eeprom') {
	print "No EEPROM found, are you sure the eeprom module is loaded?\n";
	exit;
}

for my $i ( 0 .. $#dimm_list ) {
	$_=$dimm_list[$i];
	if (($use_sysfs && /^\d+-\d+$/)
	 || (!$use_sysfs && /^eeprom-/)) {
		print "<b><u>" if $opt_html;
		printl2 "\n\nDecoding EEPROM", ($use_sysfs ?
			"/sys/bus/i2c/drivers/eeprom/$dimm_list[$i]" :
			"/proc/sys/dev/sensors/$dimm_list[$i]");
		print "</u></b>" if $opt_html;
		print "<table border=1>\n" if $opt_html;
		if (($use_sysfs && /^[^-]+-([^-]+)$/)
		 || (!$use_sysfs && /^[^-]+-[^-]+-[^-]+-([^-]+)$/)) {
			my $dimm_num=$1 - 49;
			printl "Guessing DIMM is in", "bank $dimm_num";
		}

# Decode first 3 bytes (0-2)
		prints "SPD EEPROM Information";

		my @bytes = readspd64(0, $dimm_list[$i]);
		my $dimm_checksum = 0;
		$dimm_checksum += $bytes[$_] foreach (0 .. 62);
		$dimm_checksum &= 0xff;

		my $l = "EEPROM Checksum of bytes 0-62";
		printl $l, ($bytes[63] == $dimm_checksum ?
			sprintf("OK (0x%.2X)", $bytes[63]):
			sprintf("Bad\n(found 0x%.2X, calculated 0x%.2X)\n",
				$bytes[63], $dimm_checksum));

		unless ($bytes[63] == $dimm_checksum or $opt_igncheck) {
			print "</table>\n" if $opt_html;
			next;
		}
		
		$dimm_count++;
		# Simple heuristic to detect Rambus
		my $is_rambus = $bytes[0] < 4;
		my $temp;
		if ($is_rambus) {
			if ($bytes[0] == 1) { $temp = "0.7"; }
			elsif ($bytes[0] == 2) { $temp = "1.0"; }
			elsif ($bytes[0] == 0 || $bytes[0] == 255) { $temp = "Invalid"; }
			else { $temp = "Reserved"; }
			printl "SPD Revision", $temp;
		} else {
			printl "# of bytes written to SDRAM EEPROM",
			       $bytes[0];
		}

		$l = "Total number of bytes in EEPROM";
		if ($bytes[1] <= 14) {
			printl $l, 2**$bytes[1];
		} elsif ($bytes[1] == 0) {
			printl $l, "RFU"; 
		} else { printl $l, "ERROR!"; }

		$l = "Fundamental Memory type";
		my $type = "Unknown";
		if ($is_rambus) {
			if ($bytes[2] == 1) { $type = "Direct Rambus"; }
			elsif ($bytes[2] == 17) { $type = "Rambus"; }
		} else {
			if ($bytes[2] == 1) { $type = "FPM DRAM"; }
			elsif ($bytes[2] == 2) { $type = "EDO"; }
			elsif ($bytes[2] == 3) { $type = "Pipelined Nibble"; }
			elsif ($bytes[2] == 4) { $type = "SDR SDRAM"; }
			elsif ($bytes[2] == 5) { $type = "Multiplexed ROM"; }
			elsif ($bytes[2] == 6) { $type = "DDR SGRAM"; }
			elsif ($bytes[2] == 7) { $type = "DDR SDRAM"; }
			elsif ($bytes[2] == 8) { $type = "DDR2 SDRAM"; }
		}
		printl $l, $type;

# Decode next 61 bytes (3-63, depend on memory type)
		$decode_callback{$type}->(\@bytes)
			if exists $decode_callback{$type};

# Decode next 35 bytes (64-98, common to all memory types)
		prints "Manufacturing Information";

		@bytes = readspd64(64, $dimm_list[$i]);
		
		$l = "Manufacturer";
		# $extra is a reference to an array containing up to
		# 7 extra bytes from the Manufacturer field. Sometimes
		# these bytes are filled with interesting data.
		($temp, my $extra) = manufacturer(@bytes[0..7]);
		printl $l, $temp;
		$l = "Custom Manufacturer Data";
		$temp = manufacturer_data(@{$extra});
		printl $l, $temp if defined $temp;
		
		if (spd_written($bytes[8])) {
			# Try the location code as ASCII first, as earlier specifications
			# suggested this. As newer specifications don't mention it anymore,
			# we still fall back to binary.
			$l = "Manufacturing Location Code";
			$temp = (chr($bytes[8]) =~ m/^[\w\d]$/) ? chr($bytes[8])
			      : sprintf("0x%.2X", $bytes[8]);
			printl $l, $temp;
		}
		
		$l = "Part Number";
		$temp = part_number(@bytes[9..26]);
		printl $l, $temp;
		
		if (spd_written(@bytes[27..28])) {
			$l = "Revision Code";
			$temp = sprintf("0x%02X%02X\n", @bytes[27..28]);
			printl $l, $temp;
		}
		
		if (spd_written(@bytes[29..30])) {
			$l = "Manufacturing Date";
			# In theory the year and week are in BCD format, but
			# this is not always true in practice :(
			if (($bytes[29] & 0xf0) <= 0x90
			 && ($bytes[29] & 0x0f) <= 0x09
			 && ($bytes[30] & 0xf0) <= 0x90
			 && ($bytes[30] & 0x0f) <= 0x09) {
				# Note that this heuristic will break in year 2080
				$temp = sprintf("%d%02X-W%02X\n",
						$bytes[29] >= 0x80 ? 19 : 20,
						@bytes[29..30]);
			} else {
				$temp = sprintf("0x%02X%02X\n",
						@bytes[29..30]);
			}
			printl $l, $temp;
		}
		
		if (spd_written(@bytes[31..34])) {
			$l = "Assembly Serial Number";
			$temp = sprintf("0x%02X%02X%02X%02X\n",
					@bytes[31..34]);
			printl $l, $temp;
		}

# Next 27 bytes (99-125) are manufacturer specific, can't decode

# Last 2 bytes (126-127) are reserved, Intel used them as an extension
		if ($type eq "SDR SDRAM") {
			decode_intel_spec_freq(\@bytes);
		}
		
		print "</table>\n" if $opt_html;
	}
}
printl2 "\n\nNumber of SDRAM DIMMs detected and decoded", $dimm_count;

print "</body></html>\n" if $opt_body;
