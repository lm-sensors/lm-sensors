/*
    main.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>

#include "lib/sensors.h" 
#include "lib/error.h"
#include "chips.h"
#include "version.h"

#define PROGRAM "sensors"
#define VERSION LM_VERSION
#define DEFAULT_CONFIG_FILE_NAME "sensors.conf"

FILE *config_file;
extern const char *libsensors_version;

extern int main(int argc, char *arv[]);
static void print_short_help(void);
static void print_long_help(void);
static void print_version(void);
static void do_a_print(sensors_chip_name name);
static int do_a_set(sensors_chip_name name);
static int do_the_real_work(int *error);
static const char *sprintf_chip_name(sensors_chip_name name);

#define CHIPS_MAX 20
sensors_chip_name chips[CHIPS_MAX];
int chips_count=0;
int do_sets, do_unknown, fahrenheit, show_algorithm, hide_adapter, hide_unknown;

char degstr[5]; /* store the correct string to print degrees */

void print_short_help(void)
{
  printf("Try `%s -h' for more information\n",PROGRAM);
}

void print_long_help(void)
{
  printf("Usage: %s [OPTION]... [CHIP]...\n",PROGRAM);
  printf("  -c, --config-file     Specify a config file (default: " ETCDIR "/" DEFAULT_CONFIG_FILE_NAME ")\n");
  printf("  -h, --help            Display this help text\n");
  printf("  -s, --set             Execute `set' statements too (root only)\n");
  printf("  -f, --fahrenheit      Show temperatures in degrees fahrenheit\n");
  printf("  -a, --algorithm       Show algorithm for each chip\n");
  printf("  -A, --no-adapter      Do not show adapter for each chip\n");
  printf("  -U, --no-unknown      Do not show unknown chips\n");
  printf("  -u, --unknown         Treat chips as unknown ones (testing only)\n");
  printf("  -v, --version         Display the program version\n");
  printf("\n");
  printf("Use `-' after `-c' to read the config file from stdin.\n");
  printf("If no chips are specified, all chip info will be printed.\n");
  printf("Example chip names:\n");
  printf("\tlm78-i2c-0-2d\t*-i2c-0-2d\n");
  printf("\tlm78-i2c-0-*\t*-i2c-0-*\n");
  printf("\tlm78-i2c-*-2d\t*-i2c-*-2d\n");
  printf("\tlm78-i2c-*-*\t*-i2c-*-*\n");
  printf("\tlm78-isa-0290\t*-isa-0290\n");
  printf("\tlm78-isa-*\t*-isa-*\n");
  printf("\tlm78-*\n");
}

void print_version(void)
{
  printf("%s version %s with libsensors version %s\n", PROGRAM, VERSION, libsensors_version);
}

/* This examines global var config_file, and leaves the name there too. 
   It also opens config_file. */
void open_config_file(const char* config_file_name)
{
  if (!strcmp(config_file_name,"-")) {
    config_file = stdin;
    return;
  }

  config_file = fopen(config_file_name, "r");
  if (!config_file) {
    fprintf(stderr, "Could not open config file\n");
    perror(config_file_name);
    exit(1);
  }
}
    
void close_config_file(const char* config_file_name)
{
  if (fclose(config_file) == EOF) {
    fprintf(stderr,"Could not close config file\n");
    perror(config_file_name);
  }
}

static void set_degstr(void)
{
  /* Size hardcoded for better performance.
     Don't forget to count the trailing \0! */
  size_t deg_latin1_size = 3;
  char *deg_latin1_text[2] = {"\260C", "\260F"};
  const char *deg_default_text[2] = {" C", " F"};
  size_t nconv;
  size_t degstr_size = sizeof(degstr);
  char *degstr_ptr = degstr;

  iconv_t cd = iconv_open(nl_langinfo(CODESET), "ISO-8859-1");
  if (cd != (iconv_t) -1) {
    nconv = iconv(cd, &(deg_latin1_text[fahrenheit]), &deg_latin1_size,
                  &degstr_ptr, &degstr_size);
    iconv_close(cd);
    
    if (nconv != (size_t) -1)
      return;	   
  }

  /* There was an error during the conversion, use the default text */
  strcpy(degstr, deg_default_text[fahrenheit]);
}

