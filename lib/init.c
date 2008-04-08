/*
    init.c - Part of libsensors, a Linux library for reading sensor data.
    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
    Copyright (C) 2007        Jean Delvare <khali@linux-fr.org>

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sensors.h"
#include "data.h"
#include "error.h"
#include "access.h"
#include "conf.h"
#include "sysfs.h"
#include "scanner.h"
#include "init.h"

#define DEFAULT_CONFIG_FILE	ETCDIR "/sensors3.conf"
#define ALT_CONFIG_FILE		ETCDIR "/sensors.conf"

/* Wrapper around sensors_yyparse(), which clears the locale so that
   the decimal numbers are always parsed properly. */
static int sensors_parse(void)
{
	int res;
	char *locale;

	/* Remember the current locale and clear it */
	locale = setlocale(LC_ALL, NULL);
	if (locale) {
		locale = strdup(locale);
		setlocale(LC_ALL, "C");
	}

	res = sensors_yyparse();

	/* Restore the old locale */
	if (locale) {
		setlocale(LC_ALL, locale);
		free(locale);
	}

	return res;
}

int sensors_init(FILE *input)
{
	int res;

	if (!sensors_init_sysfs())
		return -SENSORS_ERR_KERNEL;
	if ((res = sensors_read_sysfs_bus()) ||
	    (res = sensors_read_sysfs_chips()))
		goto exit_cleanup;

	res = -SENSORS_ERR_PARSE;
	if (input) {
		if (sensors_scanner_init(input) ||
		    sensors_parse())
			goto exit_cleanup;
	} else {
		/* No configuration provided, use default */
		input = fopen(DEFAULT_CONFIG_FILE, "r");
		if (!input && errno == ENOENT)
			input = fopen(ALT_CONFIG_FILE, "r");
		if (input) {
			if (sensors_scanner_init(input) ||
			    sensors_parse()) {
				fclose(input);
				goto exit_cleanup;
			}
			fclose(input);
		}
	}

	if ((res = sensors_substitute_busses()))
		goto exit_cleanup;
	return 0;

exit_cleanup:
	sensors_cleanup();
	return res;
}

static void free_chip_name(sensors_chip_name *name)
{
	free(name->prefix);
	free(name->path);
}

static void free_chip_features(sensors_chip_features *features)
{
	int i;

	for (i = 0; i < features->subfeature_count; i++)
		free(features->subfeature[i].name);
	free(features->subfeature);
	for (i = 0; i < features->feature_count; i++)
		free(features->feature[i].name);
	free(features->feature);
}

static void free_bus(sensors_bus *bus)
{
	free(bus->adapter);
}

static void free_label(sensors_label *label)
{
	free(label->name);
	free(label->value);
}

void free_expr(sensors_expr *expr)
{
	if (expr->kind == sensors_kind_var)
		free(expr->data.var);
	else if (expr->kind == sensors_kind_sub) {
		if (expr->data.subexpr.sub1)
			free_expr(expr->data.subexpr.sub1);
		if (expr->data.subexpr.sub2)
			free_expr(expr->data.subexpr.sub2);
	}
	free(expr);
}

static void free_set(sensors_set *set)
{
	free(set->name);
	free_expr(set->value);
}

static void free_compute(sensors_compute *compute)
{
	free(compute->name);
	free_expr(compute->from_proc);
	free_expr(compute->to_proc);
}

static void free_ignore(sensors_ignore *ignore)
{
	free(ignore->name);
}

static void free_chip(sensors_chip *chip)
{
	int i;

	for (i = 0; i < chip->chips.fits_count; i++)
		free_chip_name(&chip->chips.fits[i]);
	free(chip->chips.fits);
	chip->chips.fits_count = chip->chips.fits_max = 0;

	for (i = 0; i < chip->labels_count; i++)
		free_label(&chip->labels[i]);
	free(chip->labels);
	chip->labels_count = chip->labels_max = 0;

	for (i = 0; i < chip->sets_count; i++)
		free_set(&chip->sets[i]);
	free(chip->sets);
	chip->sets_count = chip->sets_max = 0;

	for (i = 0; i < chip->computes_count; i++)
		free_compute(&chip->computes[i]);
	free(chip->computes);
	chip->computes_count = chip->computes_max = 0;

	for (i = 0; i < chip->ignores_count; i++)
		free_ignore(&chip->ignores[i]);
	free(chip->ignores);
	chip->ignores_count = chip->ignores_max = 0;
}

void sensors_cleanup(void)
{
	int i;

	sensors_scanner_exit();

	for (i = 0; i < sensors_proc_chips_count; i++) {
		free_chip_name(&sensors_proc_chips[i].chip);
		free_chip_features(&sensors_proc_chips[i]);
	}
	free(sensors_proc_chips);
	sensors_proc_chips = NULL;
	sensors_proc_chips_count = sensors_proc_chips_max = 0;

	for (i = 0; i < sensors_config_busses_count; i++)
		free_bus(&sensors_config_busses[i]);
	free(sensors_config_busses);
	sensors_config_busses = NULL;
	sensors_config_busses_count = sensors_config_busses_max = 0;

	for (i = 0; i < sensors_config_chips_count; i++)
		free_chip(&sensors_config_chips[i]);
	free(sensors_config_chips);
	sensors_config_chips = NULL;
	sensors_config_chips_count = sensors_config_chips_max = 0;

	for (i = 0; i < sensors_proc_bus_count; i++)
		free_bus(&sensors_proc_bus[i]);
	free(sensors_proc_bus);
	sensors_proc_bus = NULL;
	sensors_proc_bus_count = sensors_proc_bus_max = 0;
}
