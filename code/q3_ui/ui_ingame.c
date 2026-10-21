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

INGAME MENU

=======================================================================
*/


#include "ui_local.h"


#define INGAME_FRAME					"menu/art/addbotframe"
//#define INGAME_FRAME					"menu/art/cut_frame"
#define INGAME_MENU_VERTICAL_SPACING	24

#define ID_TEAM					10
#define ID_ADDBOTS				11
#define ID_REMOVEBOTS			12
#define ID_SETUP				13
#define ID_SERVERINFO			14
#define ID_LEAVEARENA			15
#define ID_RESTART				16
#define ID_QUIT					17
#define ID_RESUME				18
#define ID_TEAMORDERS			19
#define ID_CHAMPION				20
#define ID_COLORS				21


typedef struct {
	menuframework_s	menu;

	menubitmap_s	frame;
	menutext_s		team;
	menutext_s		setup;
	menutext_s		server;
	menutext_s		leave;
	menutext_s		restart;
	menutext_s		champion;
	menutext_s		colors;
	menutext_s		addbots;
	menutext_s		removebots;
	menutext_s		teamorders;
	menutext_s		quit;
	menutext_s		resume;
} ingamemenu_t;

static ingamemenu_t	s_ingame;


/*
=================
InGame_RestartAction
=================
*/
static void InGame_RestartAction( qboolean result ) {
	if( !result ) {
		return;
	}

	UI_PopMenu();
	trap_Cmd_ExecuteText( EXEC_APPEND, "map_restart 0\n" );
}


/*
=================
InGame_QuitAction
=================
*/
static void InGame_QuitAction( qboolean result ) {
	if( !result ) {
		return;
	}
	// straight out, no credits screen
	trap_Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
}


/*
=================
InGame_Event
=================
*/
void InGame_Event( void *ptr, int notification ) {
	if( notification != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_TEAM:
		UI_TeamMainMenu();
		break;

	case ID_CHAMPION:
		UI_ChampionMenu();
		break;

	case ID_COLORS:
		UI_ColorsMenu();
		break;

	case ID_SETUP:
		UI_SetupMenu();
		break;

	case ID_LEAVEARENA:
		trap_Cmd_ExecuteText( EXEC_APPEND, "disconnect\n" );
		break;

	case ID_RESTART:
		UI_ConfirmMenu( "RESTART ARENA?", 0, InGame_RestartAction );
		break;

	case ID_QUIT:
		InGame_QuitAction( qtrue );
		break;

	case ID_SERVERINFO:
		UI_ServerInfoMenu();
		break;

	case ID_ADDBOTS:
		UI_AddBotsMenu();
		break;

	case ID_REMOVEBOTS:
		UI_RemoveBotsMenu();
		break;

	case ID_TEAMORDERS:
		UI_TeamOrdersMenu();
		break;

	case ID_RESUME:
		UI_PopMenu();
		break;
	}
}


/*
=================
InGame_MenuInit
=================
*/
static void InGame_Item( menutext_s *item, int id, const char *text, int y ) {
	item->generic.type		= MTYPE_PTEXT;
	item->generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	item->generic.x			= 28;
	item->generic.y			= y;
	item->generic.id		= id;
	item->generic.callback	= InGame_Event;
	item->string			= (char *)text;
	item->color				= color_red;
	item->style				= UI_LEFT|UI_SMALLFONT;
}

/*
=================
InGame_Draw

The game darkened behind a panel on the left: the logo, the map and mode,
then the menu
=================
*/
static void InGame_Draw( void ) {
	static const vec4_t	dim = { 0, 0, 0, 0.6f };
	static const vec4_t	panel = { 0.086f, 0.09f, 0.106f, 0.92f };
	static const vec4_t	hairline = { 0.165f, 0.17f, 0.19f, 1 };
	char	info[MAX_INFO_STRING];
	char	map[MAX_QPATH];
	float	left = -uis.bias / uis.xscale;		// the screen's left edge in 640x480 units

	// the whole screen, not just the 4:3 area
	UI_FillRect( left, 0, 2000, 480, dim );		// past any right edge
	UI_FillRect( left, 0, 220 - left, 480, panel );
	UI_FillRect( 220, 0, 1, 480, hairline );

	if ( uis.logoShader ) {
		UI_DrawHandlePic( 20, 16, 180, 47, uis.logoShader );
	}
	trap_GetConfigString( CS_SERVERINFO, info, sizeof( info ) );
	Q_strncpyz( map, Info_ValueForKey( info, "mapname" ), sizeof( map ) );
	Q_strupr( map );
	UI_DrawString( 28, 70, map, UI_LEFT | UI_SMALLFONT, text_color_disabled );

	Menu_Draw( &s_ingame.menu );
}

