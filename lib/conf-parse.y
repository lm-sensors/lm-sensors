%{
/*
    conf-parse.y - Part of libsensors, a Linux library for reading sensor data.
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

#define YYERROR_VERBOSE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "data.h"
#include "general.h"
#include "error.h"
#include "conf.h"
#include "access.h"

/* These two functions are defined in conf-lex.l */
extern int sensors_yylex(void);
extern char sensors_lex_error[];
extern int sensors_yylineno;

static void sensors_yyerror(const char *err);
static sensors_expr *malloc_expr(void);

static sensors_chip *current_chip = NULL;

#define bus_add_el(el) sensors_add_array_el(el,\
                                      &sensors_config_busses,\
                                      &sensors_config_busses_count,\
                                      &sensors_config_busses_max,\
                                      sizeof(sensors_bus))
#define label_add_el(el) sensors_add_array_el(el,\
                                        &current_chip->labels,\
                                        &current_chip->labels_count,\
                                        &current_chip->labels_max,\
                                        sizeof(sensors_label));
#define set_add_el(el) sensors_add_array_el(el,\
                                      &current_chip->sets,\
                                      &current_chip->sets_count,\
                                      &current_chip->sets_max,\
                                      sizeof(sensors_set));
#define compute_add_el(el) sensors_add_array_el(el,\
                                          &current_chip->computes,\
                                          &current_chip->computes_count,\
                                          &current_chip->computes_max,\
                                          sizeof(sensors_compute));
#define ignore_add_el(el) sensors_add_array_el(el,\
                                          &current_chip->ignores,\
                                          &current_chip->ignores_count,\
                                          &current_chip->ignores_max,\
                                          sizeof(sensors_ignore));
#define chip_add_el(el) sensors_add_array_el(el,\
                                       &sensors_config_chips,\
                                       &sensors_config_chips_count,\
                                       &sensors_config_chips_max,\
                                       sizeof(sensors_chip));

#define fits_add_el(el,list) sensors_add_array_el(el,\
                                                  &(list).fits,\
                                                  &(list).fits_count,\
                                                  &(list).fits_max, \
		                                  sizeof(sensors_chip_name));

/* YYERROR can only be called in rules, not in other functions, so this must
   be a macro */
#define check_current_chip()\
  do { if (! current_chip) {\
      sensors_yyerror("Label, Set or Compute statement before first chip statement");\
      YYERROR;\
    }\
  } while (0)

%}

%union {
  double value;
  char *name;
  void *nothing;
  sensors_chip_name_list chips;
  sensors_expr *expr;
  int bus;
  sensors_chip_name chip;
  int line;
}  

%left <nothing> '-' '+'
%left <nothing> '*' '/'
%left <nothing> NEG
%right <nothing> '^' '`'

%token <nothing> ','
%token <nothing> EOL
%token <line> BUS
%token <line> LABEL
%token <line> SET
%token <line> CHIP
%token <line> COMPUTE
%token <line> IGNORE
%token <value> FLOAT
%token <name> NAME
%token <nothing> ERROR

%type <chips> chip_name_list
%type <expr> expression
%type <bus> i2cbus_name
%type <name> adapter_name
%type <name> algorithm_name
%type <name> function_name
%type <name> string
%type <chip> chip_name

%start input

%%

input:	  /* empty */
	| input line
;

line:	  bus_statement EOL
	| label_statement EOL
	| set_statement EOL
	| chip_statement EOL
	| compute_statement EOL
	| ignore_statement EOL
	| error	EOL
;

bus_statement:	  BUS i2cbus_name adapter_name algorithm_name
		  { sensors_bus new_el;
		    new_el.lineno = $1;
                    new_el.number = $2;
                    new_el.adapter = $3;
                    new_el.algorithm = $4;
		    bus_add_el(&new_el);
		  }
;

label_statement:	  LABEL function_name string
			  { sensors_label new_el;
			    check_current_chip();
			    new_el.lineno = $1;
			    new_el.name = $2;
			    new_el.value = $3;
			    label_add_el(&new_el);
			  }
;

set_statement:	  SET function_name expression
		  { sensors_set new_el;
		    check_current_chip();
		    new_el.lineno = $1;
		    new_el.name = $2;
		    new_el.value = $3;
		    set_add_el(&new_el);
		  }
;

compute_statement:	  COMPUTE function_name expression ',' expression
			  { sensors_compute new_el;
			    check_current_chip();
			    new_el.lineno = $1;
			    new_el.name = $2;
			    new_el.from_proc = $3;
			    new_el.to_proc = $5;
			    compute_add_el(&new_el);
			  }
;

ignore_statement:	IGNORE function_name
			{ sensors_ignore new_el;
			  check_current_chip();
			  new_el.lineno = $1;
			  new_el.name = $2;
			  ignore_add_el(&new_el);
			}
