/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2026 tremor-invictus contributors

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

PLAYER COLORS

Enemy and team skin (off, bright, sport) and a palette colour for head,
upper and lower body. The palette index is kept in ui_<side><Part>Color
beside the hex value in cg_<side><Part>Color; any hex value typed in the
console still works and simply shows no palette entry.

=======================================================================
*/

#include "ui_local.h"

#define COLORS_FRAME		"menu/art/addbotframe"
#define ID_BACK				100
#define ID_SIDE_BASE		0		// side * 8 + row

#define NUM_PALETTE			12

typedef struct {
	int			name;			// stringId_t
	const char	*hex;
} paletteColor_t;

static const paletteColor_t palette[NUM_PALETTE] = {
	{ STR_COLOR_GREEN,		"0x00FF00FF" },
	{ STR_COLOR_CYAN,		"0x00FFFFFF" },
	{ STR_COLOR_RED,		"0xFF2020FF" },
	{ STR_COLOR_BLUE,		"0x2040FFFF" },
	{ STR_COLOR_YELLOW,		"0xFFFF00FF" },
	{ STR_COLOR_ORANGE,		"0xFF8000FF" },
	{ STR_COLOR_MAGENTA,	"0xFF00FFFF" },
	{ STR_COLOR_PURPLE,		"0x8000FFFF" },
	{ STR_COLOR_PINK,		"0xFF80C0FF" },
	{ STR_COLOR_WHITE,		"0xFFFFFFFF" },
	{ STR_COLOR_LIME,		"0x80FF00FF" },
	{ STR_COLOR_TEAL,		"0x00A0A0FF" }
};

static const char	*sideNames[2] = { "enemy", "team" };
static const char	*partNames[3] = { "Head", "Upper", "Lower" };
static const char	*skinValues[3] = { "", "bright", "sport" };

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	frame;
	menutext_s		sideLabel[2];
	menulist_s		skin[2];
	menulist_s		part[2][3];
	menuradiobutton_s	same[2];
	menutext_s		back;

	const char		*skinItems[4];
	const char		*colorItems[NUM_PALETTE + 1];
} colorsMenu_t;

static colorsMenu_t	s_colors;

/*
=================
Colors_SetPart
=================
*/
static void Colors_SetPart( int side, int part, int index ) {
	char	name[64];

	if ( index < 0 || index >= NUM_PALETTE ) {
		return;
	}
	Com_sprintf( name, sizeof( name ), "cg_%s%sColor", sideNames[side], partNames[part] );
	trap_Cvar_Set( name, palette[index].hex );
	Com_sprintf( name, sizeof( name ), "ui_%s%sColor", sideNames[side], partNames[part] );
	trap_Cvar_SetValue( name, index );
}

/*
=================
Colors_Event
=================
*/
static void Colors_Event( void *ptr, int notification ) {
	int		id, side, row, i;

	if ( notification != QM_ACTIVATED ) {
		return;
	}
	id = ((menucommon_s*)ptr)->id;
	if ( id == ID_BACK ) {
		UI_PopMenu();
		return;
	}

	side = id / 8;
	row = id % 8;
	if ( row == 0 ) {
		trap_Cvar_Set( side ? "cg_forceTeamSkin" : "cg_forceEnemySkin", skinValues[s_colors.skin[side].curvalue] );
	} else if ( row >= 1 && row <= 3 ) {
		Colors_SetPart( side, row - 1, s_colors.part[side][row - 1].curvalue );
		if ( s_colors.same[side].curvalue ) {
			for ( i = 0 ; i < 3 ; i++ ) {
				s_colors.part[side][i].curvalue = s_colors.part[side][row - 1].curvalue;
				Colors_SetPart( side, i, s_colors.part[side][i].curvalue );
			}
		}
	} else if ( row == 4 && s_colors.same[side].curvalue ) {
		for ( i = 0 ; i < 3 ; i++ ) {
			s_colors.part[side][i].curvalue = s_colors.part[side][1].curvalue;
			Colors_SetPart( side, i, s_colors.part[side][i].curvalue );
		}
	}
}

/*
=================
Colors_CurrentIndex

The palette entry a setting holds, from the ui_ index or the hex value
=================
*/
static int Colors_CurrentIndex( int side, int part ) {
	char	name[64], hex[32];
	int		i;

	Com_sprintf( name, sizeof( name ), "cg_%s%sColor", sideNames[side], partNames[part] );
	trap_Cvar_VariableStringBuffer( name, hex, sizeof( hex ) );
	for ( i = 0 ; i < NUM_PALETTE ; i++ ) {
		if ( !Q_stricmp( hex, palette[i].hex ) ) {
			return i;
		}
	}
	Com_sprintf( name, sizeof( name ), "ui_%s%sColor", sideNames[side], partNames[part] );
	i = (int)trap_Cvar_VariableValue( name );
	return ( i >= 0 && i < NUM_PALETTE ) ? i : 0;
}

