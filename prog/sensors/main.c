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

#include "lib/sensors.h" 
#include "lib/error.h"
#include "chips.h"
#include "version.h"

#define PROGRAM "sensors"
#define VERSION LM_VERSION
#define DEFAULT_CONFIG_FILE_NAME "sensors.conf"

static char *config_file_name;
FILE *config_file;
static const char *config_file_path[] = 
  { "/etc", "/usr/lib/sensors", "/usr/local/lib/sensors", "/usr/lib",
    "/usr/local/lib", ".", 0 };

extern int main(int argc, char *arv[]);
static void print_short_help(void);
static void print_long_help(void);
static void print_version(void);
static void open_config_file(void);
static int open_this_config_file(char *filename);
static void do_a_print(sensors_chip_name name);
static void do_a_set(sensors_chip_name name);
static int do_the_real_work(void);
static const char *sprintf_chip_name(sensors_chip_name name);

#define CHIPS_MAX 20
sensors_chip_name chips[CHIPS_MAX];
int chips_count=0;
int do_sets, do_unknown, fahrenheit;

void print_short_help(void)
{
  printf("Try `%s -h' for more information\n",PROGRAM);
}

void print_long_help(void)
{
  printf("Usage: %s [OPTION]... [CHIP]...\n",PROGRAM);
  printf("  -c, --config-file     Specify a config file\n");
  printf("  -h, --help            Display this help text\n");
  printf("  -s, --set             Execute `set' statements too (root only)\n");
  printf("  -f, --fahrenheit      Show temperatures in degrees fahrenheit\n" );
  printf("  -u, --unknown         Treat chips as unknown ones (testing only)\n");
  printf("  -v, --version         Display the program version\n");
  printf("\n");
  printf("By default, a list of directories is examined for config file `sensors.conf'.\n");
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
  printf("%s version %s\n", PROGRAM, VERSION);
}

/* This examines global var config_file, and leaves the name there too. 
   It also opens config_file. */
void open_config_file(void)
{
#define MAX_FILENAME_LEN 1024
  char *filename;
  char buffer[MAX_FILENAME_LEN];
  int res,i;

  if (config_file_name && !strcmp(config_file_name,"-")) {
    config_file = stdin;
    return;
  } else if (config_file_name && index(config_file_name,'/')) {
    if ((res = open_this_config_file(config_file_name))) {
      fprintf(stderr,"Could not locate or open config file\n");
      fprintf(stderr,"%s: %s\n",config_file_name,strerror(res));
      exit(1);
    }
  }
  else {
    if (config_file_name)
      filename = config_file_name;
    else
      filename = strdup(DEFAULT_CONFIG_FILE_NAME);
    for (i = 0; config_file_path[i]; i++) {
      if ((snprintf(buffer,MAX_FILENAME_LEN,
                   "%s/%s",config_file_path[i],filename)) < 1) {
        fprintf(stderr,
                "open_config_file: ridiculous long config file name!\n");
        exit(1);
      }
      if (!open_this_config_file(buffer)) {
        free(config_file_name);
        config_file_name = strdup(buffer);
        return;
      }
    }
    fprintf(stderr,"Could not locate or open config file!\n");
    exit(1);
  }
}
    
int open_this_config_file(char *filename)
{
  config_file = fopen(filename,"r");
  if (! config_file)
    return -errno;
  return 0;
}
  

