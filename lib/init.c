/*
    init.c - Part of libsensors, a Linux library for reading sensor data.
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

#include <stdlib.h>
#include <stdio.h>
#include "sensors.h"
#include "data.h"
#include "proc.h"
#include "error.h"
#include "access.h"
#include "conf.h"

static void free_proc_chips_entry(sensors_proc_chips_entry entry);
static void free_chip_name(sensors_chip_name name);
static void free_bus(sensors_bus bus);
static void free_chip(sensors_chip chip);
static void free_label(sensors_label label);
static void free_set(sensors_set set);
static void free_compute(sensors_compute compute);
static void free_ignore(sensors_ignore ignore);
static void free_expr(sensors_expr *expr);

int sensors_init(FILE *input)
{
  int res;
  sensors_cleanup();
  if ((res = sensors_read_proc_chips()))
    return res;
  if ((res = sensors_read_proc_bus()))
    return res;
  sensors_yyin = input;
  if ((res = sensors_yyparse()))
    return -SENSORS_ERR_PARSE;
  if ((res = sensors_substitute_busses()));
    return res;
  return 0;
}

void sensors_cleanup(void)
{
  int i;

  for (i = 0; i < sensors_proc_chips_count; i++)
    free_proc_chips_entry(sensors_proc_chips[i]);
  free(sensors_proc_chips);
  sensors_proc_chips = NULL;
  sensors_proc_chips_count = sensors_proc_chips_max = 0;
  
  for (i = 0; i < sensors_config_busses_count; i++)
    free_bus(sensors_config_busses[i]);
  free(sensors_config_busses);
  sensors_config_busses = NULL;
  sensors_config_busses_count = sensors_config_busses_max = 0;

  for (i = 0; i < sensors_config_chips_count; i++)
    free_chip(sensors_config_chips[i]);
  free(sensors_config_chips);
  sensors_config_chips = NULL;
  sensors_config_chips_count = sensors_config_chips_max = 0;

  for (i = 0; i < sensors_proc_bus_count; i++)
    free_bus(sensors_proc_bus[i]);
  free(sensors_proc_bus);
  sensors_proc_bus = NULL;
  sensors_proc_bus_count = sensors_proc_bus_max = 0;
}

void free_proc_chips_entry(sensors_proc_chips_entry entry)
{
    free_chip_name(entry.name);
}

void free_chip_name(sensors_chip_name name)
{
  free(name.prefix);
  free(name.busname);
}

void free_bus(sensors_bus bus)
{
  free(bus.adapter);
  free(bus.algorithm);
}

void free_chip(sensors_chip chip)
{
  int i;

  for (i = 0; i < chip.labels_count; i++)
    free_label(chip.labels[i]);
  free(chip.labels);
  chip.labels_count = chip.labels_max = 0;

  for (i = 0; i < chip.sets_count; i++)
    free_set(chip.sets[i]);
  free(chip.sets);
  chip.sets_count = chip.sets_max = 0;

  for (i = 0; i < chip.computes_count; i++)
    free_compute(chip.computes[i]);
  free(chip.computes);
  chip.computes_count = chip.computes_max = 0;

  for (i = 0; i < chip.ignores_count; i++)
    free_ignore(chip.ignores[i]);
  free(chip.ignores);
  chip.ignores_count = chip.ignores_max = 0;
}

void free_label(sensors_label label)
{
  free(label.name);
  free(label.value);
}

void free_set(sensors_set set)
{
  free(set.name);
  free_expr(set.value);
}

void free_compute(sensors_compute compute)
{
  free(compute.name);
  free_expr(compute.from_proc);
  free_expr(compute.to_proc);
}

void free_ignore(sensors_ignore ignore)
{
  free(ignore.name);
}

void free_expr(sensors_expr *expr)
{
  if ((expr->kind) == sensors_kind_var)
    free(expr->data.var);
  else if ((expr->kind) == sensors_kind_sub) {
    if (expr->data.subexpr.sub1)
      free_expr(expr->data.subexpr.sub1);
    if (expr->data.subexpr.sub2)
      free_expr(expr->data.subexpr.sub2);
  }
  free(expr);
}
