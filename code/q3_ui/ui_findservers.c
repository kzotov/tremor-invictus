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

FIND SERVERS

The engine keeps the server lists and pings them; this screen picks the
source, filters by mode, search text, empty, full, password and ping, and
shows server, map, mode, players and ping, best ping first.

=======================================================================
*/

#include "ui_local.h"

#define MAX_LISTED			256
#define VISIBLE_ROWS		12
#define ROW_HEIGHT			18
#define LIST_Y				196
#define NUM_MODES			10		// all, the six modes, free-for-all, the arcade modes

#define SRC_INTERNET		0
#define SRC_LAN				1
#define SRC_FAVORITES		2

#define ID_SOURCE			10
#define ID_MODE				11
#define ID_SEARCH			12
#define ID_HIDEEMPTY		13
#define ID_HIDEFULL			14
#define ID_HIDEPASS			15
#define ID_MAXPING			16
#define ID_REFRESH			17
#define ID_FAVORITE			18
#define ID_CONNECT			19
#define ID_BACK				20
#define ID_ROW				100

#define COL_NAME			16
#define COL_MAP				272
#define COL_MODE			376
#define COL_PLAYERS			480
#define COL_PING			560

static const char *fsModeKeys[NUM_MODES] = { "", "duel", "tdm", "ctf", "ca", "sacrifice", "roundduel", "ffa", "instagib", "unholytrinity" };
static const int fsModeNames[NUM_MODES] = {
	STR_FS_ALL, STR_MODE_DUEL, STR_MODE_TDM, STR_MODE_CTF, STR_MODE_CA, STR_MODE_SACRIFICE, STR_MODE_ROUNDDUEL, STR_MODE_FFA, STR_MODE_INSTAGIB, STR_MODE_TRINITY
};
static const int fsPingLimits[] = { 0, 50, 100, 150, 200 };
static const char *fsPingNames[] = { NULL, "50", "100", "150", "200", NULL };
static const char *fsOffOn[3];

typedef struct {
	int		index;			// in the engine's list
	int		ping;
	char	name[32];
	char	map[16];
	char	mode[16];
	int		clients, maxClients;
	char	addr[64];
} fsServer_t;

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menulist_s		source;
	menulist_s		mode;
	menufield_s		search;
	menulist_s		hideEmpty;
	menulist_s		hideFull;
	menulist_s		hidePass;
	menulist_s		maxPing;
	menuaction_s	rows[VISIBLE_ROWS];
	menutext_s		refresh;
	menutext_s		favorite;
	menutext_s		connect;
	menutext_s		back;

	const char		*sourceNames[4];
	const char		*modeNames[NUM_MODES + 1];

	fsServer_t		listed[MAX_LISTED];
	int				numListed;
	int				total;
	int				top;
	int				selected;
	qboolean		pinging;
	int				refreshTime;		// when the last refresh was asked for
} findServers_t;

static findServers_t	s_fs;

/*
=================
FS_Source
=================
*/
static qboolean FS_HasMaster( void );

static int FS_Source( void ) {
	switch ( s_fs.source.curvalue ) {
	case SRC_LAN:		return AS_LOCAL;
	case SRC_FAVORITES:	return AS_FAVORITES;
	default:			return FS_HasMaster() ? AS_GLOBAL : AS_LOCAL;		// no internet list yet: LAN
	}
}

