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

DEMOS MENU

=======================================================================
*/


#include "ui_local.h"

#define MAX_DEMOS			256
#define NAMEBUFSIZE			( MAX_DEMOS * 64 )
#define VISIBLE_ROWS		16
#define ROW_HEIGHT			18
#define LIST_Y				80
#define HEADER_MAX			2048

#define DEMO_BLOCK_HEADER	-2
#define DEMO_TRAILER_MAGIC	0x58444d54

#define ID_PLAY				10
#define ID_RENAME			11
#define ID_DELETE			12
#define ID_BACK				13
#define ID_UP				14
#define ID_DOWN				15
#define ID_ROW				100		// + visible row

// column positions
#define COL_DATE			16
#define COL_MAP				156
#define COL_MODE			262
#define COL_LENGTH			360
#define COL_PLAYERS			416

typedef struct {
	char		file[64];
	char		date[20];			// "YYYY-MM-DD HH:MM", empty without a header
	char		map[16];
	int			modeString;			// STR_ id, or 0
	char		players[48];
	int			duration;			// msec, 0 unknown
	qboolean	old;				// another protocol
} demoEntry_t;

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menuaction_s	rows[VISIBLE_ROWS];
	menutext_s		up;
	menutext_s		down;
	menutext_s		play;
	menutext_s		rename;
	menutext_s		del;
	menutext_s		back;
	menufield_s		newName;

	demoEntry_t		demos[MAX_DEMOS];
	int				numDemos;
	int				top;			// first visible demo
	int				selected;
	qboolean		renaming;
	int				protocol;
} demosMenu_t;

static demosMenu_t	s_demos;
static char			demoNames[NAMEBUFSIZE];

static void Demos_Refresh( void );

/*
===============
Demos_ReadInt

Little-endian, whatever the machine
===============
*/
static int Demos_ReadInt( const unsigned char *b ) {
	return b[0] | ( b[1] << 8 ) | ( b[2] << 16 ) | ( b[3] << 24 );
}

/*
===============
Demos_ModeString
===============
*/
static int Demos_ModeString( const char *gametype, const char *variant ) {
	switch ( atoi( gametype ) ) {
	case GT_TOURNAMENT:	return strstr( variant, "roundduel" ) ? STR_MODE_ROUNDDUEL : STR_MODE_DUEL;
	case GT_TEAM:		return strstr( variant, "ca" ) ? STR_MODE_CA : STR_MODE_TDM;
	case GT_CTF:		return strstr( variant, "sacrifice" ) ? STR_MODE_SACRIFICE : STR_MODE_CTF;
	case GT_FFA:		return STR_MODE_FFA;
	default:			return 0;
	}
}

/*
===============
Demos_ReadHeader

The info block at the start and the length from the trailer at the end
===============
*/
static void Demos_ReadHeader( demoEntry_t *d ) {
	fileHandle_t	f;
	unsigned char	b[16];
	static char		info[HEADER_MAX];
	int				len, blockLen;
	const char		*players;

	len = trap_FS_FOpenFile( va( "demos/%s", d->file ), &f, FS_READ );
	if ( !f ) {
		return;
	}
	if ( len >= 8 ) {
		trap_FS_Read( b, 8, f );
		blockLen = Demos_ReadInt( b + 4 );
		if ( Demos_ReadInt( b ) == DEMO_BLOCK_HEADER && blockLen > 0 && blockLen < HEADER_MAX ) {
			trap_FS_Read( info, blockLen, f );
			info[blockLen] = 0;
			Q_strncpyz( d->date, Info_ValueForKey( info, "date" ), sizeof( d->date ) );
			Q_strncpyz( d->map, Info_ValueForKey( info, "map" ), sizeof( d->map ) );
			d->modeString = Demos_ModeString( Info_ValueForKey( info, "gametype" ), Info_ValueForKey( info, "variant" ) );

			// names only, without the champion after each slash
			players = Info_ValueForKey( info, "players" );
			{
				int		n = 0;
				qboolean	skip = qfalse;

				for ( ; *players && n < (int)sizeof( d->players ) - 1; players++ ) {
					if ( *players == '/' ) {
						skip = qtrue;
					} else if ( *players == ',' ) {
						skip = qfalse;
						if ( n < (int)sizeof( d->players ) - 3 ) {
							d->players[n++] = ',';
							d->players[n++] = ' ';
						}
					} else if ( !skip && *players != '^' ) {
						d->players[n++] = *players;
					}
				}
				d->players[n] = 0;
			}
		}
	}
	if ( len >= 24 ) {
		trap_FS_Seek( f, len - 16, FS_SEEK_SET );
		trap_FS_Read( b, 16, f );
		if ( Demos_ReadInt( b + 12 ) == DEMO_TRAILER_MAGIC ) {
			d->duration = Demos_ReadInt( b + 8 );
		}
	}
	trap_FS_FCloseFile( f );
}

