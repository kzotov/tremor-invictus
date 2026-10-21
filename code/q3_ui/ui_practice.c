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

PRACTICE

One screen from top to bottom: mode, map, bots, limits, Start. The last
choices are remembered.

=======================================================================
*/

#include "ui_local.h"

#define NUM_MODES			9
#define MAX_PRACTICE_MAPS	128
#define MAP_COLUMNS			4
#define MAP_ROWS			2
#define MAPS_PER_PAGE		( MAP_COLUMNS * MAP_ROWS )
#define MAP_W				128
#define MAP_H				96
#define MAP_X				56
#define MAP_Y				72
#define MAP_STEP_X			136
#define MAP_STEP_Y			118
#define MAX_LIMITS			2

#define ID_MODE				10
#define ID_OPPONENTS		11
#define ID_TEAMMATES		12
#define ID_SKILL			13
#define ID_PREV				14
#define ID_NEXT				15
#define ID_START			16
#define ID_BACK				17
#define ID_MAP				100		// + cell

typedef struct {
	int			label;
	const char	*cvar;			// the setting the server reads
	int			defaultValue;
} modeLimit_t;

typedef struct {
	int			name;
	int			gametype;
	const char	*variant;		// setting that turns the mode on, or NULL
	qboolean	teams;
	modeLimit_t	limits[MAX_LIMITS];
} practiceMode_t;

// the six modes in the player's order, then free-for-all, each with its limits from its spec
static const practiceMode_t practiceModes[NUM_MODES] = {
	{ STR_MODE_DUEL, GT_TOURNAMENT, NULL, qfalse,
		{ { STR_PR_TIMELIMIT, "timelimit", 10 }, { STR_PR_FRAGLIMIT, "fraglimit", 0 } } },
	{ STR_MODE_TDM, GT_TEAM, NULL, qtrue,
		{ { STR_PR_TIMELIMIT, "timelimit", 10 }, { STR_PR_FRAGLIMIT, "fraglimit", 75 } } },
	{ STR_MODE_CTF, GT_CTF, NULL, qtrue,
		{ { STR_PR_HALF, "g_ctfHalfLength", 7 }, { STR_PR_CAPLIMIT, "capturelimit", 0 } } },
	{ STR_MODE_CA, GT_TEAM, "g_clanArena", qtrue,
		{ { STR_PR_ROUNDS, "g_caRounds", 7 } } },
	{ STR_MODE_SACRIFICE, GT_CTF, "g_sacrifice", qtrue,
		{ { STR_PR_ROUNDS, "g_sacRounds", 2 } } },
	{ STR_MODE_ROUNDDUEL, GT_TOURNAMENT, "g_roundDuel", qfalse,
		{ { STR_PR_ROUNDS, "g_rdRounds", 2 } } },
	{ STR_MODE_FFA, GT_FFA, NULL, qfalse,
		{ { STR_PR_TIMELIMIT, "timelimit", 10 }, { STR_PR_FRAGLIMIT, "fraglimit", 50 } } },
	{ STR_MODE_INSTAGIB, GT_FFA, "g_instagib", qfalse,
		{ { STR_PR_TIMELIMIT, "timelimit", 10 }, { STR_PR_FRAGLIMIT, "fraglimit", 50 } } },
	{ STR_MODE_TRINITY, GT_FFA, "g_unholyTrinity", qfalse,
		{ { STR_PR_TIMELIMIT, "timelimit", 10 }, { STR_PR_FRAGLIMIT, "fraglimit", 0 } } }
};

static const char *practiceCountNames[] = { "0", "1", "2", "3", "4", "5", "6", "7", NULL };
static const char *practiceSkillNames[] = { "1", "2", "3", "4", "5", NULL };

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menulist_s		mode;
	menuaction_s	maps[MAPS_PER_PAGE];
	menutext_s		prev;
	menutext_s		next;
	menulist_s		opponents;
	menulist_s		teammates;
	menulist_s		skill;
	menufield_s		limits[MAX_LIMITS];
	menutext_s		start;
	menutext_s		back;

	const char		*modeNames[NUM_MODES + 1];

	int				mapList[MAX_PRACTICE_MAPS];
	int				numMaps;
	int				page;
	int				selected;		// index into mapList
} practiceMenu_t;

static practiceMenu_t	s_practice;