/*
=================
FS_HasMaster
=================
*/
static qboolean FS_HasMaster( void ) {
	char	buf[64];
	int		i;

	for ( i = 1 ; i <= 5 ; i++ ) {
		trap_Cvar_VariableStringBuffer( va( "sv_master%i", i ), buf, sizeof( buf ) );
		if ( buf[0] ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
FS_Refresh

Ask for a new list and ping it
=================
*/
static void FS_Refresh( void ) {
	int		source = FS_Source();
	char	buf[64];
	int		i;

	s_fs.refreshTime = uis.realtime;
	s_fs.pinging = qtrue;
	trap_LAN_MarkServerVisible( source, -1, qtrue );
	trap_LAN_ResetPings( source );
	if ( source == AS_LOCAL ) {
		trap_Cmd_ExecuteText( EXEC_NOW, "localservers\n" );
	} else if ( source == AS_GLOBAL ) {
		// each master by number, so the requests go out now rather than after queued commands
		for ( i = 1 ; i <= 5 ; i++ ) {
			trap_Cvar_VariableStringBuffer( va( "sv_master%i", i ), buf, sizeof( buf ) );
			if ( buf[0] ) {
				trap_Cmd_ExecuteText( EXEC_NOW, va( "globalservers %i %i full empty\n", i,
					(int)trap_Cvar_VariableValue( "com_protocol" ) ) );
			}
		}
	}
}

/*
=================
FS_Matches

The filters, on one server's info
=================
*/
static qboolean FS_Matches( const char *info, int ping ) {
	const char	*search = s_fs.search.field.buffer;
	int			clients = atoi( Info_ValueForKey( info, "clients" ) );
	int			maxClients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );

	if ( ping <= 0 || !Info_ValueForKey( info, "mapname" )[0] ) {
		return qfalse;		// not answered (yet)
	}
	if ( s_fs.mode.curvalue > 0 && Q_stricmp( Info_ValueForKey( info, "mode" ), fsModeKeys[s_fs.mode.curvalue] ) ) {
		return qfalse;
	}
	if ( s_fs.hideEmpty.curvalue && !clients ) {
		return qfalse;
	}
	if ( s_fs.hideFull.curvalue && maxClients && clients >= maxClients ) {
		return qfalse;
	}
	if ( s_fs.hidePass.curvalue && atoi( Info_ValueForKey( info, "g_needpass" ) ) ) {
		return qfalse;
	}
	if ( fsPingLimits[s_fs.maxPing.curvalue] && ping > fsPingLimits[s_fs.maxPing.curvalue] ) {
		return qfalse;
	}
	if ( search[0] && !Q_stristr( Info_ValueForKey( info, "hostname" ), search )
		&& !Q_stristr( Info_ValueForKey( info, "mapname" ), search )
		&& !Q_stristr( Info_ValueForKey( info, "mode" ), search ) ) {
		return qfalse;
	}
	return qtrue;
}

/*
=================
FS_BuildList

The filtered list from the engine's, best ping first
=================
*/
static void FS_BuildList( void ) {
	char		info[MAX_STRING_CHARS];
	int			source = FS_Source();
	int			i, j, ping, m;
	static fsServer_t	swap;

	s_fs.total = trap_LAN_GetServerCount( source );
	s_fs.numListed = 0;
	for ( i = 0 ; i < s_fs.total && s_fs.numListed < MAX_LISTED ; i++ ) {
		fsServer_t	*f;

		trap_LAN_GetServerInfo( source, i, info, sizeof( info ) );
		ping = atoi( Info_ValueForKey( info, "ping" ) );
		if ( !FS_Matches( info, ping ) ) {
			continue;
		}
		f = &s_fs.listed[s_fs.numListed++];
		f->index = i;
		f->ping = ping;
		Q_strncpyz( f->name, Info_ValueForKey( info, "hostname" ), sizeof( f->name ) );
		Q_CleanStr( f->name );
		Q_strncpyz( f->map, Info_ValueForKey( info, "mapname" ), sizeof( f->map ) );
		Q_strncpyz( f->mode, Info_ValueForKey( info, "mode" ), sizeof( f->mode ) );
		for ( m = 1 ; m < NUM_MODES ; m++ ) {
			if ( !Q_stricmp( f->mode, fsModeKeys[m] ) ) {
				Q_strncpyz( f->mode, BG_String( fsModeNames[m] ), sizeof( f->mode ) );
			}
		}
		f->clients = atoi( Info_ValueForKey( info, "clients" ) );
		f->maxClients = atoi( Info_ValueForKey( info, "sv_maxclients" ) );
		Q_strncpyz( f->addr, Info_ValueForKey( info, "addr" ), sizeof( f->addr ) );
	}
	for ( i = 1 ; i < s_fs.numListed ; i++ ) {
		swap = s_fs.listed[i];
		for ( j = i ; j > 0 && s_fs.listed[j - 1].ping > swap.ping ; j-- ) {
			s_fs.listed[j] = s_fs.listed[j - 1];
		}
		s_fs.listed[j] = swap;
	}
	if ( s_fs.selected >= s_fs.numListed ) {
		s_fs.selected = s_fs.numListed - 1;
	}
	if ( s_fs.selected < 0 ) {
		s_fs.selected = 0;
	}
}

/*
=================
FS_Connect
=================
*/
static void FS_Connect( void ) {
	if ( s_fs.selected < s_fs.numListed && s_fs.listed[s_fs.selected].addr[0] ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "connect %s\n", s_fs.listed[s_fs.selected].addr ) );
	}
}