/*
=================
Colors_MenuInit
=================
*/
static void Colors_MenuInit( void ) {
	char	value[32];
	int		side, part, i, x, y;
	static const int	partStrings[3] = { STR_MENU_HEAD, STR_MENU_UPPER, STR_MENU_LOWER };

	memset( &s_colors, 0, sizeof( s_colors ) );
	Colors_Cache();

	s_colors.skinItems[0] = BG_String( STR_SKIN_OFF );
	s_colors.skinItems[1] = BG_String( STR_SKIN_BRIGHT );
	s_colors.skinItems[2] = BG_String( STR_SKIN_SPORT );
	s_colors.skinItems[3] = NULL;
	for ( i = 0 ; i < NUM_PALETTE ; i++ ) {
		s_colors.colorItems[i] = BG_String( palette[i].name );
	}
	s_colors.colorItems[NUM_PALETTE] = NULL;

	s_colors.menu.wrapAround = qtrue;
	s_colors.menu.fullscreen = qfalse;

	s_colors.banner.generic.type	= MTYPE_BTEXT;
	s_colors.banner.generic.x		= 320;
	s_colors.banner.generic.y		= 16;
	s_colors.banner.string			= (char *)BG_String( STR_MENU_COLORS_TITLE );
	s_colors.banner.color			= color_white;
	s_colors.banner.style			= UI_CENTER;

	s_colors.frame.generic.type		= MTYPE_BITMAP;
	s_colors.frame.generic.flags	= QMF_INACTIVE;
	s_colors.frame.generic.name		= COLORS_FRAME;
	s_colors.frame.generic.x		= 320-233;
	s_colors.frame.generic.y		= 240-166;
	s_colors.frame.width			= 466;
	s_colors.frame.height			= 332;

	for ( side = 0 ; side < 2 ; side++ ) {
		x = side ? 470 : 250;
		y = 120;

		s_colors.sideLabel[side].generic.type	= MTYPE_TEXT;
		s_colors.sideLabel[side].generic.x		= x;
		s_colors.sideLabel[side].generic.y		= y;
		s_colors.sideLabel[side].string			= (char *)BG_String( side ? STR_MENU_TEAM : STR_MENU_ENEMY );
		s_colors.sideLabel[side].color			= color_orange;
		s_colors.sideLabel[side].style			= UI_LEFT|UI_SMALLFONT;
		y += 28;

		trap_Cvar_VariableStringBuffer( side ? "cg_forceTeamSkin" : "cg_forceEnemySkin", value, sizeof( value ) );
		s_colors.skin[side].generic.type		= MTYPE_SPINCONTROL;
		s_colors.skin[side].generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
		s_colors.skin[side].generic.name		= BG_String( STR_MENU_SKIN );
		s_colors.skin[side].generic.callback	= Colors_Event;
		s_colors.skin[side].generic.id			= side * 8;
		s_colors.skin[side].generic.x			= x;
		s_colors.skin[side].generic.y			= y;
		s_colors.skin[side].itemnames			= s_colors.skinItems;
		s_colors.skin[side].curvalue			= !Q_stricmp( value, "bright" ) ? 1 : !Q_stricmp( value, "sport" ) ? 2 : 0;
		y += 24;

		for ( part = 0 ; part < 3 ; part++ ) {
			s_colors.part[side][part].generic.type		= MTYPE_SPINCONTROL;
			s_colors.part[side][part].generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
			s_colors.part[side][part].generic.name		= BG_String( partStrings[part] );
			s_colors.part[side][part].generic.callback	= Colors_Event;
			s_colors.part[side][part].generic.id		= side * 8 + 1 + part;
			s_colors.part[side][part].generic.x			= x;
			s_colors.part[side][part].generic.y			= y;
			s_colors.part[side][part].itemnames			= s_colors.colorItems;
			s_colors.part[side][part].curvalue			= Colors_CurrentIndex( side, part );
			y += 20;
		}
		y += 8;

		s_colors.same[side].generic.type		= MTYPE_RADIOBUTTON;
		s_colors.same[side].generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
		s_colors.same[side].generic.name		= BG_String( STR_MENU_SAMEALL );
		s_colors.same[side].generic.callback	= Colors_Event;
		s_colors.same[side].generic.id			= side * 8 + 4;
		s_colors.same[side].generic.x			= x;
		s_colors.same[side].generic.y			= y;
		s_colors.same[side].curvalue			=
			s_colors.part[side][0].curvalue == s_colors.part[side][1].curvalue
			&& s_colors.part[side][1].curvalue == s_colors.part[side][2].curvalue;
	}

	s_colors.back.generic.type		= MTYPE_PTEXT;
	s_colors.back.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_colors.back.generic.x			= 320;
	s_colors.back.generic.y			= 340;
	s_colors.back.generic.id		= ID_BACK;
	s_colors.back.generic.callback	= Colors_Event;
	s_colors.back.string			= (char *)BG_String( STR_MENU_BACK );
	s_colors.back.color				= color_red;
	s_colors.back.style				= UI_CENTER|UI_SMALLFONT;

	Menu_AddItem( &s_colors.menu, &s_colors.frame );
	Menu_AddItem( &s_colors.menu, &s_colors.banner );
	for ( side = 0 ; side < 2 ; side++ ) {
		Menu_AddItem( &s_colors.menu, &s_colors.sideLabel[side] );
		Menu_AddItem( &s_colors.menu, &s_colors.skin[side] );
		for ( part = 0 ; part < 3 ; part++ ) {
			Menu_AddItem( &s_colors.menu, &s_colors.part[side][part] );
		}
		Menu_AddItem( &s_colors.menu, &s_colors.same[side] );
	}
	Menu_AddItem( &s_colors.menu, &s_colors.back );
}

/*
=================
Colors_Cache
=================
*/
void Colors_Cache( void ) {
	trap_R_RegisterShaderNoMip( COLORS_FRAME );
}

/*
=================
UI_ColorsMenu
=================
*/
void UI_ColorsMenu( void ) {
	Colors_MenuInit();
	UI_PushMenu( &s_colors.menu );
}
