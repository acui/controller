/* Copyright (C) 2016 by Cui Yuting
 * Copyright (C) 2015-2016 by Jacob Alexander
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
#include <string.h>

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
void cliFunc_calcChar      ( char* args );
void cliFunc_calcInitialize  ( char* args );

// ----- Variables -----
const static uint8_t CalcFont[] = { CalcFont_define };
static uint8_t CalcFontWidth = CalcFontWidth_define;
static uint8_t CalcFontHeight = CalcFontHeight_define;
static uint8_t CalcFontSize = CalcFontSize_define;
static uint8_t CalcFontLength = CalcFontLength_define;
static uint8_t CalcLineHeight = CalcLineHeight_define;
static uint8_t CalcLetterSpace = CalcLetterSpace_define;
static uint8_t CalcStackDispOffset = 0;
static uint8_t CalcLineMax = 0;
static uint8_t CalcColumnMax = 0;
static uint8_t CalcOutputBuffer[256];

// Scan Module command dictionary
CLIDict_Entry( calcInitialize,   "Initialize the Calculator." );
CLIDict_Entry( calcChar,         "Input charactor to the Calculator." );


CLIDict_Def( calcCLIDict, "Calculator Module Commands" ) = {
	CLIDict_Item( calcInitialize ),
	CLIDict_Item( calcChar ),
	{ 0, 0, 0 } // Null entry for dictionary end
};

// ----- Interrupt Functions -----

// ----- Functions -----
void Calc_drawChar( uint8_t line, uint8_t column, uint8_t c )
{
	if ( c >= CalcFontLength )
		c = 0;
	uint8_t startx = column * ( CalcFontWidth + CalcLetterSpace );
	uint8_t starty = line * CalcLineHeight;
	STLcd_drawBitmap( CalcFont + c * CalcFontSize, startx, starty, CalcFontWidth, CalcFontHeight );
}

void Calc_bufferFlush( uint8_t line )
{
	if ( line >= CalcLineMax )
		return;
	for ( uint8_t i = 0; i < CalcColumnMax; i++ )
		Calc_drawChar(line, i, CalcOutputBuffer[i]);
	STLcd_updateBoundingBox( 0, line * CalcLineHeight, LCD_WIDTH, line * CalcLineHeight + CalcFontHeight );
	memset( CalcOutputBuffer, 0x20, CalcColumnMax );
}

static int32_t CalcStack[256];
static int16_t CalcStackTop;

static int32_t CalcCurrentNumber;
static uint8_t CalcHasCurrentNumber = 0; // 0 and 1 are all empty.
static uint8_t CalcCurrentBase;
static uint8_t CalcCurrentMaxLength;
static uint8_t CalcOutput[256];


void Calc_reverseString( uint8_t *str, uint8_t length )
{
	for ( uint8_t offset = 0; offset < length / 2; offset++ )
	{
		uint8_t c = str[ offset ];
		str[ offset ] = str[ length - offset - 1];
		str[ length - offset - 1 ] = c;
	}
}

void Calc_bufferWriteStackIndex( int16_t index )
{
	if ( index < 0 )
		return;
	CalcOutputBuffer[3] = ':';
	if ( index == 0 )
	{
		CalcOutputBuffer[2] = '0';
		return;
	}
	uint8_t offset = 2;
	while ( index > 0 )
	{
		CalcOutputBuffer[ offset ] = index % 10 + '0';
		index /= 10;
		offset--;
	}
}


void Calc_outputString( int16_t stackindex, uint8_t *str, uint8_t length )
{
	Calc_bufferWriteStackIndex( stackindex );
	uint8_t maxindex = ( length > ( CalcColumnMax - 4 ) ) ? ( CalcColumnMax - 4 ) : length;

	for ( uint8_t i = 0; i < maxindex; i++ )
		CalcOutputBuffer[ CalcColumnMax - i - 1 ] = str[ length - i - 1 ];

	if ( length == 0 )
		CalcOutputBuffer[ CalcColumnMax - 1] = '.';
}

void Calc_error( const char *str )
{
	for ( uint8_t i = 0; str[i] != 0; i++ )
		CalcOutputBuffer[i] = str[i];
	Calc_bufferFlush( 0 );
}

uint8_t Calc_convertToCharUnsigned( int32_t _n )
{
	uint8_t length = 1;
	uint8_t numberstart = 1;
	uint32_t n = (uint32_t)_n;
	if ( n == 0 )
	{
		CalcOutput[ length++ ] = '0';
		return 1;
	}
	CalcOutput[length++] = '0';
	if ( CalcCurrentBase == 2 )
		CalcOutput[length++] = 'b';
	else if ( CalcCurrentBase == 16 )
		CalcOutput[length++] = 'x';
	numberstart = length;
	while ( n > 0 )
	{
		uint8_t m = n % CalcCurrentBase;
		if ( m < 10 )
			m += '0';
		else
			m += 'A' - 10;
		CalcOutput[ length++ ] = m;
		n /= CalcCurrentBase;
	}
	Calc_reverseString( CalcOutput + numberstart, length - numberstart );
	return length - 1;
}

uint8_t Calc_convertToChar( int32_t n )
{
	if ( CalcCurrentBase != 10 )
		return Calc_convertToCharUnsigned( n );

	uint8_t length = 1;
	uint8_t numberstart = 1;
	if ( n == 0 )
	{
		CalcOutput[ length++ ] = '0';
		return 1;
	}
	if ( n < 0 )
	{
		CalcOutput[ length++ ] = '-';
		n = -n;
	}
	numberstart = length;
	while ( n > 0 )
	{
		uint8_t m = n % CalcCurrentBase;
		if ( m < 10 )
			m += '0';
		else
			m += 'A' - 10;
		CalcOutput[ length++ ] = m;
		n /= CalcCurrentBase;
	}
	Calc_reverseString( CalcOutput + numberstart, length - numberstart );
	return length - 1;
}

void Calc_outputCurrentNumber()
{
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
#endif
	uint8_t length = 0;
	if ( CalcHasCurrentNumber & 0x02 )
	{
		length = Calc_convertToChar( CalcCurrentNumber );
	}
	if ( CalcHasCurrentNumber & 0x01 )
	{
		CalcOutput[0] = '-';
		Calc_outputString ( -1, CalcOutput, length + 1 );
	}
	else
		Calc_outputString( -1 , CalcOutput + 1, length );
	Calc_bufferFlush( 0 );
#if defined(ConnectEnabled_define)
	}
#endif
}

void Calc_outputStacks()
{
	int16_t start = CalcStackTop - CalcStackDispOffset - CalcLineMax + 1;
	if ( start < 0 )
		start = 0;
	int16_t end = CalcStackTop - CalcStackDispOffset + 1;
	if ( end < 0 )
		end = 0;
	if ( end > CalcStackTop )
		end = CalcStackTop;
	for ( int16_t i = start; i < end; i++ )
	{
		uint8_t length = Calc_convertToChar( CalcStack[i] );
		Calc_outputString( i + 1, CalcOutput + 1, length );
		Calc_bufferFlush( CalcStackTop - i - CalcStackDispOffset );
	}
}

void Calc_num( int16_t n )
{
	uint8_t length = 0;
	if ( n == -1 )
	{
		CalcCurrentNumber /= CalcCurrentBase;
		if ( CalcCurrentNumber == 0 )
			CalcHasCurrentNumber &= 0x01;
	}
	else if ( n >= CalcCurrentBase && n != 100 && n != 1000 )
	{
		return;
	}
	else
	{
		if ( CalcHasCurrentNumber < 2 )
		{
			if ( ( n == 100 ) | ( n == 1000 ) )
				n = 0;
			CalcCurrentNumber = n;
			if ( n < 10 )
				CalcOutput[ length++ ] = n + '0';
			else
				CalcOutput[ length++ ] = n + 'A';
			CalcHasCurrentNumber |= 2;
		}
		else
		{
			uint8_t length = Calc_convertToChar( CalcCurrentNumber );
			if ( n == 100 )
			{
				if ( length + 2 > CalcCurrentMaxLength )
					return;
				else if ( CalcCurrentNumber == 0 )
					return;
				CalcCurrentNumber *= ( CalcCurrentBase * CalcCurrentBase );

			}
			else if ( n == 1000 )
			{
				if ( length + 3 > CalcCurrentMaxLength )
					return;
				else if ( CalcCurrentNumber == 0 )
					return;
				CalcCurrentNumber *= ( CalcCurrentBase * CalcCurrentBase * CalcCurrentBase );
			}
			else
			{
				if ( length + 1 > CalcCurrentMaxLength )
					return;
				CalcCurrentNumber = CalcCurrentNumber * CalcCurrentBase + n;
 			}
		}
	}
	Calc_outputCurrentNumber();
}

void Calc_pop()
{
	if ( CalcStackTop > 0 )
	{
		CalcStackTop--;
		STLcd_clear();
		Calc_outputStacks();
		Calc_outputCurrentNumber();
	}
	else
		Calc_error( "Too few elements on stack" );
}

void Calc_toggleSign()
{
	CalcHasCurrentNumber ^= 1;
	Calc_outputCurrentNumber();
}

void Calc_char( int8_t c )
{
	int16_t n = 0;
	switch ( c )
	{
	case 0x08:
		if ( CalcHasCurrentNumber == 1 )
			Calc_toggleSign();
		else if ( CalcHasCurrentNumber > 1 )
			Calc_num( -1 );
		else
			Calc_pop();
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		n = c - '0';
		Calc_num( n );
		break;
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		n = c - 'A' + 0x0a;
		Calc_num( n );
		break;
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
		n = c - 'a' + 0x0a;
		Calc_num( n );
		break;
	case 'h':
		Calc_num( 100 );
		break;
	case 'k':
		Calc_num( 1000 );
		break;
	}
}



void Calc_duplicate()
{
	if ( CalcStackTop < 256 )
	{
		CalcStack[ CalcStackTop ] = CalcStack[ CalcStackTop - 1 ];
		CalcStackTop++;
		STLcd_clear();
		Calc_outputStacks();
	}
	else
	{
		Calc_error( "the stack is full." );
		return;
	}
}

uint8_t Calc_pushImp()
{
	if ( !( CalcHasCurrentNumber & 0x02 ))
	{
		Calc_error( "Bad Format" );
		return 0;
	}
	if ( CalcStackTop < 256 )
	{
		CalcStack[ CalcStackTop++ ] = ( CalcHasCurrentNumber & 0x01 ) ? -CalcCurrentNumber : CalcCurrentNumber;
		CalcHasCurrentNumber = 0;
		return 1;
	}
	else
	{
		Calc_error( "the stack is full." );
		CalcHasCurrentNumber = 0;
		return 0;
	}
}

void Calc_push()
{
	if ( Calc_pushImp() )
	{
		STLcd_clear();
		Calc_outputStacks();
		Calc_outputCurrentNumber();
	}
}

void Calc_enter()
{
	if ( CalcHasCurrentNumber )
	{
		Calc_push();
	}
	else
	{
		Calc_duplicate();
	}
}

void Calc_clear()
{
	CalcStackTop = 0;
	CalcHasCurrentNumber = 0;
	STLcd_clear();
	Calc_outputStacks();
	Calc_outputCurrentNumber();
}

void Calc_clearEntry()
{
	CalcHasCurrentNumber = 0;
	Calc_outputCurrentNumber();
}

void Calc_swap()
{
	if ( CalcHasCurrentNumber & 0x02 )
	{
		if ( !Calc_pushImp() )
			return;
	}
	if ( CalcStackTop > 1 )
	{
		int32_t t = CalcStack[ CalcStackTop - 1];
		CalcStack[ CalcStackTop - 1] = CalcStack[ CalcStackTop - 2];
		CalcStack[ CalcStackTop - 2] = t;
	}
	STLcd_clear();
	Calc_outputStacks();
	Calc_outputCurrentNumber();
}

void Calc_rotate()
{
	if ( CalcHasCurrentNumber & 0x02 )
	{
		if ( !Calc_pushImp() )
			return;
	}
	if ( CalcStackTop > 2 )
	{
		int32_t t = CalcStack[ CalcStackTop - 1];
		CalcStack[ CalcStackTop - 1] = CalcStack[ CalcStackTop - 3];
		CalcStack[ CalcStackTop - 3] = CalcStack[ CalcStackTop - 2];
		CalcStack[ CalcStackTop - 2] = t;
		STLcd_clear();
		Calc_outputStacks();
		Calc_outputCurrentNumber();
	}
	else
	{
		Calc_swap();
	}
}

void Calc_mode( uint8_t mode )
{
	switch ( mode )
	{
	case 0:
		CalcCurrentBase = 10;
		CalcCurrentMaxLength = 11;
		break;
	case 'b':
		CalcCurrentBase = 2;
		CalcCurrentMaxLength = 34;
		break;
	case 'o':
		CalcCurrentBase = 8;
		CalcCurrentMaxLength = 12;
		break;
	case 'x':
		CalcCurrentBase = 16;
		CalcCurrentMaxLength = 10;
		break;
	}
	STLcd_clear();
	Calc_outputStacks();
	Calc_outputCurrentNumber();
}

void Calc_unaryOperator(uint8_t operator)
{
	if ( CalcHasCurrentNumber )
		if ( !Calc_pushImp() )
			return;
	if ( CalcStackTop < 1 )
	{
		Calc_error( "Too few elements on stack" );
		return;
	}
	switch ( operator )
	{
	case '~':
		CalcStack[ CalcStackTop - 1 ] = ~CalcStack[ CalcStackTop - 1];
		break;
	}
	STLcd_clear();
	Calc_outputStacks();
	Calc_outputCurrentNumber();
}

void Calc_binaryOperator(uint8_t operator)
{
	if ( CalcHasCurrentNumber )
		if ( !Calc_pushImp() )
			return;
	if ( CalcStackTop < 2 )
	{
		Calc_error( "Too few elements on stack" );
		return;
	}
	int32_t t;
	switch ( operator )
	{
	case '+':
		CalcStack[ CalcStackTop - 2 ] += CalcStack[ CalcStackTop - 1];
		break;
	case '-':
		CalcStack[ CalcStackTop - 2 ] -= CalcStack[ CalcStackTop - 1];
		break;
	case '*':
		CalcStack[ CalcStackTop - 2 ] *= CalcStack[ CalcStackTop - 1];
		break;
	case '/':
		t = CalcStack[ CalcStackTop - 1];
		CalcStack[ CalcStackTop - 1 ] = CalcStack[ CalcStackTop - 2 ] / CalcStack[ CalcStackTop - 1 ];
		CalcStack[ CalcStackTop - 2 ] = CalcStack[ CalcStackTop - 2 ] % t;
		CalcStackTop++;
		break;
	case '|':
		CalcStack[ CalcStackTop - 2 ] |= CalcStack[ CalcStackTop - 1];
		break;
	case '&':
		CalcStack[ CalcStackTop - 2 ] &= CalcStack[ CalcStackTop - 1];
		break;
	case '^':
		CalcStack[ CalcStackTop - 2 ] ^= CalcStack[ CalcStackTop - 1];
	}
	CalcStackTop--;
	STLcd_clear();
	Calc_outputStacks();
	Calc_outputCurrentNumber();
}

void Calc_ternaryOperator(uint8_t operator)
{
	if ( CalcHasCurrentNumber )
		if ( !Calc_pushImp() )
			return;
	if ( CalcStackTop < 3 )
	{
		Calc_error( "Too few elements on stack" );
		return;
	}
	STLcd_clear();
	Calc_outputStacks();
	Calc_outputCurrentNumber();
}

void Calc_initialize()
{
	STLcd_clear();
	CalcHasCurrentNumber = 0;
	CalcStackTop = 0;
	CalcLineMax = ( LCD_HEIGHT - CalcFontHeight ) / CalcLineHeight + 1;
	CalcColumnMax = ( LCD_WIDTH - CalcFontWidth ) / ( CalcFontWidth + CalcLetterSpace ) + 1;
	CalcStackDispOffset = 0;
	memset( CalcOutputBuffer, 0x20, CalcColumnMax );
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		CalcStackDispOffset = 0;
	}
	else
	{
		CalcStackDispOffset = CalcLineMax;
	}
#endif
	Calc_mode( 0 );

}

void Calc_inputChar( uint8_t c )
{
	switch (c)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 0x08:
		Calc_char( c );
		break;
	case 'b':
	case 'o':
	case 'x':
		Calc_mode( c );
		break;
	case 'd':
		Calc_mode( 0 );
		break;
	case 'H':
		Calc_char( 'h' );
		break;
	case 'K':
		Calc_char( 'k' );
		break;
	case '+':
	case '-':
	case '*':
	case '/':
	case '|':
	case '&':
	case '^':
		Calc_binaryOperator( c );
		break;
	case '~':
		Calc_unaryOperator( c );
		break;
	case '_':
		Calc_toggleSign();
		break;
	case 's':
		Calc_swap();
		break;
	case 'r':
		Calc_rotate();
		break;
	case '\n':
		Calc_enter();
		break;
	case 'c':
		Calc_clear();
		break;
	case 'e':
		Calc_clearEntry();
		break;
	default:
		print("Unknown key 0x");
		printHex(c);
		break;
	}
}

void Calc_initialize_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_initialize_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_initialize();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_initialize_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_initialize_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_enter_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_enter_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_enter();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_enter_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_enter_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_clear_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_clear_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_clear();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_clear_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_clear_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_clearEntry_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_clearEntry_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_clearEntry();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_clearEntry_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_clearEntry_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_duplicate_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_duplicate_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_duplicate();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_duplicate_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_duplicate_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_pop_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_pop_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_pop();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_pop_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_pop_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_swap_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_swap_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_swap();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_swap_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_swap_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_rotate_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_rotate_capability()");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	Calc_rotate();
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_rotate_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_rotate_capability_index ].argCount,
			0
		);
	}
#endif
}

void Calc_char_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_char_capability(c)");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	uint8_t c = *args;
	Calc_char( c );
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_char_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_char_capability_index ].argCount,
			args
		);
	}
#endif
}

void Calc_unaryOperator_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_unaryOperator_capability(operator)");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	uint8_t c = *args;
	Calc_unaryOperator( c );
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_unaryOperator_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_unaryOperator_capability_index ].argCount,
			args
		);
	}
#endif
}

void Calc_binaryOperator_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_binaryOperator_capability(operator)");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	uint8_t c = *args;
	Calc_binaryOperator( c );
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_binaryOperator_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_binaryOperator_capability_index ].argCount,
			args
		);
	}
#endif
}

void Calc_ternaryOperator_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_ternaryOperator_capability(operator)");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	uint8_t c = *args;
	Calc_ternaryOperator( c );
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_ternaryOperator_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_ternaryOperator_capability_index ].argCount,
			args
		);
	}
#endif
}

void Calc_mode_capability( uint8_t state, uint8_t stateType, uint8_t *args )
{
	// Display capability name
	if ( stateType == 0xFF && state == 0xFF )
	{
		print("Calc_mode_capability(mode)");
		return;
	}

	if ( stateType == 0x00 )
	{
		if ( state != 0x01 )
		{
			return;
		}
	}

	uint8_t c = *args;
	Calc_mode( c );
#if defined(ConnectEnabled_define)
	if ( Connect_master )
	{
		// generatedKeymap.h
		extern const Capability CapabilitiesList[];

		// Broadcast layerStackExact remote capability (0xFF is the broadcast id)
		Connect_send_RemoteCapability(
			0xFF,
			Calc_mode_capability_index,
			state,
			stateType,
			CapabilitiesList[ Calc_mode_capability_index ].argCount,
			args
		);
	}
#endif
}

inline void Calc_setup()
{
	CLI_registerDictionary( calcCLIDict, calcCLIDictName );
}



// ----- CLI Command Functions -----

void cliFunc_calcInitialize ( char* args )
{
	Calc_initialize();
}

void cliFunc_calcChar ( char* args )
{
	char* curArgs;
	char* arg1Ptr;
	char* arg2Ptr = args;

	curArgs = arg2Ptr;
	CLI_argumentIsolation( curArgs, &arg1Ptr, &arg2Ptr );

	// Stop processing args if no more are found
	if ( *arg1Ptr == '\0' )
	    return;

	uint8_t c = numToInt( arg1Ptr );
	Calc_inputChar( c );
}
