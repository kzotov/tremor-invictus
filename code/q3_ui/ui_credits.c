/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
/*
=======================================================================

CREDITS

=======================================================================
*/


#include "ui_local.h"


typedef struct {
	menuframework_s	menu;
	int frame;
} creditsmenu_t;

static creditsmenu_t	s_credits;


/*
===============
UI_CreditMenu_Draw_ioq3
===============
*/
static void UI_CreditMenu_Draw_ioq3( void ) {
	int		y;
	int		i;

	// These are all people that have made commits to Subversion, and thus
	//  probably incomplete.
	// (These are in alphabetical order, for the defense of everyone's egos.)
	static const char *names[] = {
		"Tim Angus",
		"James Canete",
		"Vincent Cojot",
		"Ryan C. Gordon",
		"Aaron Gyes",
		"Zack Middleton",
		"Ludwig Nussel",
		"Julian Priestley",
		"Scirocco Six",
		"Thilo Schulz",
		"Zachary J. Slater",
		"Tony J. White",
		"...and many, many others!",  // keep this one last.
		NULL
	};

	// Center text vertically on the screen
	y = (SCREEN_HEIGHT - ARRAY_LEN(names) * (1.42 * PROP_HEIGHT * PROP_SMALL_SIZE_SCALE)) / 2;

	UI_DrawProportionalString( 320, y, BG_String( STR_CR_IOQ3 ), UI_CENTER|UI_SMALLFONT, color_white );
	y += 1.42 * PROP_HEIGHT * PROP_SMALL_SIZE_SCALE;

	for (i = 0; names[i]; i++) {
		UI_DrawProportionalString( 320, y, names[i], UI_CENTER|UI_SMALLFONT, color_white );
		y += 1.42 * PROP_HEIGHT * PROP_SMALL_SIZE_SCALE;
	}

	UI_DrawString( 320, 459, "https://www.ioquake3.org/", UI_CENTER|UI_SMALLFONT, color_red );
}


/*
=================
UI_CreditMenu_Key

Any key turns the page; after the last one, back to the menu
=================
*/
static sfxHandle_t UI_CreditMenu_Key( int key ) {
	if( key & K_CHAR_FLAG ) {
		return 0;
	}

	s_credits.frame++;
	if (s_credits.frame == 1) {
		s_credits.menu.draw = UI_CreditMenu_Draw_ioq3;
	} else {
		UI_PopMenu();
	}
	return 0;
}


/*
===============
UI_CreditMenu_Draw

The game, its licence, the font and the libraries
===============
*/
static void UI_CreditMenu_Draw( void ) {
	static const int	lines[] = {
		STR_CR_ENGINE, STR_CR_IDCOPY, -1, STR_CR_GPL1, STR_CR_GPL2, -1, STR_CR_FONT, -1
	};
	static const int	libs[] = { STR_CR_LIB1, STR_CR_LIB2, STR_CR_LIB3 };
	int		y, i;

	UI_DrawBannerString( 320, 16, BG_String( STR_CR_TITLE ), UI_CENTER, color_white );

	y = 76;
	UI_DrawProportionalString( 320, y, BG_String( STR_CR_GAME ), UI_CENTER, color_white );
	y += PROP_HEIGHT + 4;
	UI_DrawString( 320, y, BG_String( STR_CR_MADEBY ), UI_CENTER|UI_SMALLFONT, color_white );
	y += SMALLCHAR_HEIGHT + 2;
	UI_DrawString( 320, y, BG_String( STR_CR_SITE ), UI_CENTER|UI_SMALLFONT, color_red );
	y += SMALLCHAR_HEIGHT * 2;

	for ( i = 0; i < ARRAY_LEN( lines ); i++ ) {
		if ( lines[i] < 0 ) {
			y += SMALLCHAR_HEIGHT;
			continue;
		}
		UI_DrawString( 320, y, BG_String( lines[i] ), UI_CENTER|UI_SMALLFONT, text_color_normal );
		y += SMALLCHAR_HEIGHT + 2;
	}

	UI_DrawString( 320, y, BG_String( STR_CR_THIRDPARTY ), UI_CENTER|UI_SMALLFONT, color_white );
	y += SMALLCHAR_HEIGHT + 6;
	for ( i = 0; i < ARRAY_LEN( libs ); i++ ) {
		UI_DrawString( 320, y, BG_String( libs[i] ), UI_CENTER|UI_SMALLFONT, text_color_normal );
		y += SMALLCHAR_HEIGHT + 2;
	}

	UI_DrawString( 320, 456, BG_String( STR_CR_NEXT ), UI_CENTER|UI_SMALLFONT, text_color_disabled );
}


/*
===============
UI_CreditMenu
===============
*/
void UI_CreditMenu( void ) {
	memset( &s_credits, 0 ,sizeof(s_credits) );

	s_credits.menu.draw = UI_CreditMenu_Draw;
	s_credits.menu.key = UI_CreditMenu_Key;
	s_credits.menu.fullscreen = qtrue;
	UI_PushMenu ( &s_credits.menu );
}