/*
===============
Demos_Newer

Newest first; demos without a date go last, by name
===============
*/
static int Demos_Newer( const demoEntry_t *a, const demoEntry_t *b ) {
	if ( a->date[0] && b->date[0] ) {
		return strcmp( a->date, b->date ) > 0;
	}
	if ( a->date[0] != b->date[0] ) {
		return a->date[0] != 0;
	}
	return Q_stricmp( a->file, b->file ) < 0;
}

/*
===============
Demos_Refresh
===============
*/
static void Demos_Refresh( void ) {
	char		*name;
	int			i, j, count, len;
	const char	*ext;
	static demoEntry_t	swap;

	s_demos.numDemos = 0;
	count = trap_FS_GetFileList( "demos", "", demoNames, sizeof( demoNames ) );
	name = demoNames;
	for ( i = 0; i < count && s_demos.numDemos < MAX_DEMOS; i++, name += len + 1 ) {
		demoEntry_t	*d;

		len = strlen( name );
		ext = strstr( name, "." DEMOEXT );
		if ( !ext || len >= (int)sizeof( d->file ) ) {
			continue;
		}
		d = &s_demos.demos[s_demos.numDemos++];
		memset( d, 0, sizeof( *d ) );
		Q_strncpyz( d->file, name, sizeof( d->file ) );
		d->old = atoi( ext + strlen( "." DEMOEXT ) ) != s_demos.protocol;
		if ( !d->old ) {
			Demos_ReadHeader( d );
		}
	}

	// insertion sort, newest first
	for ( i = 1; i < s_demos.numDemos; i++ ) {
		swap = s_demos.demos[i];
		for ( j = i; j > 0 && Demos_Newer( &swap, &s_demos.demos[j - 1] ); j-- ) {
			s_demos.demos[j] = s_demos.demos[j - 1];
		}
		s_demos.demos[j] = swap;
	}

	if ( s_demos.selected >= s_demos.numDemos ) {
		s_demos.selected = s_demos.numDemos - 1;
	}
	if ( s_demos.selected < 0 ) {
		s_demos.selected = 0;
	}
	if ( s_demos.top > s_demos.selected ) {
		s_demos.top = s_demos.selected;
	}
}

/*
===============
Demos_Scroll
===============
*/
static void Demos_Select( int i ) {
	if ( i < 0 ) {
		i = 0;
	}
	if ( i >= s_demos.numDemos ) {
		i = s_demos.numDemos - 1;
	}
	if ( i < 0 ) {
		return;
	}
	s_demos.selected = i;
	if ( i < s_demos.top ) {
		s_demos.top = i;
	}
	if ( i >= s_demos.top + VISIBLE_ROWS ) {
		s_demos.top = i - VISIBLE_ROWS + 1;
	}
}

