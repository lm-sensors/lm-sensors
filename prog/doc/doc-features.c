/*
    doc-features.c - A program to dump sensor feature documentation
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

/* This program uses undocumented sensors library access. It it undocumented
   for a reason: normal programs really should not work on this level! */

#include "lib/data.h"
#include <stdlib.h>
#include <getopt.h>

const char *lookup_feature_name(int featurenr,
                  const sensors_chip_features *chipdata)
{
  int i;
  if (featurenr == SENSORS_NO_MAPPING)
    return "";
  for (i = 0; chipdata->feature[i].name ; i++) 
    if (chipdata->feature[i].number == featurenr)
      return chipdata->feature[i].name;
  return "***ERROR***";
}

const char *mode_string(int mode)
{
  if (mode == SENSORS_MODE_RW)
    return "RW";
  else if (mode == SENSORS_MODE_R)
    return "R ";
  else if (mode == SENSORS_MODE_W)
    return " W";
  else
   return "--";
}

void dump_feature(const sensors_chip_feature *featuredata,
                  const sensors_chip_features *chipdata)
{
  printf("  %17s: %17s %17s   %2s\n",featuredata->name,
         lookup_feature_name(featuredata->logical_mapping,chipdata),
         lookup_feature_name(featuredata->compute_mapping,chipdata),
         mode_string(featuredata->mode));
}

int qsort_compare (const void *f1, const void *f2)
{
  return strcmp((* ((sensors_chip_feature * const *)f1))->name,
                (* ((sensors_chip_feature * const *)f2))->name);
}

void dump_chip (const sensors_chip_features *chipdata)
{
  int i;
  sensors_chip_feature **features;
  for (i = 0; chipdata->feature[i].name; i++);

  features = malloc(sizeof(*features) * i+1);
  for (i = 0; chipdata->feature[i].name; i++)
    features[i] = chipdata->feature + i;
  features[i] = chipdata->feature + i;
  qsort(features,i,sizeof(*features),qsort_compare);

  printf("Chip `%s'\n",chipdata->prefix);
  printf("  %17s  %17s %17s   %2s\n","NAME","LABEL CLASS","COMPUTE CLASS",
         "RW");
  for (i = 0; features[i]->name; i++)
    dump_feature(features[i],chipdata);

  free(features);
  printf("\n");
}

void print_short_help(void)
{
  fprintf(stderr,"Usage: doc-features [-h] [-v] [chipname]..\n");
  fprintf(stderr,"Try `doc-features -h' for more information\n");
}

void print_long_help(void)
{
  fprintf(stderr,"Usage: doc-features [-h] [-v] [chipname]..\n");
  fprintf(stderr,"  -h, --help         Display this help text\n");
  fprintf(stderr,"  -v, --version      Display version information\n");
  fprintf(stderr,"If no chipnames are specified, information for all chips is dumped.\n");
}
  
void print_version(void)
{
  printf("doc-features version 1.0\n");
}

int main (int argc, char *argv[])
{
  int c,i;

  struct option long_opts[] = {
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v'}
  };

  while(1) {
    c = getopt_long(argc,argv,"hv",long_opts,NULL);
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
    default:
      fprintf(stderr,"Internal error while parsing options!\n");
      exit(1);
    }
  }

  if (optind == argc) 
    for (i = 0; sensors_chip_features_list[i].prefix ; i++) 
      dump_chip(sensors_chip_features_list+i);
  else
    for (; optind != argc ; optind ++)
      for (i = 0; sensors_chip_features_list[i].prefix; i++)   
        if (!strcmp(sensors_chip_features_list[i].prefix,argv[optind]))
          dump_chip(sensors_chip_features_list+i);
        
  return 0;
}
  
