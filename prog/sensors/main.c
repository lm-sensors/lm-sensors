/*
    main.c - Part of sensors, a user-space program for hardware monitoring
    Copyright (c) 1998  Frodo Looijaard <frodol@dds.nl>

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

#define PROGRAM "sensors"
#define VERSION "0.0"
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

void print_short_help(void)
{
  printf("Try `%s -h' for more information\n",PROGRAM);
}

void print_long_help(void)
{
  printf("Usage: %s [OPTION]...\n",PROGRAM);
  printf("  -c, --config-file     Specify a config file\n");
  printf("  -h, --help            Display this help text\n");
  printf("  -v, --version         Display the program version\n");
  printf("\n");
  printf("By default, a list of directories is examined for the config file `sensors.conf'\n");
  printf("Use `-' after `-c' to read the config file from stdin\n");
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
  int c,res;

  struct option long_opts[] =  {
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v'},
    { "config-file", required_argument, NULL, 'c' }
  };

  while (1) {
    c = getopt_long(argc,argv,"hvc:",long_opts,NULL);
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
    default:
      fprintf(stderr,"Internal error while parsing options!\n");
    }
  }
  open_config_file();

  if ((res = sensors_init(config_file))) {
    if (res == SENSORS_ERR_PROC)
      fprintf(stderr,
              "/proc/sys/dev/sensors/chips or /proc/bus/i2c unreadable:\n"
              "Make sure you have inserted modules sensors.o and i2c-proc.o!");
    else
      fprintf(stderr,"%s\n",sensors_strerror(res));
    exit(1);
  }
}