/*
===============
Demos_DeleteAction
===============
*/
static void Demos_DeleteAction( qboolean result ) {
	if ( !result || s_demos.selected >= s_demos.numDemos ) {
		return;
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo_delete %s\nui_demosRefresh\n", s_demos.demos[s_demos.selected].file ) );
}

/*
===============
Demos_StopRenaming
===============
*/
static void Demos_StopRenaming( void ) {
	s_demos.renaming = qfalse;
	s_demos.newName.generic.flags |= QMF_HIDDEN | QMF_INACTIVE;
	Menu_SetCursorToItem( &s_demos.menu, &s_demos.rows[s_demos.selected - s_demos.top] );
}

/*
===============
Demos_Event
===============
*/
static void Demos_Event( void *ptr, int event ) {
	int			id = ( (menucommon_s *)ptr )->id;
	demoEntry_t	*d;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	if ( id >= ID_ROW && id < ID_ROW + VISIBLE_ROWS ) {
		int		i = s_demos.top + id - ID_ROW;

		if ( i < s_demos.numDemos ) {
			if ( i == s_demos.selected && !s_demos.demos[i].old ) {
				UI_ForceMenuOff();
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo %s\n", s_demos.demos[i].file ) );
				return;
			}
			s_demos.selected = i;
		}
		return;
	}

	d = s_demos.numDemos ? &s_demos.demos[s_demos.selected] : NULL;
	switch ( id ) {
	case ID_PLAY:
		if ( d && !d->old ) {
			UI_ForceMenuOff();
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo %s\n", d->file ) );
		}
		break;
	case ID_RENAME:
		if ( d ) {
			char	*dot;

			s_demos.renaming = qtrue;
			s_demos.newName.generic.flags &= ~( QMF_HIDDEN | QMF_INACTIVE );
			Q_strncpyz( s_demos.newName.field.buffer, d->file, sizeof( s_demos.newName.field.buffer ) );
			dot = strstr( s_demos.newName.field.buffer, "." DEMOEXT );
			if ( dot ) {
				*dot = 0;
			}
			s_demos.newName.field.cursor = strlen( s_demos.newName.field.buffer );
			Menu_SetCursorToItem( &s_demos.menu, &s_demos.newName );
		}
		break;
	case ID_DELETE:
		if ( d ) {
			UI_ConfirmMenu( BG_String( STR_DM_DELETE_ASK ), NULL, Demos_DeleteAction );
		}
		break;
	case ID_UP:
		Demos_Select( s_demos.selected - VISIBLE_ROWS );
		break;
	case ID_DOWN:
		Demos_Select( s_demos.selected + VISIBLE_ROWS );
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
===============
Demos_DrawRow
===============
*/
static void Demos_DrawRow( void *self ) {
	menuaction_s	*a = (menuaction_s *)self;
	int				i = s_demos.top + a->generic.id - ID_ROW;
	qboolean		focus = ( a->generic.parent->cursor == a->generic.menuPosition );
	demoEntry_t		*d;
	float			*color;
	int				y = a->generic.y;
	char			text[64];

	if ( i >= s_demos.numDemos ) {
		return;
	}
	d = &s_demos.demos[i];
	if ( i == s_demos.selected ) {
		UI_FillRect( a->generic.left, a->generic.top, a->generic.right - a->generic.left,
			a->generic.bottom - a->generic.top, listbar_color );
	}
	color = d->old ? text_color_disabled : ( focus || i == s_demos.selected ) ? color_white : text_color_normal;

	if ( !d->date[0] ) {
		// no header: the file name, and why it can't play
		Q_strncpyz( text, d->file, 34 );
		UI_DrawString( COL_DATE, y, text, UI_LEFT | UI_SMALLFONT, color );
		if ( d->old ) {
			UI_DrawString( 624, y, BG_String( STR_DM_OLD ), UI_RIGHT | UI_SMALLFONT, color );
		}
		return;
	}
	UI_DrawString( COL_DATE, y, d->date, UI_LEFT | UI_SMALLFONT, color );
	Q_strncpyz( text, d->map, 13 );
	UI_DrawString( COL_MAP, y, text, UI_LEFT | UI_SMALLFONT, color );
	if ( d->modeString ) {
		Q_strncpyz( text, BG_String( d->modeString ), 12 );
		UI_DrawString( COL_MODE, y, text, UI_LEFT | UI_SMALLFONT, color );
	}
	if ( d->duration > 0 ) {
		UI_DrawString( COL_LENGTH, y, va( "%i:%02i", d->duration / 60000, ( d->duration / 1000 ) % 60 ),
			UI_LEFT | UI_SMALLFONT, color );
	}
	Q_strncpyz( text, d->players, 27 );
	UI_DrawString( COL_PLAYERS, y, text, UI_LEFT | UI_SMALLFONT, color );
}

/*
===============
Demos_Draw
===============
*/
static void Demos_Draw( void ) {
	float	*h = text_color_disabled;

	UI_DrawString( COL_DATE, LIST_Y - 20, BG_String( STR_DM_DATE ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_MAP, LIST_Y - 20, BG_String( STR_DM_MAP ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_MODE, LIST_Y - 20, BG_String( STR_DM_MODE ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_LENGTH, LIST_Y - 20, BG_String( STR_DM_LENGTH ), UI_LEFT | UI_SMALLFONT, h );
	UI_DrawString( COL_PLAYERS, LIST_Y - 20, BG_String( STR_DM_PLAYERS ), UI_LEFT | UI_SMALLFONT, h );

	Menu_Draw( &s_demos.menu );

	if ( !s_demos.numDemos ) {
		UI_DrawString( 320, LIST_Y + 40, BG_String( STR_DM_NONE ), UI_CENTER | UI_SMALLFONT, text_color_normal );
	} else {
		UI_DrawString( 624, LIST_Y + VISIBLE_ROWS * ROW_HEIGHT + 4, va( (char *)BG_String( STR_DM_COUNT ), s_demos.numDemos ),
			UI_RIGHT | UI_SMALLFONT, text_color_disabled );
	}
	if ( s_demos.renaming ) {
		UI_DrawString( 320, 412, BG_String( STR_DM_RENAME_HINT ), UI_CENTER | UI_SMALLFONT, colorWhite );
	}
}

/*
===============
Demos_Key
===============
*/
static sfxHandle_t Demos_Key( int key ) {
	menucommon_s	*item = Menu_ItemAtCursor( &s_demos.menu );

	if ( s_demos.renaming ) {
		if ( key == K_ESCAPE || key == K_MOUSE2 ) {
			Demos_StopRenaming();
			return menu_out_sound;
		}
		if ( key == K_ENTER || key == K_KP_ENTER ) {
			if ( s_demos.newName.field.buffer[0] && s_demos.selected < s_demos.numDemos ) {
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "demo_rename %s \"%s\"\nui_demosRefresh\n",
					s_demos.demos[s_demos.selected].file, s_demos.newName.field.buffer ) );
			}
			Demos_StopRenaming();
			return menu_in_sound;
		}
		return Menu_DefaultKey( &s_demos.menu, key );
	}

	if ( item && item->id >= ID_ROW && item->id < ID_ROW + VISIBLE_ROWS ) {
		switch ( key ) {
		case K_UPARROW: case K_KP_UPARROW:
			Demos_Select( s_demos.selected - 1 );
			Menu_SetCursorToItem( &s_demos.menu, &s_demos.rows[s_demos.selected - s_demos.top] );
			return menu_move_sound;
		case K_DOWNARROW: case K_KP_DOWNARROW:
			Demos_Select( s_demos.selected + 1 );
			Menu_SetCursorToItem( &s_demos.menu, &s_demos.rows[s_demos.selected - s_demos.top] );
			return menu_move_sound;
		case K_PGUP: case K_MWHEELUP:
			Demos_Select( s_demos.selected - ( key == K_PGUP ? VISIBLE_ROWS : 3 ) );
			return menu_move_sound;
		case K_PGDN: case K_MWHEELDOWN:
			Demos_Select( s_demos.selected + ( key == K_PGDN ? VISIBLE_ROWS : 3 ) );
			return menu_move_sound;
		case K_DEL:
			Demos_Event( &s_demos.del, QM_ACTIVATED );
			return menu_in_sound;
		}
	}
	return Menu_DefaultKey( &s_demos.menu, key );
}

/*
===============
Demos_InitText
===============
*/
static void Demos_InitText( menutext_s *t, int id, const char *string, int x, int y, int style ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_PULSEIFFOCUS;
	t->generic.id		= id;
	t->generic.callback	= Demos_Event;
	t->generic.x		= x;
	t->generic.y		= y;
	t->string			= (char *)string;
	t->style			= style;
	t->color			= color_red;
	Menu_AddItem( &s_demos.menu, t );
}

/*
===============
Demos_MenuInit
===============
*/
static void Demos_MenuInit( void ) {
	int		i;

	memset( &s_demos, 0, sizeof( s_demos ) );
	s_demos.menu.fullscreen = qtrue;
	s_demos.menu.wrapAround = qtrue;
	s_demos.menu.draw = Demos_Draw;
	s_demos.menu.key = Demos_Key;

	s_demos.protocol = trap_Cvar_VariableValue( "com_protocol" );
	if ( !s_demos.protocol ) {
		s_demos.protocol = trap_Cvar_VariableValue( "protocol" );
	}

	s_demos.banner.generic.type		= MTYPE_BTEXT;
	s_demos.banner.generic.x		= 320;
	s_demos.banner.generic.y		= 16;
	s_demos.banner.string			= (char *)BG_String( STR_DM_TITLE );
	s_demos.banner.color			= color_white;
	s_demos.banner.style			= UI_CENTER;
	Menu_AddItem( &s_demos.menu, &s_demos.banner );

	for ( i = 0; i < VISIBLE_ROWS; i++ ) {
		menuaction_s	*a = &s_demos.rows[i];

		a->generic.type			= MTYPE_ACTION;
		a->generic.flags		= QMF_NODEFAULTINIT;
		a->generic.id			= ID_ROW + i;
		a->generic.callback		= Demos_Event;
		a->generic.ownerdraw	= Demos_DrawRow;
		a->generic.x			= COL_DATE;
		a->generic.y			= LIST_Y + i * ROW_HEIGHT;
		a->generic.left			= 8;
		a->generic.right		= 632;
		a->generic.top			= a->generic.y - 1;
		a->generic.bottom		= a->generic.y + SMALLCHAR_HEIGHT + 1;
		Menu_AddItem( &s_demos.menu, a );
	}

	s_demos.newName.generic.type	= MTYPE_FIELD;
	s_demos.newName.generic.flags	= QMF_SMALLFONT | QMF_PULSEIFFOCUS | QMF_HIDDEN | QMF_INACTIVE;
	s_demos.newName.generic.name	= BG_String( STR_DM_NEWNAME );
	s_demos.newName.generic.x		= 240;
	s_demos.newName.generic.y		= 392;
	s_demos.newName.field.widthInChars	= 40;
	s_demos.newName.field.maxchars		= 48;
	Menu_AddItem( &s_demos.menu, &s_demos.newName );

	Demos_InitText( &s_demos.back, ID_BACK, BG_String( STR_MENU_BACK ), 40, 446, UI_LEFT | UI_SMALLFONT );
	Demos_InitText( &s_demos.del, ID_DELETE, BG_String( STR_DM_DELETE ), 250, 446, UI_CENTER | UI_SMALLFONT );
	Demos_InitText( &s_demos.rename, ID_RENAME, BG_String( STR_DM_RENAME ), 390, 446, UI_CENTER | UI_SMALLFONT );
	Demos_InitText( &s_demos.play, ID_PLAY, BG_String( STR_DM_PLAY ), 600, 440, UI_RIGHT );

	Demos_Refresh();
}

/*
=================
Demos_Cache
=================
*/
void Demos_Cache( void ) {
}

/*
===============
UI_DemosRefresh

After a rename or delete has run
===============
*/
void UI_DemosRefresh( void ) {
	if ( uis.activemenu == &s_demos.menu ) {
		Demos_Refresh();
	}
}

/*
===============
UI_DemosMenu
===============
*/
void UI_DemosMenu( void ) {
	Demos_MenuInit();
	UI_PushMenu( &s_demos.menu );
	if ( s_demos.numDemos ) {
		Menu_SetCursorToItem( &s_demos.menu, &s_demos.rows[0] );
	} else {
		Menu_SetCursorToItem( &s_demos.menu, &s_demos.back );
	}
}

/*
===============================================================================

Demos in RmlUi (ui/demos.rml)

The same list, a page at a time: ui_dm<n>_date, _map, _mode, _length and
_players per row, ui_dmSel the selected row. Commands: ui_dmPick <row>
(again to play), ui_dmPlay, ui_dmScroll <+n|-n>, ui_dmDelete (twice to
confirm), ui_dmRename (to the name typed, ui_dmNewName).

===============================================================================
*/

#define RML_DEMO_ROWS	14

static void Demos_Publish( void ) {
	int		r, i;
	char	text[64];

	for ( r = 0; r < RML_DEMO_ROWS; r++ ) {
		i = s_demos.top + r;
		if ( i >= s_demos.numDemos ) {
			trap_Cvar_Set( va( "ui_dm%i_date", r ), "" );
			continue;
		}
		{
			demoEntry_t	*d = &s_demos.demos[i];

			if ( !d->date[0] ) {
				Q_strncpyz( text, d->file, sizeof( text ) );
				trap_Cvar_Set( va( "ui_dm%i_date", r ), text );
				trap_Cvar_Set( va( "ui_dm%i_map", r ), "" );
				trap_Cvar_Set( va( "ui_dm%i_mode", r ), d->old ? BG_String( STR_DM_OLD ) : "" );
				trap_Cvar_Set( va( "ui_dm%i_length", r ), "" );
				trap_Cvar_Set( va( "ui_dm%i_players", r ), "" );
				continue;
			}
			trap_Cvar_Set( va( "ui_dm%i_date", r ), d->date );
			trap_Cvar_Set( va( "ui_dm%i_map", r ), d->map );
			trap_Cvar_Set( va( "ui_dm%i_mode", r ), d->modeString ? BG_String( d->modeString ) : "" );
			trap_Cvar_Set( va( "ui_dm%i_length", r ), d->duration > 0 ? va( "%i:%02i", d->duration / 60000, ( d->duration / 1000 ) % 60 ) : "" );
			trap_Cvar_Set( va( "ui_dm%i_players", r ), d->players );
		}
	}
	trap_Cvar_SetValue( "ui_dmSel", s_demos.selected - s_demos.top );
	trap_Cvar_SetValue( "ui_dmCount", s_demos.numDemos );
	trap_Cvar_Set( "ui_dmConfirm", "0" );
	if ( s_demos.selected < s_demos.numDemos ) {
		char	*dot;

		Q_strncpyz( text, s_demos.demos[s_demos.selected].file, sizeof( text ) );
		dot = strstr( text, "." DEMOEXT );
		if ( dot ) {
			*dot = 0;
		}
		trap_Cvar_Set( "ui_dmName", text );
	}
}

/*
===============
UI_DemosRml
===============
*/
void UI_DemosRml( void ) {
	memset( &s_demos, 0, sizeof( s_demos ) );
	s_demos.protocol = trap_Cvar_VariableValue( "com_protocol" );
	if ( !s_demos.protocol ) {
		s_demos.protocol = trap_Cvar_VariableValue( "protocol" );
	}
	Demos_Refresh();
	Demos_Publish();
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/demos.rml\n" );
}

/*
===============
UI_DemosCommand
===============
*/
qboolean UI_DemosCommand( const char *cmd ) {
	demoEntry_t	*d = s_demos.selected < s_demos.numDemos ? &s_demos.demos[s_demos.selected] : NULL;
	int			i;

	if ( !Q_stricmp( cmd, "ui_dmPick" ) ) {
		i = s_demos.top + atoi( UI_Argv( 1 ) );
		if ( i < s_demos.numDemos ) {
			if ( i == s_demos.selected && !s_demos.demos[i].old ) {
				UI_ForceMenuOff();
				trap_Cmd_ExecuteText( EXEC_APPEND, va( "rml_hide\ndemo %s\n", s_demos.demos[i].file ) );
				return qtrue;
			}
			s_demos.selected = i;
		}
		Demos_Publish();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_dmPlay" ) ) {
		if ( d && !d->old ) {
			UI_ForceMenuOff();
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rml_hide\ndemo %s\n", d->file ) );
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_dmScroll" ) ) {
		Demos_Select( s_demos.selected + atoi( UI_Argv( 1 ) ) );
		Demos_Publish();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_dmDelete" ) ) {
		if ( !d ) {
			return qtrue;
		}
		if ( !trap_Cvar_VariableValue( "ui_dmConfirm" ) ) {
			trap_Cvar_Set( "ui_dmConfirm", "1" );
			return qtrue;
		}
		trap_Cmd_ExecuteText( EXEC_NOW, va( "demo_delete %s\n", d->file ) );
		Demos_Refresh();
		Demos_Publish();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_dmRename" ) ) {
		// the new name typed into the field (ui_dmNewName)
		char	name[64];

		trap_Cvar_VariableStringBuffer( "ui_dmNewName", name, sizeof( name ) );
		if ( d && name[0] ) {
			trap_Cmd_ExecuteText( EXEC_NOW, va( "demo_rename %s \"%s\"\n", d->file, name ) );
			Demos_Refresh();
			Demos_Publish();
		}
		return qtrue;
	}
	return qfalse;
}

/*
===============================================================================

Profile (ui/profile.rml): ratings per mode from the matchmaker (mm_profile)
and the last matches from history.txt, newest first, as ui_h<n>_date,
_mode, _map, _result, _score and _demo. ui_profilePlay <n> watches a
match's recording.

===============================================================================
*/

#define PROFILE_ROWS	20

static char	profileDemos[PROFILE_ROWS][MAX_QPATH];

void UI_ProfileRml( void ) {
	static char		text[32768];
	char			*lines[256], *p, *field[6];
	fileHandle_t	f;
	int				len, n = 0, i, k, row = 0;

	text[0] = 0;
	len = trap_FS_FOpenFile( "history.txt", &f, FS_READ );
	if ( f ) {
		if ( len > (int)sizeof( text ) - 1 ) {
			// the newest lines are at the end
			trap_FS_Seek( f, len - ( sizeof( text ) - 1 ), FS_SEEK_SET );
			len = sizeof( text ) - 1;
		}
		trap_FS_Read( text, len, f );
		text[len] = 0;
		trap_FS_FCloseFile( f );
	}
	for ( p = text; *p && n < 256; ) {
		lines[n++] = p;
		p = strchr( p, '\n' );
		if ( !p ) {
			break;
		}
		*p++ = 0;
	}
	for ( i = n - 1; i >= 0 && row < PROFILE_ROWS; i-- ) {
		p = lines[i];
		for ( k = 0; k < 6; k++ ) {
			field[k] = p;
			p = p ? strchr( p, '|' ) : NULL;
			if ( p ) {
				*p++ = 0;
			}
		}
		if ( !field[0] || !field[0][0] || !field[4] ) {
			continue;
		}
		trap_Cvar_Set( va( "ui_h%i_date", row ), field[0] );
		trap_Cvar_Set( va( "ui_h%i_mode", row ), field[1] );
		trap_Cvar_Set( va( "ui_h%i_map", row ), field[2] );
		trap_Cvar_Set( va( "ui_h%i_result", row ), field[3] );
		trap_Cvar_Set( va( "ui_h%i_score", row ), field[4] );
		trap_Cvar_Set( va( "ui_h%i_demo", row ), field[5] && field[5][0] ? "1" : "0" );
		Q_strncpyz( profileDemos[row], field[5] ? field[5] : "", MAX_QPATH );
		row++;
	}
	trap_Cvar_SetValue( "ui_hCount", row );
	for ( ; row < PROFILE_ROWS; row++ ) {
		trap_Cvar_Set( va( "ui_h%i_date", row ), "" );
	}
	// EXEC_NOW runs one command line: one call each
	trap_Cmd_ExecuteText( EXEC_NOW, "mm_profile\n" );
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/profile.rml\n" );
}

qboolean UI_ProfileCommand( const char *cmd ) {
	int i;

	if ( !Q_stricmp( cmd, "ui_profile" ) ) {
		UI_ProfileRml();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_profilePlay" ) ) {
		i = atoi( UI_Argv( 1 ) );
		if ( i >= 0 && i < PROFILE_ROWS && profileDemos[i][0] ) {
			UI_ForceMenuOff();
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "rml_hide\ndemo \"%s\"\n", profileDemos[i] ) );
		}
		return qtrue;
	}
	return qfalse;
}
