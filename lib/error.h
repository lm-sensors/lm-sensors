/*
    error.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef LIB_SENSORS_ERROR_H
#define LIB_SENSORS_ERROR_H

#define SENSORS_ERR_WILDCARDS 1 /* Wildcard found in chip name */
#define SENSORS_ERR_NO_ENTRY 2  /* No such feature known */
#define SENSORS_ERR_ACCESS 3    /* Can't read or write */
#define SENSORS_ERR_PROC 4      /* Can't access /proc file */
#define SENSORS_ERR_DIV_ZERO 5  /* Divide by zero */
#define SENSORS_ERR_CHIP_NAME 6 /* Can't parse chip name */
#define SENSORS_ERR_BUS_NAME 7  /* Can't parse bus name */
#define SENSORS_ERR_PARSE 8     /* General parse error */
#define SENSORS_ERR_ACCESS_W 9    /* Can't write */
#define SENSORS_ERR_ACCESS_R 10    /* Can't read */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* This function returns a pointer to a string which describes the error.
   errnum may be negative (the corresponding positive error is returned).
   You may not modify the result! */
extern const char *sensors_strerror(int errnum);

/* This function is called when a parse error is detected. Give it a new
   value, and your own function is called instead of the default (which
   prints to stderr). This function may terminate the program, but it
   usually outputs an error and returns. */
extern void (*sensors_parse_error) (const char *err, int lineno);

/* This function is called when an immediately fatal error (like no
   memory left) is detected. Give it a new value, and your own function
   is called instead of the default (which prints to stderr and ends
   the program). Never let it return! */
extern void (*sensors_fatal_error) (const char *proc, const char *err);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* def LIB_SENSORS_ERROR_H */