/*
=================
Practice_Cvar

Remembered choices live in archived ui_practice_* settings
=================
*/
static int Practice_Cvar( const char *name, int defaultValue ) {
	char	cvar[64], value[16];

	// no va() here: callers pass names made with it
	Com_sprintf( cvar, sizeof( cvar ), "ui_practice_%s", name );
	Com_sprintf( value, sizeof( value ), "%i", defaultValue );
	trap_Cvar_Register( NULL, cvar, value, CVAR_ARCHIVE );
	trap_Cvar_VariableStringBuffer( cvar, value, sizeof( value ) );
	return atoi( value );
}

static void Practice_SetCvar( const char *name, int value ) {
	char	cvar[64];

	Com_sprintf( cvar, sizeof( cvar ), "ui_practice_%s", name );
	trap_Cvar_SetValue( cvar, value );
}

/*
=================
Practice_GametypeBits
=================
*/
static int Practice_GametypeBits( const char *string ) {
	char	*p = (char *)string;
	char	*token;
	int		bits = 0;

	while ( 1 ) {
		token = COM_ParseExt( &p, qfalse );
		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, "ffa" ) ) {
			bits |= 1 << GT_FFA;
		} else if ( !Q_stricmp( token, "tourney" ) ) {
			bits |= 1 << GT_TOURNAMENT;
		} else if ( !Q_stricmp( token, "team" ) ) {
			bits |= 1 << GT_TEAM;
		} else if ( !Q_stricmp( token, "ctf" ) ) {
			bits |= 1 << GT_CTF;
		}
	}
	return bits;
}

/*
=================
Practice_MapName
=================
*/
static const char *Practice_MapName( int i ) {
	return Info_ValueForKey( UI_GetArenaInfoByNumber( s_practice.mapList[i] ), "map" );
}

/*
=================
Practice_BuildMaps

Maps that support the mode, by name
=================
*/
static void Practice_BuildMaps( void ) {
	const practiceMode_t	*m = &practiceModes[s_practice.mode.curvalue];
	char		remembered[MAX_QPATH];
	const char	*info;
	int			i, j, num, count;

	count = UI_GetNumArenas();
	s_practice.numMaps = 0;
	for ( i = 0; i < count && s_practice.numMaps < MAX_PRACTICE_MAPS; i++ ) {
		info = UI_GetArenaInfoByNumber( i );
		if ( Practice_GametypeBits( Info_ValueForKey( info, "type" ) ) & ( 1 << m->gametype ) ) {
			s_practice.mapList[s_practice.numMaps++] = i;
		}
	}
	for ( i = 1; i < s_practice.numMaps; i++ ) {
		char	name[MAX_QPATH];

		num = s_practice.mapList[i];
		Q_strncpyz( name, Info_ValueForKey( UI_GetArenaInfoByNumber( num ), "longname" ), sizeof( name ) );
		for ( j = i; j > 0 && Q_stricmp( Info_ValueForKey( UI_GetArenaInfoByNumber( s_practice.mapList[j - 1] ), "longname" ), name ) > 0; j-- ) {
			s_practice.mapList[j] = s_practice.mapList[j - 1];
		}
		s_practice.mapList[j] = num;
	}

	// back on the map last played in this mode
	s_practice.selected = 0;
	trap_Cvar_Register( NULL, va( "ui_practice_map%i", s_practice.mode.curvalue ), "", CVAR_ARCHIVE );
	trap_Cvar_VariableStringBuffer( va( "ui_practice_map%i", s_practice.mode.curvalue ), remembered, sizeof( remembered ) );
	for ( i = 0; i < s_practice.numMaps; i++ ) {
		if ( !Q_stricmp( Practice_MapName( i ), remembered ) ) {
			s_practice.selected = i;
		}
	}
	s_practice.page = s_practice.selected / MAPS_PER_PAGE;
}