void InGame_MenuInit( void ) {
	int		y;
	uiClientState_t	cs;
	char	info[MAX_INFO_STRING];
	int		team;

	memset( &s_ingame, 0 ,sizeof(ingamemenu_t) );

	InGame_Cache();

	s_ingame.menu.wrapAround = qtrue;
	s_ingame.menu.fullscreen = qfalse;
	s_ingame.menu.draw = InGame_Draw;

	s_ingame.frame.generic.type			= MTYPE_BITMAP;
	s_ingame.frame.generic.flags		= QMF_INACTIVE;
	s_ingame.frame.generic.name			= INGAME_FRAME;
	s_ingame.frame.generic.x			= 320-233;//142;
	s_ingame.frame.generic.y			= 240-166;//118;
	s_ingame.frame.width				= 466;//359;
	s_ingame.frame.height				= 332;//256;

	// resume, champion, colours and settings first; then match actions
	y = 104;
	InGame_Item( &s_ingame.resume, ID_RESUME, BG_String( STR_MENU_RESUME ), y );
	y += INGAME_MENU_VERTICAL_SPACING;
	// in duel the champion is set for the match, so no Champion entry
	if ( trap_Cvar_VariableValue( "g_gametype" ) != GT_TOURNAMENT ) {
		InGame_Item( &s_ingame.champion, ID_CHAMPION, BG_String( STR_MENU_CHAMPION ), y );
		y += INGAME_MENU_VERTICAL_SPACING;
	}
	InGame_Item( &s_ingame.colors, ID_COLORS, BG_String( STR_MENU_PLAYER_COLORS ), y );
	y += INGAME_MENU_VERTICAL_SPACING;
	InGame_Item( &s_ingame.setup, ID_SETUP, BG_String( STR_MENU_SETTINGS ), y );
	y += INGAME_MENU_VERTICAL_SPACING;
	InGame_Item( &s_ingame.team, ID_TEAM, BG_String( STR_MENU_JOIN_TEAM ), y );

	y += INGAME_MENU_VERTICAL_SPACING;
	InGame_Item( &s_ingame.addbots, ID_ADDBOTS, BG_String( STR_MENU_ADD_BOTS ), y );
	y += INGAME_MENU_VERTICAL_SPACING;
	InGame_Item( &s_ingame.removebots, ID_REMOVEBOTS, BG_String( STR_MENU_REMOVE_BOTS ), y );
	if( !trap_Cvar_VariableValue( "sv_running" ) || !trap_Cvar_VariableValue( "bot_enable" ) ) {
		s_ingame.addbots.generic.flags |= QMF_GRAYED;
		s_ingame.removebots.generic.flags |= QMF_GRAYED;
	}

	y += INGAME_MENU_VERTICAL_SPACING;
	InGame_Item( &s_ingame.teamorders, ID_TEAMORDERS, "TEAM ORDERS", y );
	if( !(trap_Cvar_VariableValue( "g_gametype" ) >= GT_TEAM) ) {
		s_ingame.teamorders.generic.flags |= QMF_GRAYED;
	}
	else {
		trap_GetClientState( &cs );
		trap_GetConfigString( CS_PLAYERS + cs.clientNum, info, MAX_INFO_STRING );
		team = atoi( Info_ValueForKey( info, "t" ) );
		if( team == TEAM_SPECTATOR ) {
			s_ingame.teamorders.generic.flags |= QMF_GRAYED;
		}
	}

	y += INGAME_MENU_VERTICAL_SPACING * 2;
	InGame_Item( &s_ingame.leave, ID_LEAVEARENA, BG_String( STR_MENU_LEAVE ), y );
	y += INGAME_MENU_VERTICAL_SPACING;
	InGame_Item( &s_ingame.quit, ID_QUIT, BG_String( STR_MENU_QUIT ), y );

	Menu_AddItem( &s_ingame.menu, &s_ingame.resume );
	if ( trap_Cvar_VariableValue( "g_gametype" ) != GT_TOURNAMENT ) {
		Menu_AddItem( &s_ingame.menu, &s_ingame.champion );
	}
	Menu_AddItem( &s_ingame.menu, &s_ingame.colors );
	Menu_AddItem( &s_ingame.menu, &s_ingame.setup );
	Menu_AddItem( &s_ingame.menu, &s_ingame.team );
	Menu_AddItem( &s_ingame.menu, &s_ingame.addbots );
	Menu_AddItem( &s_ingame.menu, &s_ingame.removebots );
	Menu_AddItem( &s_ingame.menu, &s_ingame.teamorders );
	Menu_AddItem( &s_ingame.menu, &s_ingame.leave );
	Menu_AddItem( &s_ingame.menu, &s_ingame.quit );
}


/*
=================
InGame_Cache
=================
*/
void InGame_Cache( void ) {
	trap_R_RegisterShaderNoMip( INGAME_FRAME );
}


/*
=================
UI_InGameMenu
=================
*/
void UI_InGameMenu( void ) {
	// force as top level menu
	uis.menusp = 0;  

	// set menu cursor to a nice location
	uis.cursorx = 319;
	uis.cursory = 80;

	InGame_MenuInit();
	UI_PushMenu( &s_ingame.menu );
}
