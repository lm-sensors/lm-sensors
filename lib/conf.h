/*
    conf.h - Part of libsensors, a Linux library for reading sensor data.
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

#ifndef LIB_SENSORS_CONF_H
#define LIB_SENSORS_CONF_H

/* This is defined in conf-lex.l */
extern int sensors_yylex(void);
extern char sensors_lex_error[];
extern int sensors_yylineno;
extern FILE *sensors_yyin;

/* This is defined in conf-parse.y */
extern int sensors_yyparse(void);

#endif /* LIB_SENSORS_CONF_H */
