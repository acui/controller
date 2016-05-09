/* Copyright (C) 2016 by Cui Yuting
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file.  If not, see <http://www.gnu.org/licenses/>.
 */

// Compilier Includes
#include <Lib/MacroLib.h>

// Project Includes
#include <cli.h>
#include <kll_defs.h>
#include <led.h>
#include <print.h>
#include <lcd_scan.h>

// Interconnect module if compiled in
#if defined(ConnectEnabled_define)
#include <connect_scan.h>
#endif

// Local Includes
#include "calc.h"

// ----- Defines -----

// ----- Macros -----

// ----- Structs -----

// ----- Function Declarations -----

// CLI Functions
void cliFunc_calcNumber ( char* args );

// ----- Variables -----

// Scan Module command dictionary
CLIDict_Entry( calcNumber,     "Input number to the Calculator." );

CLIDict_Def( calcCLIDict, "Calculator Module Commands" ) = {
	CLIDict_Item( calcNumber ),
	{ 0, 0, 0 } // Null entry for dictionary end
};

// ----- Interrupt Functions -----

// ----- Functions -----

// ----- CLI Command Functions -----
void cliFunc_calcNumber ( char* args )
{
	char* curArgs;
	char* arg1Ptr;
	char* arg2Ptr = args;

	curArgs = arg2Ptr;
	CLI_argumentIsolation( curArgs, &arg1Ptr, &arg2Ptr );

	// Stop processing args if no more are found
	if ( *arg1Ptr == '\0' )
	    return;

	STLcd_drawBitmap( 0, 0, 0, 0, 0 );

	uint8_t value = numToInt( arg1Ptr );
}