int main (int argc, char *argv[])
{
  int c,res,i,error;
  char *config_file_name = NULL;

  struct option long_opts[] =  {
    { "help", no_argument, NULL, 'h' },
    { "set", no_argument, NULL, 's' },
    { "version", no_argument, NULL, 'v'},
    { "fahrenheit", no_argument, NULL, 'f' },
    { "algorithm", no_argument, NULL, 'a' },
    { "no-adapter", no_argument, NULL, 'A' },
    { "no-unknown", no_argument, NULL, 'U' },
    { "config-file", required_argument, NULL, 'c' },
    { "unknown", no_argument, NULL, 'u' },
    { 0,0,0,0 }
  };

  setlocale(LC_CTYPE, "");

  do_unknown = 0;
  do_sets = 0;
  show_algorithm = 0;
  hide_adapter = 0;
  hide_unknown = 0;
  while (1) {
    c = getopt_long(argc,argv,"hsvfaAUc:u",long_opts,NULL);
    if (c == EOF)
      break;
    switch(c) {
    case ':':
    case '?':
      print_short_help();
      exit(1);
    case 'h':
      print_long_help();
      exit(0);
    case 'v':
      print_version();
      exit(0);
    case 'c':
      config_file_name = strdup(optarg);
      break;
    case 's':
      do_sets = 1;
      break;
    case 'f':
      fahrenheit = 1;
      break;
    case 'a':
      show_algorithm = 1;
      break;
    case 'A':
      hide_adapter = 1;
      break;
    case 'U':
      hide_unknown = 1;
      break;
    case 'u':
      do_unknown = 1;
      break;
    default:
      fprintf(stderr,"Internal error while parsing options!\n");
      exit(1);
    }
  }

  if (optind == argc) {
    chips[0].prefix = SENSORS_CHIP_NAME_PREFIX_ANY;
    chips[0].bus = SENSORS_CHIP_NAME_BUS_ANY;
    chips[0].addr = SENSORS_CHIP_NAME_ADDR_ANY;
    chips_count = 1;
  } else 
    for(i = optind; i < argc; i++) 
      if ((res = sensors_parse_chip_name(argv[i],chips+chips_count))) {
        fprintf(stderr,"Parse error in chip name `%s'\n",argv[i]);
        print_short_help();
        exit(1);
      } else if (++chips_count == CHIPS_MAX) {
        fprintf(stderr,"Too many chips on command line!\n");
        exit(1);
      }


  if (config_file_name == NULL)
    config_file_name = strdup(ETCDIR "/" DEFAULT_CONFIG_FILE_NAME);
  open_config_file(config_file_name);

  if ((res = sensors_init(config_file))) {
    fprintf(stderr,"%s\n",sensors_strerror(res));
    if (res == -SENSORS_ERR_PROC)
      fprintf(stderr,
              "Unable to find i2c bus information;\n"
              "For 2.6 kernels, make sure you have mounted sysfs and done\n"
              "'modprobe i2c_sensor'!\n"
              "For older kernels, make sure you have done 'modprobe i2c-proc'!\n");
    exit(1);
  }

  close_config_file(config_file_name);
  free(config_file_name);

  /* build the degrees string */
  set_degstr();

  if(do_the_real_work(&error)) {
    sensors_cleanup();
    exit(error);
  } else {
    if(chips[0].prefix == SENSORS_CHIP_NAME_PREFIX_ANY)
	    fprintf(stderr,"No sensors found!\n");
    else
	    fprintf(stderr,"Specified sensor(s) not found!\n");
    sensors_cleanup();
    exit(1);
  }
}

/* returns number of chips found */
int do_the_real_work(int *error)
{
  const sensors_chip_name *chip;
  int chip_nr,i;
  int cnt = 0;

  *error = 0;
  for (chip_nr = 0; (chip = sensors_get_detected_chips(&chip_nr));)
    for(i = 0; i < chips_count; i++)
      if (sensors_match_chip(*chip,chips[i])) {
        if(do_sets) {
          if (do_a_set(*chip))
            *error = 1;
        } else
          do_a_print(*chip);
        i = chips_count;
	cnt++;
      }
   return(cnt);
}

/* returns 1 on error */
int do_a_set(sensors_chip_name name)
{
  int res;

  if ((res = sensors_do_chip_sets(name))) {
    if (res == -SENSORS_ERR_PROC) {
      fprintf(stderr,"%s: %s for writing;\n",sprintf_chip_name(name),
              sensors_strerror(res));
      fprintf(stderr,"Run as root?\n");
      return 1;
    } else if (res == -SENSORS_ERR_ACCESS_W) {
      fprintf(stderr, "%s: At least one \"set\" statement failed\n",
              sprintf_chip_name(name));
    } else {
      fprintf(stderr,"%s: %s\n",sprintf_chip_name(name),
              sensors_strerror(res));
    }
  }
  return 0;
}

const char *sprintf_chip_name(sensors_chip_name name)
{
  #define BUF_SIZE 200
  static char buf[BUF_SIZE];

  if (name.bus == SENSORS_CHIP_NAME_BUS_ISA)
    snprintf(buf,BUF_SIZE,"%s-isa-%04x",name.prefix,name.addr);
  else if (name.bus == SENSORS_CHIP_NAME_BUS_DUMMY)
    snprintf(buf,BUF_SIZE,"%s-%s-%04x",name.prefix,name.busname,name.addr);
  else
    snprintf(buf,BUF_SIZE,"%s-i2c-%d-%02x",name.prefix,name.bus,name.addr);
  return buf;
}

