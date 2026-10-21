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

MAIN MENU

=======================================================================
*/


#include "ui_local.h"


#define ID_PLAY					10
#define ID_CHAMPIONS			11
#define ID_COLORS				12
#define ID_SETTINGS				13
#define ID_DEMOS				14
#define ID_EXIT					15
#define ID_NAME					16

#define MAIN_MENU_X					40
#define MAIN_MENU_TOP				130
#define MAIN_MENU_VERTICAL_SPACING	34
#define MAIN_NAME_LENGTH			20


typedef struct {
	menuframework_s	menu;

	menutext_s		play;
	menutext_s		champions;
	menutext_s		colors;
	menutext_s		settings;
	menutext_s		demos;
	menutext_s		exit;
	menufield_s		name;
} mainmenu_t;


static mainmenu_t s_main;

typedef struct {
	menuframework_s menu;	
	char errorMessage[4096];
} errorMessage_t;

static errorMessage_t s_errorMessage;

/*
=================
MainMenu_ExitAction
=================
*/
static void MainMenu_ExitAction( qboolean result ) {
	if( !result ) {
		return;
	}
	// straight out, no credits screen
	trap_Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
}



/*
=================
Main_MenuEvent
=================
*/
void Main_MenuEvent (void* ptr, int event) {
	if( ((menucommon_s*)ptr)->id == ID_NAME ) {
		if ( event == QM_LOSTFOCUS ) {
			trap_Cvar_Set( "name", s_main.name.field.buffer );
		}
		return;
	}
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_PLAY:
		UI_PlayMenu();
		break;

	case ID_CHAMPIONS:
		UI_ChampionMenu();
		break;

	case ID_COLORS:
		UI_ColorsMenu();
		break;

	case ID_SETTINGS:
		UI_SetupMenu();
		break;

	case ID_DEMOS:
		UI_DemosMenu();
		break;

	case ID_EXIT:
		MainMenu_ExitAction( qtrue );
		break;
	}
}


/*
===============
MainMenu_Cache
===============
*/
void MainMenu_Cache( void ) {
}

sfxHandle_t ErrorMessage_Key(int key)
{
	trap_Cvar_Set( "com_errorMessage", "" );
	UI_MainMenu();
	return (menu_null_sound);
}

/*
===============
Main_MenuDraw
TTimo: this function is common to the main menu and errorMessage menu
===============
*/

static void Main_MenuDraw( void ) {
	if (strlen(s_errorMessage.errorMessage))
	{
		UI_DrawProportionalString_AutoWrapped( 320, 192, 600, 20, s_errorMessage.errorMessage, UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, menu_text_color );
	}
	else
	{
		// standard menu drawing
		Menu_Draw( &s_main.menu );		
	}

	UI_DrawString( 320, 458, BG_String( STR_MENU_FOOTER ), UI_CENTER|UI_SMALLFONT, text_color_disabled );
}


/*
===============
UI_MainItem

One entry of the main menu, left-aligned under the logo
===============
*/
static void UI_MainItem( menutext_s *item, int id, int stringId, int y ) {
	item->generic.type		= MTYPE_PTEXT;
	item->generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	item->generic.x			= MAIN_MENU_X;
	item->generic.y			= y;
	item->generic.id		= id;
	item->generic.callback	= Main_MenuEvent;
	item->string			= (char *)BG_String( stringId );
	item->color				= color_red;
	item->style				= UI_LEFT | UI_DROPSHADOW;
}


/*
===============
UI_MainMenu

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
void UI_MainMenu( void ) {
	int		y;

	trap_Cvar_Set( "sv_killserver", "1" );

	memset( &s_main, 0 ,sizeof(mainmenu_t) );
	memset( &s_errorMessage, 0 ,sizeof(errorMessage_t) );

	// com_errorMessage would need that too
	MainMenu_Cache();
	
	trap_Cvar_VariableStringBuffer( "com_errorMessage", s_errorMessage.errorMessage, sizeof(s_errorMessage.errorMessage) );
	if (strlen(s_errorMessage.errorMessage))
	{	
		s_errorMessage.menu.draw = Main_MenuDraw;
		s_errorMessage.menu.key = ErrorMessage_Key;
		s_errorMessage.menu.fullscreen = qtrue;
		s_errorMessage.menu.wrapAround = qtrue;
		s_errorMessage.menu.showlogo = qtrue;		

		trap_Key_SetCatcher( KEYCATCH_UI );
		uis.menusp = 0;
		UI_PushMenu ( &s_errorMessage.menu );
		
		return;
	}

	s_main.menu.draw = Main_MenuDraw;
	s_main.menu.fullscreen = qtrue;
	s_main.menu.wrapAround = qtrue;
	s_main.menu.showlogo = qtrue;

	y = MAIN_MENU_TOP;
	UI_MainItem( &s_main.play, ID_PLAY, STR_MENU_PLAY, y );
	y += MAIN_MENU_VERTICAL_SPACING;
	UI_MainItem( &s_main.champions, ID_CHAMPIONS, STR_MENU_CHAMPIONS, y );
	y += MAIN_MENU_VERTICAL_SPACING;
	UI_MainItem( &s_main.colors, ID_COLORS, STR_MENU_PLAYER_COLORS, y );
	y += MAIN_MENU_VERTICAL_SPACING;
	UI_MainItem( &s_main.settings, ID_SETTINGS, STR_MENU_SETTINGS, y );
	y += MAIN_MENU_VERTICAL_SPACING;
	UI_MainItem( &s_main.demos, ID_DEMOS, STR_MENU_DEMOS, y );
	y += MAIN_MENU_VERTICAL_SPACING;
	UI_MainItem( &s_main.exit, ID_EXIT, STR_MENU_QUIT, y );

	// the player's name, under the buttons
	y += MAIN_MENU_VERTICAL_SPACING + 16;
	s_main.name.generic.type			= MTYPE_FIELD;
	s_main.name.generic.name			= (char *)BG_String( STR_MENU_NAME );
	s_main.name.generic.flags			= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_main.name.generic.x				= MAIN_MENU_X + 56;
	s_main.name.generic.y				= y;
	s_main.name.generic.id				= ID_NAME;
	s_main.name.generic.callback		= Main_MenuEvent;
	s_main.name.field.widthInChars		= MAIN_NAME_LENGTH;
	s_main.name.field.maxchars			= MAIN_NAME_LENGTH;

	Menu_AddItem( &s_main.menu,	&s_main.play );
	Menu_AddItem( &s_main.menu,	&s_main.champions );
	Menu_AddItem( &s_main.menu,	&s_main.colors );
	Menu_AddItem( &s_main.menu,	&s_main.settings );
	Menu_AddItem( &s_main.menu,	&s_main.demos );
	Menu_AddItem( &s_main.menu,	&s_main.exit );
	Menu_AddItem( &s_main.menu,	&s_main.name );
	// after adding, which clears the field
	trap_Cvar_VariableStringBuffer( "name", s_main.name.field.buffer, sizeof( s_main.name.field.buffer ) );

	trap_Key_SetCatcher( KEYCATCH_UI );
	uis.menusp = 0;
	UI_PushMenu ( &s_main.menu );
		
}
