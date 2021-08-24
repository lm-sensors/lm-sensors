/*
    test-scanner.c - Regression test driver for the libsensors config file scanner.
    Copyright (C) 2006 Mark M. Hoffman <mhoffman@lightlink.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>

#include "../data.h"
#include "../conf.h"
#include "../conf-parse.h"
#include "../scanner.h"

YYSTYPE sensors_yylval;

int main(void)
{
	int result;

	/* init the scanner */
	if ((result = sensors_scanner_init(stdin, NULL)))
		return result;

	do {
		result = sensors_yylex();

		printf("%d: ", sensors_yylineno);

		switch (result) {

			case 0:
				printf("EOF\n");
				break;

			case NEG:
				printf("NEG\n");
				break;

			case EOL:
				printf("EOL\n");
				break;

			case BUS:
				printf("BUS\n");
				break;

			case LABEL:
				printf("LABEL\n");
				break;

			case SET:
				printf("SET\n");
				break;

			case CHIP:
				printf("CHIP\n");
				break;

			case COMPUTE:
				printf("COMPUTE\n");
				break;

			case IGNORE:
				printf("IGNORE\n");
				break;

			case FLOAT:
				printf("FLOAT: %f\n", sensors_yylval.value);
				break;

			case NAME:
				printf("NAME: %s\n", sensors_yylval.name);
				free(sensors_yylval.name);
				break;

			case ERROR:
				printf("ERROR\n");
				break;

			default:
				printf("%c\n", (char)result);
				break;
		}

	} while (result);

	/* clean up the scanner */
	sensors_scanner_exit();

	return 0;
}
