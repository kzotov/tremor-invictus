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

CHAMPION SELECT

=======================================================================
*/

#include "ui_local.h"

#define CHAMPION_FRAME		"menu/art/addbotframe"
#define CHAMPION_ROWS		8
#define CHAMPION_SPACING	26
#define MAX_MENU_CHAMPIONS	32

#define ID_BACK				100

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	frame;
	menutext_s		champions[MAX_MENU_CHAMPIONS];
	menutext_s		hint;
	menutext_s		back;
} championMenu_t;

static championMenu_t	s_champion;


/*
=================
Champion_Event
=================
*/
static void Champion_Event( void *ptr, int notification ) {
	int		id;

	if ( notification != QM_ACTIVATED ) {
		return;
	}

	id = ((menucommon_s*)ptr)->id;
	if ( id == ID_BACK ) {
		UI_PopMenu();
		return;
	}
	if ( id >= 0 && id < bg_numChampions ) {
		trap_Cvar_Set( "champion", bg_championList[id].name );
		UI_ForceMenuOff();
	}
}


/*
=================
Champion_MenuInit
=================
*/
static void Champion_MenuInit( void ) {
	char	current[MAX_QPATH];
	int		i, count;

	memset( &s_champion, 0, sizeof( s_champion ) );

	Champion_Cache();

	s_champion.menu.wrapAround = qtrue;
	s_champion.menu.fullscreen = qfalse;

	s_champion.banner.generic.type		= MTYPE_BTEXT;
	s_champion.banner.generic.x			= 320;
	s_champion.banner.generic.y			= 16;
	s_champion.banner.string			= (char *)BG_String( STR_MENU_CHAMPION_TITLE );
	s_champion.banner.color				= color_white;
	s_champion.banner.style				= UI_CENTER;

	s_champion.frame.generic.type		= MTYPE_BITMAP;
	s_champion.frame.generic.flags		= QMF_INACTIVE;
	s_champion.frame.generic.name		= CHAMPION_FRAME;
	s_champion.frame.generic.x			= 320-233;
	s_champion.frame.generic.y			= 240-166;
	s_champion.frame.width				= 466;
	s_champion.frame.height				= 332;

	trap_Cvar_VariableStringBuffer( "champion", current, sizeof( current ) );

	count = bg_numChampions < MAX_MENU_CHAMPIONS ? bg_numChampions : MAX_MENU_CHAMPIONS;
	for ( i = 0 ; i < count ; i++ ) {
		menutext_s	*item = &s_champion.champions[i];

		item->generic.type		= MTYPE_PTEXT;
		item->generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
		item->generic.x			= ( i / CHAMPION_ROWS ) ? 420 : 220;
		item->generic.y			= 96 + ( i % CHAMPION_ROWS ) * CHAMPION_SPACING;
		item->generic.id		= i;
		item->generic.callback	= Champion_Event;
		item->string			= (char *)BG_String( bg_championList[i].nameString );
		item->color				= Q_stricmp( current, bg_championList[i].name ) ? color_red : color_white;
		item->style				= UI_CENTER|UI_SMALLFONT;
	}

	s_champion.hint.generic.type		= MTYPE_TEXT;
	s_champion.hint.generic.x			= 320;
	s_champion.hint.generic.y			= 96 + CHAMPION_ROWS * CHAMPION_SPACING + 4;
	s_champion.hint.string				= (char *)BG_String( STR_MENU_CHAMPION_NEXT_SPAWN );
	s_champion.hint.color				= color_white;
	s_champion.hint.style				= UI_CENTER|UI_SMALLFONT;

	s_champion.back.generic.type		= MTYPE_PTEXT;
	s_champion.back.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_champion.back.generic.x			= 320;
	s_champion.back.generic.y			= 96 + CHAMPION_ROWS * CHAMPION_SPACING + 30;
	s_champion.back.generic.id			= ID_BACK;
	s_champion.back.generic.callback	= Champion_Event;
	s_champion.back.string				= (char *)BG_String( STR_MENU_BACK );
	s_champion.back.color				= color_red;
	s_champion.back.style				= UI_CENTER|UI_SMALLFONT;

	Menu_AddItem( &s_champion.menu, &s_champion.frame );
	Menu_AddItem( &s_champion.menu, &s_champion.banner );
	for ( i = 0 ; i < count ; i++ ) {
		Menu_AddItem( &s_champion.menu, &s_champion.champions[i] );
	}
	Menu_AddItem( &s_champion.menu, &s_champion.hint );
	Menu_AddItem( &s_champion.menu, &s_champion.back );
}


/*
=================
Champion_Cache
=================
*/
void Champion_Cache( void ) {
	trap_R_RegisterShaderNoMip( CHAMPION_FRAME );
}


/*
=================
UI_ChampionMenu
=================
*/
void UI_ChampionMenu( void ) {
	Champion_MenuInit();
	UI_PushMenu( &s_champion.menu );
}
