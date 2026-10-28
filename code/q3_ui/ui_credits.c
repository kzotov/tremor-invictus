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

static const char *ioq3Names[] = {
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


	// Center text vertically on the screen
	y = (SCREEN_HEIGHT - ARRAY_LEN(ioq3Names) * (1.42 * PROP_HEIGHT * PROP_SMALL_SIZE_SCALE)) / 2;

	UI_DrawProportionalString( 320, y, BG_String( STR_CR_IOQ3 ), UI_CENTER|UI_SMALLFONT, color_white );
	y += 1.42 * PROP_HEIGHT * PROP_SMALL_SIZE_SCALE;

	for (i = 0; ioq3Names[i]; i++) {
		UI_DrawProportionalString( 320, y, ioq3Names[i], UI_CENTER|UI_SMALLFONT, color_white );
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
	static const int	libs[] = { STR_CR_LIB1, STR_CR_LIB2, STR_CR_LIB3, STR_CR_LIB4 };
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

/*
===============
UI_CreditsRml

The credits as an RmlUi page, written from the string table to
ui-gen/credits.rml in the manner of a film poster's billing block: the
title spaced wide, then lines of a small two-word role stacked before a
tall condensed name, the contributors and libraries in columns, and a
closing card with the copyright and licence
===============
*/

// a role ("MADE|BY") stacked beside its name
static void Credits_Billing( char *page, int size, int role, int name ) {
	char	words[64], *bar;

	Q_strncpyz( words, BG_String( role ), sizeof( words ) );
	bar = strchr( words, '|' );
	if ( bar ) {
		*bar++ = 0;
	}
	Q_strcat( page, size, va( "<div class=\"bill\"><span class=\"role\"><span>%s</span>", UI_RmlText( words ) ) );
	Q_strcat( page, size, va( "<span>%s</span></span><span class=\"name\">%s</span></div>\n", bar ? UI_RmlText( bar ) : "",
		UI_RmlText( BG_String( name ) ) ) );
}

void UI_CreditsRml( void ) {
	static char		page[32768];
	char			title[128], libs[MAX_STRING_CHARS], *lib, *next, *bar;
	const char		*t;
	fileHandle_t	f;
	int				i;

	page[0] = 0;
	Q_strcat( page, sizeof( page ), "<rml>\n<head>\n<title>Credits</title>\n<link type=\"text/rcss\" href=\"/ui/tremor.rcss\"/>\n</head>\n"
		"<body data-escape=\"rml_show ui/main.rml\">\n<div class=\"credits\">\n" );

	// the title, its letters spread out
	title[0] = 0;
	for ( t = BG_String( STR_CR_GAME ) ; *t && strlen( title ) < sizeof( title ) - 12 ; t++ ) {
		Q_strcat( title, sizeof( title ), *t == ' ' ? "&#160; &#160;" : va( "%c ", toupper( *t ) ) );
	}
	Q_strcat( page, sizeof( page ), va( "<div class=\"crtitle\">%s</div>\n", title ) );		// (letters only: no escaping)

	// the billing block
	Q_strcat( page, sizeof( page ), "<div class=\"billing\">\n" );
	Credits_Billing( page, sizeof( page ), STR_CRB_MADEBY, STR_CRB_MADEBY_NAME );
	Credits_Billing( page, sizeof( page ), STR_CRB_BUILTON, STR_CRB_BUILTON_NAME );
	Credits_Billing( page, sizeof( page ), STR_CRB_LOGO, STR_CRB_LOGO_NAME );
	Credits_Billing( page, sizeof( page ), STR_CRB_TYPE, STR_CRB_TYPE_NAME );
	Credits_Billing( page, sizeof( page ), STR_CRB_MENUS, STR_CRB_MENUS_NAME );
	Q_strcat( page, sizeof( page ), "</div>\n" );

	// the engine's contributors, in columns
	Q_strcat( page, sizeof( page ), va( "<div class=\"crhead\">%s</div>\n<div class=\"crgrid\">", UI_RmlText( BG_String( STR_CRB_IOQ3 ) ) ) );
	for ( i = 0; ioq3Names[i]; i++ ) {
		Q_strcat( page, sizeof( page ), va( "<span class=\"crcell\">%s</span>", UI_RmlText( ioq3Names[i] ) ) );
	}
	Q_strcat( page, sizeof( page ), "</div>\n" );

	// the libraries, each with its licence beside it
	Q_strcat( page, sizeof( page ), va( "<div class=\"crhead\">%s</div>\n<div class=\"crgrid\">", UI_RmlText( BG_String( STR_CRB_LIBHEAD ) ) ) );
	Q_strncpyz( libs, BG_String( STR_CRB_LIBS ), sizeof( libs ) );
	for ( lib = libs ; lib && *lib ; lib = next ) {
		next = strchr( lib, ';' );
		if ( next ) {
			*next++ = 0;
		}
		bar = strchr( lib, '|' );
		if ( bar ) {
			*bar++ = 0;
		}
		Q_strcat( page, sizeof( page ), va( "<span class=\"crcell\">%s <span class=\"lic\">%s</span></span>",
			UI_RmlText( lib ), bar ? UI_RmlText( bar ) : "" ) );
	}
	Q_strcat( page, sizeof( page ), "</div>\n" );

	// the closing card
	Q_strcat( page, sizeof( page ), "<div class=\"crclose\">" );
	Q_strcat( page, sizeof( page ), va( "<div>%s</div>", UI_RmlText( BG_String( STR_CR_IDCOPY ) ) ) );
	Q_strcat( page, sizeof( page ), va( "<div>%s %s</div>", UI_RmlText( BG_String( STR_CR_GPL1 ) ), UI_RmlText( BG_String( STR_CR_GPL2 ) ) ) );
	Q_strcat( page, sizeof( page ), va( "<div>%s</div>", UI_RmlText( BG_String( STR_CR_LIB4 ) ) ) );
	Q_strcat( page, sizeof( page ), va( "<div class=\"site\">%s</div></div>\n", UI_RmlText( BG_String( STR_CR_SITE ) ) ) );
	Q_strcat( page, sizeof( page ), "<div class=\"sactions\"><button onclick=\"rml_show ui/main.rml\">[menu_back]</button></div>\n</div>\n</body>\n</rml>\n" );

	trap_FS_FOpenFile( "ui-gen/credits.rml", &f, FS_WRITE );
	if ( !f ) {
		UI_CreditMenu();
		return;
	}
	trap_FS_Write( page, strlen( page ), f );
	trap_FS_FCloseFile( f );
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui-gen/credits.rml\n" );
}