/*
=================
FS_Event
=================
*/
static void FS_Event( void *ptr, int event ) {
	int		id = ( (menucommon_s *)ptr )->id;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	if ( id >= ID_ROW && id < ID_ROW + VISIBLE_ROWS ) {
		int		i = s_fs.top + id - ID_ROW;

		if ( i < s_fs.numListed ) {
			if ( i == s_fs.selected ) {
				FS_Connect();
			}
			s_fs.selected = i;
		}
		return;
	}
	switch ( id ) {
	case ID_SOURCE:
		trap_Cvar_SetValue( "ui_fsSource", s_fs.source.curvalue );
		s_fs.favorite.string = (char *)BG_String( s_fs.source.curvalue == SRC_FAVORITES ? STR_FS_REMOVEFAV : STR_FS_ADDFAV );
		s_fs.top = s_fs.selected = 0;
		FS_Refresh();
		break;
	case ID_MODE:
		trap_Cvar_SetValue( "ui_fsMode", s_fs.mode.curvalue );
		break;
	case ID_HIDEEMPTY:
	case ID_HIDEFULL:
	case ID_HIDEPASS:
	case ID_MAXPING:
		trap_Cvar_SetValue( "ui_fsFilters", s_fs.hideEmpty.curvalue | ( s_fs.hideFull.curvalue << 1 )
			| ( s_fs.hidePass.curvalue << 2 ) | ( s_fs.maxPing.curvalue << 3 ) );
		break;
	case ID_REFRESH:
		FS_Refresh();
		break;
	case ID_FAVORITE:
		if ( s_fs.selected < s_fs.numListed ) {
			fsServer_t	*f = &s_fs.listed[s_fs.selected];

			if ( FS_Source() == AS_FAVORITES ) {
				trap_LAN_RemoveServer( AS_FAVORITES, f->addr );
			} else {
				trap_LAN_AddServer( AS_FAVORITES, f->name, f->addr );
			}
		}
		break;
	case ID_CONNECT:
		FS_Connect();
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
=================
FS_DrawRow
=================
*/
static void FS_DrawRow( void *self ) {
	menuaction_s	*a = (menuaction_s *)self;
	int				i = s_fs.top + a->generic.id - ID_ROW;
	qboolean		focus = ( a->generic.parent->cursor == a->generic.menuPosition );
	fsServer_t		*f;
	float			*color;
	int				y = a->generic.y;

	if ( i >= s_fs.numListed ) {
		return;
	}
	f = &s_fs.listed[i];
	if ( i == s_fs.selected ) {
		UI_FillRect( a->generic.left, a->generic.top, a->generic.right - a->generic.left,
			a->generic.bottom - a->generic.top, listbar_color );
	}
	color = ( focus || i == s_fs.selected ) ? color_white : text_color_normal;
	UI_DrawString( COL_NAME, y, f->name, UI_LEFT | UI_SMALLFONT, color );
	UI_DrawString( COL_MAP, y, f->map, UI_LEFT | UI_SMALLFONT, color );
	UI_DrawString( COL_MODE, y, f->mode, UI_LEFT | UI_SMALLFONT, color );
	UI_DrawString( COL_PLAYERS, y, va( "%i/%i", f->clients, f->maxClients ), UI_LEFT | UI_SMALLFONT, color );
	UI_DrawString( COL_PING, y, va( "%i", f->ping ), UI_LEFT | UI_SMALLFONT,
		f->ping < 80 ? color : f->ping < 150 ? color_yellow : color_red );
}

/*
=================
FS_Draw
=================
*/
static void FS_Draw( void ) {
	float	*h = text_color_disabled;
	const char	*status;

	// the engine pings a few at a time; the list follows it
	// (the master's answer can take a few seconds, so keep at it for a while)
	if ( s_fs.pinging ) {
		qboolean	busy;

		// servers the master lists arrive hidden; show them all, so they get pinged
		trap_LAN_MarkServerVisible( FS_Source(), -1, qtrue );
		busy = trap_LAN_UpdateVisiblePings( FS_Source() );

		s_fs.pinging = busy || uis.realtime - s_fs.refreshTime < 5000;
	}
	FS_BuildList();
	if ( s_fs.top > s_fs.selected ) {
		s_fs.top = s_fs.selected;
	}

	UI_DrawString( COL_NAME, LIST_Y - 20, BG_String( STR_FS_SERVER ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_MAP, LIST_Y - 20, BG_String( STR_FS_MAP ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_MODE, LIST_Y - 20, BG_String( STR_FS_MODE ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_PLAYERS, LIST_Y - 20, BG_String( STR_FS_PLAYERS ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_PING, LIST_Y - 20, BG_String( STR_FS_PING ), UI_LEFT | UI_SMALLFONT, h );

	Menu_Draw( &s_fs.menu );

	if ( s_fs.pinging ) {
		status = BG_String( STR_FS_SCANNING );
	} else if ( !s_fs.numListed ) {
		status = BG_String( STR_FS_NONE );
	} else {
		status = va( (char *)BG_String( STR_FS_COUNT ), s_fs.numListed, s_fs.total );
	}
	if ( s_fs.source.curvalue != SRC_LAN && s_fs.source.curvalue != SRC_FAVORITES && !FS_HasMaster() ) {
		status = va( "%s %s", BG_String( STR_FS_NOMASTER ), status );
	}
	UI_DrawString( 320, LIST_Y + VISIBLE_ROWS * ROW_HEIGHT + 6, status, UI_CENTER | UI_SMALLFONT, text_color_disabled );
}

/*
=================
FS_Key
=================
*/
static sfxHandle_t FS_Key( int key ) {
	menucommon_s	*item = Menu_ItemAtCursor( &s_fs.menu );

	if ( item && item->id >= ID_ROW && item->id < ID_ROW + VISIBLE_ROWS ) {
		switch ( key ) {
		case K_UPARROW: case K_KP_UPARROW:
			if ( s_fs.selected > 0 ) {
				s_fs.selected--;
			}
			break;
		case K_DOWNARROW: case K_KP_DOWNARROW:
			if ( s_fs.selected < s_fs.numListed - 1 ) {
				s_fs.selected++;
			}
			break;
		case K_MWHEELUP: case K_PGUP:
			s_fs.selected -= key == K_PGUP ? VISIBLE_ROWS : 3;
			if ( s_fs.selected < 0 ) {
				s_fs.selected = 0;
			}
			break;
		case K_MWHEELDOWN: case K_PGDN:
			s_fs.selected += key == K_PGDN ? VISIBLE_ROWS : 3;
			if ( s_fs.selected > s_fs.numListed - 1 ) {
				s_fs.selected = s_fs.numListed - 1;
			}
			break;
		default:
			return Menu_DefaultKey( &s_fs.menu, key );
		}
		if ( s_fs.selected < 0 ) {
			s_fs.selected = 0;
		}
		if ( s_fs.selected < s_fs.top ) {
			s_fs.top = s_fs.selected;
		}
		if ( s_fs.selected >= s_fs.top + VISIBLE_ROWS ) {
			s_fs.top = s_fs.selected - VISIBLE_ROWS + 1;
		}
		Menu_SetCursorToItem( &s_fs.menu, &s_fs.rows[s_fs.selected - s_fs.top] );
		return menu_move_sound;
	}
	return Menu_DefaultKey( &s_fs.menu, key );
}

/*
=================
FS_Spin
=================
*/
static void FS_Spin( menulist_s *s, int id, int label, const char **names, int x, int y ) {
	s->generic.type		= MTYPE_SPINCONTROL;
	s->generic.flags	= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s->generic.name		= BG_String( label );
	s->generic.id		= id;
	s->generic.callback	= FS_Event;
	s->generic.x		= x;
	s->generic.y		= y;
	s->itemnames		= names;
	Menu_AddItem( &s_fs.menu, s );
}

static void FS_Text( menutext_s *t, int id, int string, int x, int y, int style ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_PULSEIFFOCUS;
	t->generic.id		= id;
	t->generic.callback	= FS_Event;
	t->generic.x		= x;
	t->generic.y		= y;
	t->string			= (char *)BG_String( string );
	t->style			= style;
	t->color			= color_red;
	Menu_AddItem( &s_fs.menu, t );
}

/*
=================
UI_FindServersMenu
=================
*/
void UI_FindServersMenu( void ) {
	int		i, filters;

	memset( &s_fs, 0, sizeof( s_fs ) );
	s_fs.menu.fullscreen = qtrue;
	s_fs.menu.wrapAround = qtrue;
	s_fs.menu.draw = FS_Draw;
	s_fs.menu.key = FS_Key;

	s_fs.banner.generic.type	= MTYPE_BTEXT;
	s_fs.banner.generic.x		= 320;
	s_fs.banner.generic.y		= 16;
	s_fs.banner.string			= (char *)BG_String( STR_FS_TITLE );
	s_fs.banner.color			= color_white;
	s_fs.banner.style			= UI_CENTER;
	Menu_AddItem( &s_fs.menu, &s_fs.banner );

	fsOffOn[0] = BG_String( STR_SET_OFF );
	fsOffOn[1] = BG_String( STR_SET_ON );
	fsOffOn[2] = NULL;
	fsPingNames[0] = BG_String( STR_FS_ANY );
	s_fs.sourceNames[0] = BG_String( STR_FS_INTERNET );
	s_fs.sourceNames[1] = BG_String( STR_FS_LAN );
	s_fs.sourceNames[2] = BG_String( STR_FS_FAVORITES );
	s_fs.sourceNames[3] = NULL;
	for ( i = 0 ; i < NUM_MODES ; i++ ) {
		s_fs.modeNames[i] = BG_String( fsModeNames[i] );
	}
	s_fs.modeNames[NUM_MODES] = NULL;

	FS_Spin( &s_fs.source, ID_SOURCE, STR_FS_SOURCE, s_fs.sourceNames, 200, 64 );
	FS_Spin( &s_fs.mode, ID_MODE, STR_FS_MODE, s_fs.modeNames, 200, 82 );
	FS_Spin( &s_fs.maxPing, ID_MAXPING, STR_FS_MAXPING, fsPingNames, 200, 100 );
	FS_Spin( &s_fs.hideEmpty, ID_HIDEEMPTY, STR_FS_HIDEEMPTY, fsOffOn, 500, 64 );
	FS_Spin( &s_fs.hideFull, ID_HIDEFULL, STR_FS_HIDEFULL, fsOffOn, 500, 82 );
	FS_Spin( &s_fs.hidePass, ID_HIDEPASS, STR_FS_HIDEPASS, fsOffOn, 500, 100 );

	s_fs.search.generic.type	= MTYPE_FIELD;
	s_fs.search.generic.flags	= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_fs.search.generic.name	= BG_String( STR_FS_SEARCH );
	s_fs.search.generic.id		= ID_SEARCH;
	s_fs.search.generic.x		= 200;
	s_fs.search.generic.y		= 128;
	s_fs.search.field.widthInChars	= 24;
	s_fs.search.field.maxchars		= 24;
	Menu_AddItem( &s_fs.menu, &s_fs.search );

	for ( i = 0 ; i < VISIBLE_ROWS ; i++ ) {
		menuaction_s	*a = &s_fs.rows[i];

		a->generic.type			= MTYPE_ACTION;
		a->generic.flags		= QMF_NODEFAULTINIT;
		a->generic.id			= ID_ROW + i;
		a->generic.callback		= FS_Event;
		a->generic.ownerdraw	= FS_DrawRow;
		a->generic.x			= COL_NAME;
		a->generic.y			= LIST_Y + i * ROW_HEIGHT;
		a->generic.left			= 8;
		a->generic.right		= 632;
		a->generic.top			= a->generic.y - 1;
		a->generic.bottom		= a->generic.y + SMALLCHAR_HEIGHT + 1;
		Menu_AddItem( &s_fs.menu, a );
	}

	FS_Text( &s_fs.back, ID_BACK, STR_MENU_BACK, 40, 446, UI_LEFT | UI_SMALLFONT );
	FS_Text( &s_fs.refresh, ID_REFRESH, STR_FS_REFRESH, 180, 446, UI_CENTER | UI_SMALLFONT );
	FS_Text( &s_fs.favorite, ID_FAVORITE, STR_FS_ADDFAV, 340, 446, UI_CENTER | UI_SMALLFONT );
	FS_Text( &s_fs.connect, ID_CONNECT, STR_FS_CONNECT, 600, 440, UI_RIGHT );

	// the last choices
	trap_Cvar_Register( NULL, "ui_fsSource", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_fsMode", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_fsFilters", "0", CVAR_ARCHIVE );
	s_fs.source.curvalue = (int)trap_Cvar_VariableValue( "ui_fsSource" ) % 3;
	s_fs.mode.curvalue = (int)trap_Cvar_VariableValue( "ui_fsMode" ) % NUM_MODES;
	filters = (int)trap_Cvar_VariableValue( "ui_fsFilters" );
	s_fs.hideEmpty.curvalue = filters & 1;
	s_fs.hideFull.curvalue = ( filters >> 1 ) & 1;
	s_fs.hidePass.curvalue = ( filters >> 2 ) & 1;
	s_fs.maxPing.curvalue = ( filters >> 3 ) % ARRAY_LEN( fsPingLimits );
	if ( s_fs.source.curvalue == SRC_FAVORITES ) {
		s_fs.favorite.string = (char *)BG_String( STR_FS_REMOVEFAV );
	}

	UI_PushMenu( &s_fs.menu );
	FS_Refresh();
}

/*
===============================================================================

Find Servers in RmlUi (ui/findservers.rml)

The same list and filters as above, driven by cvars: ui_fsSource,
ui_fsMode, ui_fsFilters (empty 1, full 2, password 4, ping index << 3) and
ui_fsSearch. Each visible row goes out as ui_fs<n>_name, _map, _mode,
_players and _ping; ui_fsSel is the selected row on the page.
Commands: ui_fsSet <cvar> <value>, ui_fsToggle <bit>, ui_fsPing <index>,
ui_fsRefresh, ui_fsPick <row>, ui_fsScroll <+n|-n>, ui_fsConnect,
ui_fsFavorite.

===============================================================================
*/

static int	fsPublishTime;

// the menu controls the filters read, from the cvars
static void FS_LoadSettings( void ) {
	int filters;

	trap_Cvar_Register( NULL, "ui_fsSource", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_fsMode", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_fsFilters", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_fsSearch", "", CVAR_TEMP );
	s_fs.source.curvalue = (int)trap_Cvar_VariableValue( "ui_fsSource" ) % 3;
	s_fs.mode.curvalue = (int)trap_Cvar_VariableValue( "ui_fsMode" ) % NUM_MODES;
	filters = (int)trap_Cvar_VariableValue( "ui_fsFilters" );
	s_fs.hideEmpty.curvalue = filters & 1;
	s_fs.hideFull.curvalue = ( filters >> 1 ) & 1;
	s_fs.hidePass.curvalue = ( filters >> 2 ) & 1;
	s_fs.maxPing.curvalue = ( filters >> 3 ) % ARRAY_LEN( fsPingLimits );
	trap_Cvar_VariableStringBuffer( "ui_fsSearch", s_fs.search.field.buffer, sizeof( s_fs.search.field.buffer ) );
	trap_Cvar_SetValue( "ui_fsPingIndex", s_fs.maxPing.curvalue );
	trap_Cvar_SetValue( "ui_fsEmpty", s_fs.hideEmpty.curvalue );
	trap_Cvar_SetValue( "ui_fsFull", s_fs.hideFull.curvalue );
	trap_Cvar_SetValue( "ui_fsPass", s_fs.hidePass.curvalue );
}

/*
=================
UI_FindServersRml
=================
*/
void UI_FindServersRml( void ) {
	FS_LoadSettings();
	s_fs.top = s_fs.selected = 0;
	fsPublishTime = 0;
	FS_Refresh();
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/findservers.rml\n" );
}

/*
=================
UI_FindServers_Frame

Pings, the filtered list and its visible page, four times a second
=================
*/
void UI_FindServers_Frame( void ) {
	const char	*status;
	int			r, i;

	if ( s_fs.pinging ) {
		qboolean	busy;

		trap_LAN_MarkServerVisible( FS_Source(), -1, qtrue );
		busy = trap_LAN_UpdateVisiblePings( FS_Source() );
		s_fs.pinging = busy || uis.realtime - s_fs.refreshTime < 5000;
	}
	if ( uis.realtime - fsPublishTime < 250 ) {
		return;
	}
	fsPublishTime = uis.realtime;
	FS_BuildList();
	if ( s_fs.top > s_fs.selected ) {
		s_fs.top = s_fs.selected;
	}
	if ( s_fs.top > s_fs.numListed - VISIBLE_ROWS ) {
		s_fs.top = s_fs.numListed - VISIBLE_ROWS > 0 ? s_fs.numListed - VISIBLE_ROWS : 0;
	}

	for ( r = 0; r < VISIBLE_ROWS; r++ ) {
		i = s_fs.top + r;
		if ( i < s_fs.numListed ) {
			fsServer_t	*f = &s_fs.listed[i];

			trap_Cvar_Set( va( "ui_fs%i_name", r ), f->name );
			trap_Cvar_Set( va( "ui_fs%i_map", r ), f->map );
			trap_Cvar_Set( va( "ui_fs%i_mode", r ), f->mode );
			trap_Cvar_Set( va( "ui_fs%i_players", r ), va( "%i / %i", f->clients, f->maxClients ) );
			trap_Cvar_Set( va( "ui_fs%i_ping", r ), va( "%i", f->ping ) );
		} else {
			trap_Cvar_Set( va( "ui_fs%i_name", r ), "" );
		}
	}
	trap_Cvar_SetValue( "ui_fsSel", s_fs.selected - s_fs.top );

	if ( s_fs.pinging ) {
		status = BG_String( STR_FS_SCANNING );
	} else if ( !s_fs.numListed ) {
		status = BG_String( STR_FS_NONE );
	} else {
		status = va( (char *)BG_String( STR_FS_COUNT ), s_fs.numListed, s_fs.total );
	}
	if ( s_fs.source.curvalue != SRC_LAN && s_fs.source.curvalue != SRC_FAVORITES && !FS_HasMaster() ) {
		status = va( "%s %s", BG_String( STR_FS_NOMASTER ), status );
	}
	trap_Cvar_Set( "ui_fsStatus", status );
}

/*
=================
UI_FindServersCommand
=================
*/
qboolean UI_FindServersCommand( const char *cmd ) {
	int		v;

	if ( !Q_stricmp( cmd, "ui_fsSet" ) ) {
		// ui_fsSet source|mode|search <value>
		if ( !Q_stricmp( UI_Argv( 1 ), "source" ) ) {
			trap_Cvar_Set( "ui_fsSource", UI_Argv( 2 ) );
			FS_LoadSettings();
			s_fs.top = s_fs.selected = 0;
			FS_Refresh();
		} else if ( !Q_stricmp( UI_Argv( 1 ), "mode" ) ) {
			trap_Cvar_Set( "ui_fsMode", UI_Argv( 2 ) );
		} else if ( !Q_stricmp( UI_Argv( 1 ), "search" ) ) {
			trap_Cvar_Set( "ui_fsSearch", UI_Argv( 2 ) );
		}
		FS_LoadSettings();
		fsPublishTime = 0;
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_fsToggle" ) || !Q_stricmp( cmd, "ui_fsPing" ) ) {
		v = (int)trap_Cvar_VariableValue( "ui_fsFilters" );
		if ( !Q_stricmp( cmd, "ui_fsToggle" ) ) {
			v ^= atoi( UI_Argv( 1 ) ) & 7;
		} else {
			v = ( v & 7 ) | ( ( atoi( UI_Argv( 1 ) ) % ARRAY_LEN( fsPingLimits ) ) << 3 );
		}
		trap_Cvar_SetValue( "ui_fsFilters", v );
		FS_LoadSettings();
		fsPublishTime = 0;
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_fsRefresh" ) ) {
		FS_Refresh();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_fsPick" ) ) {
		v = s_fs.top + atoi( UI_Argv( 1 ) );
		if ( v < s_fs.numListed ) {
			if ( v == s_fs.selected ) {
				FS_Connect();		// a second click joins
			}
			s_fs.selected = v;
		}
		fsPublishTime = 0;
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_fsScroll" ) ) {
		s_fs.top += atoi( UI_Argv( 1 ) );
		if ( s_fs.top < 0 ) {
			s_fs.top = 0;
		}
		if ( s_fs.selected < s_fs.top ) {
			s_fs.selected = s_fs.top;
		}
		if ( s_fs.selected >= s_fs.top + VISIBLE_ROWS ) {
			s_fs.selected = s_fs.top + VISIBLE_ROWS - 1;
		}
		fsPublishTime = 0;
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_fsConnect" ) ) {
		FS_Connect();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_fsFavorite" ) ) {
		if ( s_fs.selected < s_fs.numListed ) {
			fsServer_t	*f = &s_fs.listed[s_fs.selected];

			if ( FS_Source() == AS_FAVORITES ) {
				trap_LAN_RemoveServer( AS_FAVORITES, f->addr );
			} else {
				trap_LAN_AddServer( AS_FAVORITES, f->name, f->addr );
			}
		}
		return qtrue;
	}
	return qfalse;
}
