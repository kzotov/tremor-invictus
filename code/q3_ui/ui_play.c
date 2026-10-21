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
// ui_play.c -- PLAY: practice against bots, find a server, or host one

#include "ui_local.h"

#define ID_PRACTICE		10
#define ID_FIND			11
#define ID_CREATE		12
#define ID_BACK			13

#define PLAY_X			40
#define PLAY_TOP		130
#define PLAY_SPACING	34

typedef struct {
	menuframework_s	menu;
	menutext_s		practice;
	menutext_s		find;
	menutext_s		create;
	menutext_s		back;
} playMenu_t;

static playMenu_t	s_play;

static void UI_PlayMenu_Event( void *ptr, int event ) {
	if ( event != QM_ACTIVATED ) {
		return;
	}
	switch ( ((menucommon_s *)ptr)->id ) {
	case ID_PRACTICE:
		UI_PracticeMenu();
		break;
	case ID_FIND:
		UI_ArenaServersMenu();
		break;
	case ID_CREATE:
		UI_StartServerMenu( qtrue );
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

static void UI_PlayItem( menutext_s *item, int id, const char *text, int y ) {
	item->generic.type		= MTYPE_PTEXT;
	item->generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	item->generic.x			= PLAY_X;
	item->generic.y			= y;
	item->generic.id		= id;
	item->generic.callback	= UI_PlayMenu_Event;
	item->string			= (char *)text;
	item->color				= color_red;
	item->style				= UI_LEFT | UI_DROPSHADOW;
}

/*
===============
UI_PlayMenu
===============
*/
void UI_PlayMenu( void ) {
	int		y = PLAY_TOP;

	memset( &s_play, 0, sizeof( s_play ) );
	s_play.menu.fullscreen = qtrue;
	s_play.menu.wrapAround = qtrue;
	s_play.menu.showlogo = qtrue;

	UI_PlayItem( &s_play.practice, ID_PRACTICE, BG_String( STR_MENU_PRACTICE ), y );
	y += PLAY_SPACING;
	UI_PlayItem( &s_play.find, ID_FIND, BG_String( STR_MENU_FIND_SERVERS ), y );
	y += PLAY_SPACING;
	UI_PlayItem( &s_play.create, ID_CREATE, BG_String( STR_MENU_CREATE_SERVER ), y );
	y += PLAY_SPACING * 2;
	UI_PlayItem( &s_play.back, ID_BACK, BG_String( STR_MENU_BACK ), y );

	Menu_AddItem( &s_play.menu, &s_play.practice );
	Menu_AddItem( &s_play.menu, &s_play.find );
	Menu_AddItem( &s_play.menu, &s_play.create );
	Menu_AddItem( &s_play.menu, &s_play.back );
	UI_PushMenu( &s_play.menu );
}