struct match {
	const char * prefix;
	void (*fn) (const sensors_chip_name *name);
};

struct match matches[] = {
	{ "ds1621", print_ds1621 },
	{ "lm75", print_lm75 },
	{ "adm1021", print_adm1021 },
	{ "max1617", print_adm1021 },
	{ "max1617a", print_adm1021 },
	{ "thmc10", print_adm1021 },
	{ "lm84", print_adm1021 },
	{ "gl523", print_adm1021 },
	{ "adm1023", print_adm1021 },
	{ "mc1066", print_adm1021 },
	{ "adm9240", print_adm9240 },
	{ "ds1780", print_adm9240 },
	{ "lm81", print_adm9240 },
	{ "lm78", print_lm78 },
	{ "lm78-j", print_lm78 },
	{ "lm79", print_lm78 },
	{ "mtp008", print_mtp008 },
	{ "sis5595", print_sis5595 },
	{ "via686a", print_via686a },
	{ "lm80", print_lm80 },
	{ "lm85", print_lm85 },
	{ "lm85b", print_lm85 },
	{ "lm85c", print_lm85 },
	{ "adm1027", print_lm85 },
	{ "adt7463", print_lm85 },
	{ "emc6d100", print_lm85 },
	{ "emc6d102", print_lm85 },
	{ "lm87", print_lm87 },
	{ "gl518sm", print_gl518 },
	{ "gl520sm", print_gl520 },
	{ "adm1025", print_adm1025 },
	{ "ne1619", print_adm1025 },
	{ "adm1024", print_adm1024 },
	{ "w83781d", print_w83781d },
	{ "w83782d", print_w83781d },
	{ "w83783d", print_w83781d },
	{ "w83627hf", print_w83781d },
	{ "w83627thf", print_w83781d },
	{ "w83637hf", print_w83781d },
	{ "w83697hf", print_w83781d },
	{ "w83627ehf", print_w83627ehf },
	{ "w83791d", print_w83781d },
        { "w83792d", print_w83792d },
	{ "w83l785ts", print_w83l785ts },
	{ "as99127f", print_w83781d },
	{ "maxilife", print_maxilife },
	{ "maxilife-cg", print_maxilife },
	{ "maxilife-co", print_maxilife },
	{ "maxilife-as", print_maxilife },
	{ "maxilife-nba", print_maxilife },
	{ "it87", print_it87 },
	{ "it8712", print_it87 },
	{ "ddcmon", print_ddcmon },
	{ "eeprom", print_eeprom },
	{ "fscpos", print_fscpos },
	{ "fscscy", print_fscscy },
	{ "fscher", print_fscher },
	{ "pcf8591", print_pcf8591 },
	{ "vt1211", print_vt1211 },
	{ "smsc47m1", print_smsc47m1 },
	{ "pc87360", print_pc87360 },
	{ "pc87363", print_pc87360 },
	{ "pc87364", print_pc87364 },
	{ "pc87365", print_pc87366 },
	{ "pc87366", print_pc87366 },
	{ "lm92", print_lm92 },
	{ "vt8231", print_vt8231 },
	{ "bmc", print_bmc },
	{ "adm1026", print_adm1026 },
	{ "lm83", print_lm83 },
	{ "lm90", print_lm90 },
	{ "adm1032", print_lm90 },
	{ "lm99", print_lm90 },
	{ "lm86", print_lm90 },
	{ "max6657", print_lm90 },
	{ "adt7461", print_lm90 },
	{ "lm63", print_lm63 },
	{ "xeontemp", print_xeontemp },
	{ "max6650", print_max6650 },
	{ "asb100", print_asb100 },
	{ "adm1030", print_adm1031 },
	{ "adm1031", print_adm1031 },
	{ "lm93", print_lm93 },
	{ "smsc47b397", print_smsc47b397 },
	{ NULL, NULL }
};

void do_a_print(sensors_chip_name name)
{
  const char *algo,*adap;
  struct match *m;

  /* do we know how to display it? */
  for(m = matches; m->prefix != NULL; m++) {
    if(!strcmp(name.prefix, m->prefix)) break;
  }

  if(m->prefix==NULL && hide_unknown)
    return;

  printf("%s\n",sprintf_chip_name(name));
  adap = sensors_get_adapter_name(name.bus);
  if (adap && !hide_adapter)
    printf("Adapter: %s\n",adap);
  algo = sensors_get_algorithm_name(name.bus);
  if (algo && show_algorithm)
    printf("Algorithm: %s\n",algo);
  if (!algo || !adap)
    printf(" ERROR: Can't get adapter or algorithm?!?\n");
  if (do_unknown)
    print_unknown_chip(&name);
  else {
    if(m->prefix == NULL)
	print_unknown_chip(&name);
    else
	m->fn(&name);
  }
  printf("\n");
}