;

chip_statement:	  CHIP chip_name_list
		  { sensors_chip new_el;
		    new_el.lineno = $1;
		    new_el.labels = NULL;
		    new_el.sets = NULL;
		    new_el.computes = NULL;
		    new_el.ignores = NULL;
		    new_el.labels_count = new_el.labels_max = 0;
		    new_el.sets_count = new_el.sets_max = 0;
		    new_el.computes_count = new_el.computes_max = 0;
		    new_el.ignores_count = new_el.ignores_max = 0;
		    new_el.chips = $2;
		    chip_add_el(&new_el);
		    current_chip = sensors_config_chips + 
		                   sensors_config_chips_count - 1;
		  }
;

chip_name_list:	  chip_name
		  { 
		    $$.fits = NULL;
		    $$.fits_count = $$.fits_max = 0;
		    fits_add_el(&$1,$$);
		  }
		| chip_name_list chip_name
		  { $$ = $1;
		    fits_add_el(&$2,$$);
		  }
;
	
expression:	  FLOAT	
		  { $$ = malloc_expr(); 
		    $$->data.val = $1; 
		    $$->kind = sensors_kind_val;
		  }
		| NAME
		  { $$ = malloc_expr(); 
		    $$->data.var = $1;
		    $$->kind = sensors_kind_var;
		  }
		| '@'
		  { $$ = malloc_expr();
		    $$->kind = sensors_kind_source;
		  }
		| expression '+' expression
		  { $$ = malloc_expr(); 
		    $$->kind = sensors_kind_sub;
		    $$->data.subexpr.op = sensors_add;
		    $$->data.subexpr.sub1 = $1;
		    $$->data.subexpr.sub2 = $3;
		  }
		| expression '-' expression
		  { $$ = malloc_expr(); 
		    $$->kind = sensors_kind_sub;
		    $$->data.subexpr.op = sensors_sub;
		    $$->data.subexpr.sub1 = $1;
		    $$->data.subexpr.sub2 = $3;
		  }
		| expression '*' expression
		  { $$ = malloc_expr(); 
		    $$->kind = sensors_kind_sub;
		    $$->data.subexpr.op = sensors_multiply;
		    $$->data.subexpr.sub1 = $1;
		    $$->data.subexpr.sub2 = $3;
		  }
		| expression '/' expression
		  { $$ = malloc_expr(); 
		    $$->kind = sensors_kind_sub;
		    $$->data.subexpr.op = sensors_divide;
		    $$->data.subexpr.sub1 = $1;
		    $$->data.subexpr.sub2 = $3;
		  }
		| '-' expression  %prec NEG
		  { $$ = malloc_expr(); 
		    $$->kind = sensors_kind_sub;
		    $$->data.subexpr.op = sensors_negate;
		    $$->data.subexpr.sub1 = $2;
		    $$->data.subexpr.sub2 = NULL;
		  }
		| '(' expression ')'
		  { $$ = $2; }
		| '^' expression
		  { $$ = malloc_expr(); 
		    $$->kind = sensors_kind_sub;
		    $$->data.subexpr.op = sensors_exp;
		    $$->data.subexpr.sub1 = $2;
		    $$->data.subexpr.sub2 = NULL;
		  }
		| '`' expression
		  { $$ = malloc_expr(); 
		    $$->kind = sensors_kind_sub;
		    $$->data.subexpr.op = sensors_log;
		    $$->data.subexpr.sub1 = $2;
		    $$->data.subexpr.sub2 = NULL;
		  }
;

i2cbus_name:	  NAME
		  { int res = sensors_parse_i2cbus_name($1,&$$);
		    free($1);
		    if (res) {
                      sensors_yyerror("Parse error in i2c bus name");
		      YYERROR;
                    }
		  }
;

adapter_name:	  NAME
		  { sensors_strip_of_spaces($1);
		    $$ = $1; }
;

algorithm_name:	  NAME
		  { sensors_strip_of_spaces($1);
		    $$ = $1; }
;

function_name:	  NAME
		  { $$ = $1; }
;

string:	  NAME
	  { $$ = $1; }
;

chip_name:	  NAME
		  { int res = sensors_parse_chip_name($1,&$$); 
		    free($1);
		    if (res) {
		      sensors_yyerror("Parse error in chip name");
		      YYERROR;
		    }
		  }
;

%%

void sensors_yyerror(const char *err)
{
  if (sensors_lex_error[0]) {
    sensors_parse_error(sensors_lex_error,sensors_yylineno);
    sensors_lex_error[0] = '\0';
  } else
    sensors_parse_error(err,sensors_yylineno);
}

sensors_expr *malloc_expr(void)
{
  sensors_expr *res = malloc(sizeof(sensors_expr));
  if (! res)
    sensors_fatal_error("malloc_expr","Allocating a new expression");
  return res;
}
  