/*
=================
Practice_UpdateMode

Shows the rows the mode uses and fills its limits
=================
*/
static void Practice_UpdateMode( void ) {
	int		mode = s_practice.mode.curvalue;
	const practiceMode_t	*m = &practiceModes[mode];
	int		i;

	Practice_BuildMaps();

	if ( m->teams ) {
		s_practice.teammates.generic.flags &= ~( QMF_HIDDEN | QMF_INACTIVE );
		s_practice.opponents.generic.flags &= ~QMF_GRAYED;
		s_practice.opponents.curvalue = Practice_Cvar( "opponents", 4 );
		s_practice.teammates.curvalue = Practice_Cvar( "teammates", 3 );
	} else if ( m->gametype == GT_FFA ) {
		s_practice.teammates.generic.flags |= QMF_HIDDEN | QMF_INACTIVE;
		s_practice.opponents.generic.flags &= ~QMF_GRAYED;
		s_practice.opponents.curvalue = Practice_Cvar( "ffaOpponents", 5 );
	} else {
		s_practice.teammates.generic.flags |= QMF_HIDDEN | QMF_INACTIVE;
		s_practice.opponents.generic.flags |= QMF_GRAYED;
		s_practice.opponents.curvalue = 1;
	}

	for ( i = 0; i < MAX_LIMITS; i++ ) {
		menufield_s	*f = &s_practice.limits[i];

		if ( !m->limits[i].cvar ) {
			f->generic.flags |= QMF_HIDDEN | QMF_INACTIVE;
			continue;
		}
		f->generic.flags &= ~( QMF_HIDDEN | QMF_INACTIVE );
		f->generic.name = BG_String( m->limits[i].label );
		Com_sprintf( f->field.buffer, sizeof( f->field.buffer ), "%i",
			Practice_Cvar( va( "limit%i_%i", mode, i ), m->limits[i].defaultValue ) );
		f->field.cursor = strlen( f->field.buffer );
	}
}

/*
=================
Practice_RandomBot
=================
*/
static const char *Practice_RandomBot( void ) {
	int		n = UI_GetNumBots();

	if ( n <= 0 ) {
		return "sarge";
	}
	return Info_ValueForKey( UI_GetBotInfoByNumber( rand() % n ), "name" );
}

/*
=================
Practice_Start
=================
*/
static void Practice_Start( void ) {
	int		mode = s_practice.mode.curvalue;
	const practiceMode_t	*m = &practiceModes[mode];
	int		i, skill, opponents, teammates, value;
	char	map[MAX_QPATH];

	if ( !s_practice.numMaps ) {
		return;
	}

	// remember everything
	Practice_SetCvar( "mode", mode );
	Q_strncpyz( map, Practice_MapName( s_practice.selected ), sizeof( map ) );
	trap_Cvar_Set( va( "ui_practice_map%i", mode ), map );
	skill = s_practice.skill.curvalue + 1;
	Practice_SetCvar( "skill", skill );
	opponents = s_practice.opponents.curvalue;
	teammates = m->teams ? s_practice.teammates.curvalue : 0;
	if ( m->teams ) {
		Practice_SetCvar( "opponents", opponents );
		Practice_SetCvar( "teammates", teammates );
	} else if ( m->gametype == GT_FFA ) {
		Practice_SetCvar( "ffaOpponents", opponents );
	}

	// the mode
	trap_Cvar_SetValue( "g_gametype", m->gametype );
	trap_Cvar_Set( "g_clanArena", "0" );
	trap_Cvar_Set( "g_sacrifice", "0" );
	trap_Cvar_Set( "g_roundDuel", "0" );
	trap_Cvar_Set( "g_instagib", "0" );
	trap_Cvar_Set( "g_unholyTrinity", "0" );
	if ( m->variant ) {
		trap_Cvar_Set( m->variant, "1" );
	}
	trap_Cvar_SetValue( "ui_mode", mode );

	// limits: the mode's own, the others off
	trap_Cvar_Set( "timelimit", "0" );
	trap_Cvar_Set( "fraglimit", "0" );
	trap_Cvar_Set( "capturelimit", "0" );
	for ( i = 0; i < MAX_LIMITS; i++ ) {
		if ( !m->limits[i].cvar ) {
			continue;
		}
		value = atoi( s_practice.limits[i].field.buffer );
		if ( value < 0 ) {
			value = 0;
		}
		Practice_SetCvar( va( "limit%i_%i", mode, i ), value );
		trap_Cvar_SetValue( m->limits[i].cvar, value );
	}

	// a local game only
	trap_Cvar_SetValue( "sv_maxclients", 1 + opponents + teammates );
	trap_Cvar_Set( "dedicated", "0" );
	trap_Cvar_Set( "sv_public", "0" );
	trap_Cvar_Set( "g_friendlyfire", "0" );
	if ( m->teams ) {
		trap_Cvar_Set( "g_localTeamPref", "red" );
	}

	trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", map ) );
	trap_Cmd_ExecuteText( EXEC_APPEND, "wait 3\n" );
	for ( i = 0; i < opponents; i++ ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "addbot %s %i%s\n", (char *)Practice_RandomBot(), skill, m->teams ? " blue" : "" ) );
	}
	for ( i = 0; i < teammates; i++ ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, va( "addbot %s %i red\n", (char *)Practice_RandomBot(), skill ) );
	}
	if ( m->teams ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, "wait 5; team red\n" );
	}
}

