%{
/*
    conf-lex.l - Part of libsensors, a Linux library for reading sensor data.
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
#include <string.h>

#include "general.h"
#include "data.h"
#include "conf-parse.h"
#include "error.h"

static int buffer_count;
static int buffer_max;
static char *buffer;

char sensors_lex_error[100];

#define buffer_malloc() sensors_malloc_array(&buffer,&buffer_count,\
                                             &buffer_max,1)
#define buffer_free() sensors_free_array(&buffer,&buffer_count,\
                                         &buffer_max)
#define buffer_add_char(c) sensors_add_array_el(c,&buffer,\
                                                &buffer_count,\
                                                &buffer_max,1)
#define buffer_add_string(s) sensors_add_array_els(s,strlen(s),\
                                                   &buffer, \
                                                   &buffer_count,&buffer_max,1)

%}

/* Scanner for configuration files */

%option noyywrap
%option nodefault
%option yylineno
%option nounput

/* States. 'Normal' states STRING and MIDDLE share some rules; other states
   have only their own rules */
%s MIDDLE
%x STRING
%x ERR

/* Any whitespace-like character */
BLANK		[[:space:]]

IDCHAR		[[:alnum:]_]

/* Note: `10', `10.4' and `.4' are valid, `10.' is not */
FLOAT   [[:digit:]]*\.?[[:digit:]]+

/* Only positive whole numbers are recognized here */
NUM	0|([1-9][[:digit:]]*)

/* Only number between 1 and 255, octally represented. */
OCTESC		(1[0-7]{0,2})|([2-7][0-7]?)|(0[1-7][0-7]?)|(00[1-7])


%%


 /* End of line: It may be the end of this line. Same for End of file. */
<MIDDLE>\n	|
<MIDDLE><<EOF>> {
		  BEGIN(INITIAL);
		  return EOL;
		}

 /* We want to match any blank, except End of line; that is why we have to
    match whitespace one by one! */
{BLANK}		/* Eat up a blank */

 /* Escaped End of line: eat and be happy */
<MIDDLE>\\\n	/* Eat this! */

 /* Remove a comment; we do not change the state, this is done when the \n is
    eaten */
#[^\n]*		/* Eat this! */

 /* Some keywords at the beginning of lines */
<INITIAL>"label" {
		  sensors_yylval.line = sensors_yylineno;
		  BEGIN(MIDDLE);
		  return LABEL;
		}

<INITIAL>"set"	{
		  sensors_yylval.line = sensors_yylineno;
		  BEGIN(MIDDLE);
		  return SET;
		}

<INITIAL>"compute" {
		  sensors_yylval.line = sensors_yylineno;
		  BEGIN(MIDDLE);
		  return COMPUTE;
		}

<INITIAL>"bus"	{
		  sensors_yylval.line = sensors_yylineno;
		  BEGIN(MIDDLE);
		  return BUS;
		}

<INITIAL>"chip"	{
		  sensors_yylval.line = sensors_yylineno;
		  BEGIN(MIDDLE);
		  return CHIP;
		}
<INITIAL>"ignore" {
		  sensors_yylval.line = sensors_yylineno;
		  BEGIN(MIDDLE);
		  return IGNORE;
		}

 /* Anything else at the beginning of a line is an error */
<INITIAL>.	{
		  yymore();
		  BEGIN(ERR);
		}

<ERR>[^\n]*\n	{
		  BEGIN(INITIAL);
		  strcpy(sensors_lex_error,"Invalid keyword");
		  return ERROR;
		}

 /* A number */
<MIDDLE>{FLOAT}	{
		  sensors_yylval.value = atof(sensors_yytext);
		  return FLOAT;
		}

 /* Some operators */
<MIDDLE>"+"	{
		  return '+';
		}

<MIDDLE>"-"	{
		  return '-';
		}

<MIDDLE>"*"	{
		  return '*';
		}

<MIDDLE>"/"	{
		  return '/';
		}

<MIDDLE>"("	{
		  return '(';
		}

<MIDDLE>")"	{
		  return ')';
		}
<MIDDLE>","	{
		  return ',';
		}
<MIDDLE>"@"	{
		  return '@';
		}
<MIDDLE>"^"	{
		  return '^';
		}
<MIDDLE>"`"	{
		  return '`';
		}

 /* Quoted string */
<MIDDLE>\"	{
		  buffer_malloc();
		  BEGIN(STRING);
		}

 /* Oops, newline while in a string is not good */
<STRING>\n	|
<STRING>\\\n	{
		  buffer_add_char("\0");
		  strcpy(sensors_lex_error,"No matching double quote");
		  buffer_free();
		  BEGIN(INITIAL);
		  return ERROR;
		}

 /* At the end */
<STRING>\"	{
		  buffer_add_char("\0");
		  sensors_yylval.name = strdup(buffer);
		  if (! sensors_yylval.name)
		    sensors_fatal_error("conf-lex.l",
                                        "Allocating a new string");
		  buffer_free();
		  BEGIN(MIDDLE);
		  return NAME;
		}

<STRING>\\a	{
		  buffer_add_char("\a");
		}

<STRING>\\b	{
		  buffer_add_char("\b");
		}

<STRING>\\f	{
		  buffer_add_char("\f");
		}

<STRING>\\n	{
		  buffer_add_char("\n");
		}

<STRING>\\r	{
		  buffer_add_char("\r");
		}

<STRING>\\t	{
		  buffer_add_char("\t");
		}

<STRING>\\v	{
		  buffer_add_char("\v");
		}

 /* We can't support \0, this would cause havoc! */
<STRING>\\{OCTESC} {
		  int res;
		  sscanf(sensors_yytext+1,"%o",&res);
		  buffer_add_char(&res);
		}

 /* Other escapes: just copy the character behind the slash */
<STRING>\\.	{
		  buffer_add_char(&sensors_yytext[1]);
		}

 /* Anything else */
<STRING>[^\\\n\"]+ {
		  buffer_add_string(sensors_yytext);
		}

 /* A normal, unquoted identifier */
<MIDDLE>{IDCHAR}+ {
		  sensors_yylval.name = strdup(sensors_yytext);
		  if (! sensors_yylval.name)
		    sensors_fatal_error("conf-lex.l",
                                        "Allocating a new string");
		  
		  return NAME;
		}

%%
