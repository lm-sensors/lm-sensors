/*
 * sensord
 *
 * A daemon that logs all sensor information to /var/log/sensors
 * every 5 minutes.
 *
 * Copyright (c) 1999 Merlin Hughes <merlin@merlin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define MAX_DATA 5

typedef char * (*FormatterFN) (double[]);

typedef struct {
  FormatterFN format;
  int labelNumber;
  int dataNumbers[MAX_DATA + 1];
} FeatureDescriptor;

typedef struct {
  char **names;
  FeatureDescriptor *features;
} ChipDescriptor;

extern ChipDescriptor knownChips[];