/*
=================
Practice_Event
=================
*/
static void Practice_Event( void *ptr, int event ) {
	int		id = ( (menucommon_s *)ptr )->id;
	int		pages;

	if ( event != QM_ACTIVATED ) {
		return;
	}
	pages = ( s_practice.numMaps + MAPS_PER_PAGE - 1 ) / MAPS_PER_PAGE;

	if ( id >= ID_MAP && id < ID_MAP + MAPS_PER_PAGE ) {
		int		i = s_practice.page * MAPS_PER_PAGE + id - ID_MAP;

		if ( i < s_practice.numMaps ) {
			s_practice.selected = i;
		}
		return;
	}
	switch ( id ) {
	case ID_MODE:
		Practice_UpdateMode();
		break;
	case ID_PREV:
		if ( s_practice.page > 0 ) {
			s_practice.page--;
		}
		break;
	case ID_NEXT:
		if ( s_practice.page < pages - 1 ) {
			s_practice.page++;
		}
		break;
	case ID_START:
		Practice_Start();
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
=================
Practice_DrawMap
=================
*/
static void Practice_DrawMap( void *self ) {
	menuaction_s	*a = (menuaction_s *)self;
	int				i = s_practice.page * MAPS_PER_PAGE + a->generic.id - ID_MAP;
	qboolean		focus = ( a->generic.parent->cursor == a->generic.menuPosition );
	int				x = a->generic.left, y = a->generic.top;
	char			name[MAX_QPATH];
	qhandle_t		shot;

	if ( i >= s_practice.numMaps ) {
		return;
	}
	Q_strncpyz( name, Practice_MapName( i ), sizeof( name ) );
	shot = trap_R_RegisterShaderNoMip( va( "levelshots/%s", name ) );
	if ( shot ) {
		UI_DrawHandlePic( x, y, MAP_W, MAP_H, shot );
	} else {
		UI_FillRect( x, y, MAP_W, MAP_H, colorBlack );
	}
	if ( i == s_practice.selected ) {
		UI_DrawRect( x - 2, y - 2, MAP_W + 4, MAP_H + 4, color_white );
		UI_DrawRect( x - 1, y - 1, MAP_W + 2, MAP_H + 2, color_white );
	} else if ( focus ) {
		UI_DrawRect( x - 1, y - 1, MAP_W + 2, MAP_H + 2, color_yellow );
	}
	Q_strupr( name );
	UI_DrawString( x + MAP_W / 2, y + MAP_H + 2, name, UI_CENTER | UI_SMALLFONT,
		i == s_practice.selected ? color_white : text_color_normal );
}

/*
=================
Practice_Draw
=================
*/
static void Practice_Draw( void ) {
	int		pages = ( s_practice.numMaps + MAPS_PER_PAGE - 1 ) / MAPS_PER_PAGE;

	Menu_Draw( &s_practice.menu );

	if ( !s_practice.numMaps ) {
		UI_DrawString( 320, MAP_Y + MAP_STEP_Y - 10, BG_String( STR_PR_NOMAPS ), UI_CENTER | UI_SMALLFONT, color_yellow );
	} else if ( pages > 1 ) {
		UI_DrawString( 320, MAP_Y + MAP_STEP_Y * MAP_ROWS - 2, va( (char *)BG_String( STR_PR_PAGE ), s_practice.page + 1, pages ),
			UI_CENTER | UI_SMALLFONT, text_color_normal );
	}
}

/*
=================
Practice_InitSpin
=================
*/
static void Practice_InitSpin( menulist_s *s, int id, int label, const char **names, int y ) {
	s->generic.type		= MTYPE_SPINCONTROL;
	s->generic.flags	= QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s->generic.name		= BG_String( label );
	s->generic.id		= id;
	s->generic.callback	= Practice_Event;
	s->generic.x		= 320;
	s->generic.y		= y;
	s->itemnames		= names;
	Menu_AddItem( &s_practice.menu, s );
}

/*
=================
Practice_InitText
=================
*/
static void Practice_InitText( menutext_s *t, int id, const char *string, int x, int y, int style ) {
	t->generic.type		= MTYPE_PTEXT;
	t->generic.flags	= QMF_PULSEIFFOCUS;
	t->generic.id		= id;
	t->generic.callback	= Practice_Event;
	t->generic.x		= x;
	t->generic.y		= y;
	t->string			= (char *)string;
	t->style			= style;
	t->color			= color_red;
	Menu_AddItem( &s_practice.menu, t );
}

/*
=================
UI_PracticeStart

Starts with the remembered choices, as if Start were pressed
=================
*/
void UI_PracticeStart( void ) {
	Practice_Start();
}

/*
=================
UI_PracticeMenu
=================
*/
void UI_PracticeMenu( void ) {
	int		i, y;

	memset( &s_practice, 0, sizeof( s_practice ) );
	s_practice.menu.wrapAround = qtrue;
	s_practice.menu.fullscreen = qtrue;
	s_practice.menu.draw = Practice_Draw;

	s_practice.banner.generic.type	= MTYPE_BTEXT;
	s_practice.banner.generic.x		= 320;
	s_practice.banner.generic.y		= 12;
	s_practice.banner.string		= (char *)BG_String( STR_MENU_PRACTICE );
	s_practice.banner.color			= color_white;
	s_practice.banner.style			= UI_CENTER;
	Menu_AddItem( &s_practice.menu, &s_practice.banner );

	for ( i = 0; i < NUM_MODES; i++ ) {
		s_practice.modeNames[i] = BG_String( practiceModes[i].name );
	}
	s_practice.modeNames[NUM_MODES] = NULL;
	Practice_InitSpin( &s_practice.mode, ID_MODE, STR_PR_MODE, s_practice.modeNames, 50 );
	s_practice.mode.curvalue = Practice_Cvar( "mode", 0 );
	if ( s_practice.mode.curvalue < 0 || s_practice.mode.curvalue >= NUM_MODES ) {
		s_practice.mode.curvalue = 0;
	}

	for ( i = 0; i < MAPS_PER_PAGE; i++ ) {
		menuaction_s	*a = &s_practice.maps[i];

		a->generic.type			= MTYPE_ACTION;
		a->generic.flags		= QMF_NODEFAULTINIT;
		a->generic.id			= ID_MAP + i;
		a->generic.callback		= Practice_Event;
		a->generic.ownerdraw	= Practice_DrawMap;
		a->generic.left			= MAP_X + ( i % MAP_COLUMNS ) * MAP_STEP_X;
		a->generic.top			= MAP_Y + ( i / MAP_COLUMNS ) * MAP_STEP_Y;
		a->generic.right		= a->generic.left + MAP_W;
		a->generic.bottom		= a->generic.top + MAP_H;
		a->generic.x			= a->generic.left;
		a->generic.y			= a->generic.top;
		Menu_AddItem( &s_practice.menu, a );
	}
	y = MAP_Y + MAP_STEP_Y * MAP_ROWS - 4;
	Practice_InitText( &s_practice.prev, ID_PREV, "<", MAP_X, y, UI_LEFT | UI_SMALLFONT );
	Practice_InitText( &s_practice.next, ID_NEXT, ">", MAP_X + MAP_STEP_X * MAP_COLUMNS - 8, y, UI_RIGHT | UI_SMALLFONT );

	y += 22;
	Practice_InitSpin( &s_practice.opponents, ID_OPPONENTS, STR_PR_OPPONENTS, practiceCountNames, y );
	y += 18;
	Practice_InitSpin( &s_practice.teammates, ID_TEAMMATES, STR_PR_TEAMMATES, practiceCountNames, y );
	y += 18;
	Practice_InitSpin( &s_practice.skill, ID_SKILL, STR_PR_SKILL, practiceSkillNames, y );
	s_practice.skill.curvalue = Practice_Cvar( "skill", 3 ) - 1;
	if ( s_practice.skill.curvalue < 0 || s_practice.skill.curvalue > 4 ) {
		s_practice.skill.curvalue = 2;
	}
	for ( i = 0; i < MAX_LIMITS; i++ ) {
		menufield_s	*f = &s_practice.limits[i];

		y += 18;
		f->generic.type			= MTYPE_FIELD;
		f->generic.flags		= QMF_NUMBERSONLY | QMF_PULSEIFFOCUS | QMF_SMALLFONT;
		f->generic.name			= BG_String( STR_PR_TIMELIMIT );	// replaced per mode
		f->generic.x			= 320;
		f->generic.y			= y;
		f->field.widthInChars	= 4;
		f->field.maxchars		= 3;
		Menu_AddItem( &s_practice.menu, f );
	}

	Practice_InitText( &s_practice.back, ID_BACK, BG_String( STR_MENU_BACK ), 40, 446, UI_LEFT | UI_SMALLFONT );
	Practice_InitText( &s_practice.start, ID_START, BG_String( STR_PR_START ), 600, 440, UI_RIGHT );

	Practice_UpdateMode();
	UI_PushMenu( &s_practice.menu );
	Menu_SetCursorToItem( &s_practice.menu, &s_practice.start );
}
