/* Copyright (C) 2015-2016 by Jacob Alexander
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

#pragma once

// ----- Includes -----

// Compiler Includes
#include <stdint.h>

// ----- Defines -----
#define LCD_WIDTH 128
#define LCD_HEIGHT 32

// ----- Functions -----

void LCD_setup();
uint8_t LCD_scan();

void LCD_currentChange( unsigned int current );

void STLcd_drawBitmap( const uint8_t *bitmap, uint8_t x, uint8_t y, uint8_t width, uint8_t height );
void STLcd_updateBoundingBox( uint8_t xmin, uint8_t ymin, uint8_t xmax, uint8_t ymax );
void STLcd_clear();