int main (int argc, char *argv[])
{
  int c,res,i;

  struct option long_opts[] =  {
    { "help", no_argument, NULL, 'h' },
    { "set", no_argument, NULL, 's' },
    { "version", no_argument, NULL, 'v'},
    { "fahrenheit", no_argument, NULL, 'f' },
    { "config-file", required_argument, NULL, 'c' },
    { "unknown", required_argument, NULL, 'u' },
    { 0,0,0,0 }
  };

  do_unknown = 0;
  do_sets = 0;
  while (1) {
    c = getopt_long(argc,argv,"hvufsc:",long_opts,NULL);
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


  open_config_file();

  if ((res = sensors_init(config_file))) {
    fprintf(stderr,"%s\n",sensors_strerror(res));
    if (res == -SENSORS_ERR_PROC)
      fprintf(stderr,
              "/proc/sys/dev/sensors/chips or /proc/bus/i2c unreadable;\n"
              "Make sure you have done 'modprobe i2c-proc'!\n");
    exit(1);
  }

  if(do_the_real_work()) {
    exit(0);
  } else {
    if(chips[0].prefix == SENSORS_CHIP_NAME_PREFIX_ANY)
	    fprintf(stderr,"No sensors found!\n");
    else
	    fprintf(stderr,"Specified sensor(s) not found!\n");
    exit(1);
  }
}

/* returns number of chips found */
int do_the_real_work(void)
{
  const sensors_chip_name *chip;
  int chip_nr,i;
  int cnt = 0;

  for (chip_nr = 0; (chip = sensors_get_detected_chips(&chip_nr));)
    for(i = 0; i < chips_count; i++)
      if (sensors_match_chip(*chip,chips[i])) {
        if(do_sets)
          do_a_set(*chip);
        else
          do_a_print(*chip);
        i = chips_count;
	cnt++;
      }
   return(cnt);
}

void do_a_set(sensors_chip_name name)
{
  int res;
  if ((res = sensors_do_chip_sets(name))) {
    if (res == -SENSORS_ERR_PROC) {
      fprintf(stderr,"%s: %s for writing;\n",sprintf_chip_name(name),
              sensors_strerror(res));
      fprintf(stderr,"Run as root?\n");
    } else {
      fprintf(stderr,"%s: %s\n",sprintf_chip_name(name),
              sensors_strerror(res));
    }
  }
}

const char *sprintf_chip_name(sensors_chip_name name)
{
  #define BUF_SIZE 200
  static char buf[BUF_SIZE];

  if (name.bus == SENSORS_CHIP_NAME_BUS_ISA)
    snprintf(buf,BUF_SIZE,"%s-isa-%04x",name.prefix,name.addr);
  else
    snprintf(buf,BUF_SIZE,"%s-i2c-%d-%02x",name.prefix,name.bus,name.addr);
  return buf;
}

void do_a_print(sensors_chip_name name)
{
  const char *algo,*adap;

  printf("%s\n",sprintf_chip_name(name));
  adap = sensors_get_adapter_name(name.bus);
  if (adap)
    printf("Adapter: %s\n",adap);
  algo = sensors_get_algorithm_name(name.bus);
  if (algo)
    printf("Algorithm: %s\n",algo);
  if (!algo || !adap)
    printf(" ERROR: Can't get adapter or algorithm?!?\n");
  if (do_unknown)
    print_unknown_chip(&name);
  else if (!strcmp(name.prefix,"ds1621"))
    print_ds1621(&name);
  else if (!strcmp(name.prefix,"lm75"))
    print_lm75(&name);
  else if (!strcmp(name.prefix,"adm1021") || !strcmp(name.prefix,"max1617") ||
           !strcmp(name.prefix,"max1617a") || !strcmp(name.prefix, "thmc10") ||
           !strcmp(name.prefix,"lm84") || !strcmp(name.prefix, "gl523") ||
	   !strcmp(name.prefix, "adm1023"))
    print_adm1021(&name);
  else if (!strcmp(name.prefix,"adm9240") ||
           !strcmp(name.prefix,"ds1780") ||
           !strcmp(name.prefix,"lm81"))
    print_adm9240(&name);
  else if (!strcmp(name.prefix,"lm78") || !strcmp(name.prefix,"lm78-j") ||
           !strcmp(name.prefix,"lm79"))
    print_lm78(&name);
  else if (!strcmp(name.prefix,"mtp008"))
    print_mtp008(&name);
  else if (!strcmp(name.prefix,"sis5595"))
    print_sis5595(&name);
  else if (!strcmp(name.prefix,"via686a"))
    print_via686a(&name);
  else if (!strcmp(name.prefix,"lm80"))
    print_lm80(&name);
  else if (!strcmp(name.prefix,"lm87"))
    print_lm87(&name);
  else if (!strcmp(name.prefix,"gl518sm"))
    print_gl518(&name);
  else if (!strcmp(name.prefix,"adm1025"))
    print_adm1025(&name);
  else if (!strcmp(name.prefix,"adm1024"))
    print_adm1024(&name);
  else if ((!strcmp(name.prefix,"w83781d")) ||
           (!strcmp(name.prefix,"w83782d")) ||
           (!strcmp(name.prefix,"w83783s")) ||
           (!strcmp(name.prefix,"w83627hf")) ||
           (!strcmp(name.prefix,"w83697hf")) ||
           (!strcmp(name.prefix,"as99127f")))
    print_w83781d(&name);
  else if (!strncmp(name.prefix,"maxilife-", 9))
    print_maxilife(&name);
  else if (!strncmp(name.prefix,"it87", 4))
    print_it87(&name);
  else if (!strcmp(name.prefix,"ddcmon"))
    print_ddcmon(&name);
  else if (!strcmp(name.prefix,"eeprom"))
    print_eeprom(&name);
  else if (!strcmp(name.prefix,"fscpos"))
    print_fscpos(&name);
  else if (!strcmp(name.prefix,"fscscy"))
    print_fscscy(&name);
  else if (!strcmp(name.prefix,"pcf8591"))
    print_pcf8591(&name);
  else
    print_unknown_chip(&name);
  printf("\n");
}
