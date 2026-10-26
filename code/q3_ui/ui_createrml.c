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
// ui_createrml.c -- Create Server as an RmlUi document (ui/createserver.rml)
//
// The document shows cvars this module fills (ui_cs*) and runs:
//   ui_csMode <preset>	pick the mode; lists its maps
//   ui_csPage <+1|-1>	the next or previous page of maps
//   ui_csPick <slot>	pick the map in a slot of the page
//   ui_csBotsAdd <+1|-1>	more or fewer bots (in ui_csBots)
//   ui_csNext			on to the champion page (its Start runs ui_csStart)
//   ui_csStart			start the server
// Practice is the same document with ui_csPractice 1: opponents and
// teammates (ui_prOpp, ui_prMates) instead of the server settings, and the
// champion the opponents play (ui_prChamp <+1|-1>, shown in ui_prChampName).
//

#include "ui_local.h"

#define CS_SLOTS		64		// names in the list (a page)
#define CS_MAX_MAPS		256

typedef struct {
	const char	*preset;		// presets/<preset>.cfg
	int			stringKey;		// unused when 0; names come from the document
	int			gametype;		// which arenas suit it
	qboolean	teams;
	int			teamSize;		// players a side (free for all: opponents at most)
} csMode_t;

static const csMode_t	csModes[] = {
	{ "ffa", 0, GT_FFA, qfalse, 7 },
	{ "duel", 0, GT_TOURNAMENT, qfalse, 1 },
	{ "tdm", 0, GT_TEAM, qtrue, 4 },
	{ "tdm2v2", 0, GT_TEAM, qtrue, 2 },
	{ "ctf", 0, GT_CTF, qtrue, 5 },
	{ "ca", 0, GT_TEAM, qtrue, 4 },
	{ "sacrifice", 0, GT_TEAM, qtrue, 4 },
	{ "roundduel", 0, GT_TOURNAMENT, qfalse, 1 },
	{ "instagib", 0, GT_FFA, qfalse, 7 },
	{ "unholytrinity", 0, GT_FFA, qfalse, 7 },
	{ "movement", 0, GT_FFA, qfalse, 0 }		// practice only: the map to yourself
};

static struct {
	int		mode;
	int		maps[CS_MAX_MAPS];		// arena numbers
	int		numMaps;
	int		page;
	int		picked;					// index into maps
} cs;

static int CS_TypeBits( const char *types ) {
	char	buffer[MAX_INFO_VALUE], *p = buffer, *token;
	int		bits = 0;

	Q_strncpyz( buffer, types, sizeof( buffer ) );
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

// the page of maps into ui_csMap<n> (names), the picked one's levelshot into ui_csShotSel
static void CS_PublishPage( void ) {
	const char	*info;
	char		map[MAX_QPATH];
	int			i, n;

	for ( i = 0; i < CS_SLOTS; i++ ) {
		n = cs.page * CS_SLOTS + i;
		if ( n < cs.numMaps ) {
			info = UI_GetArenaInfoByNumber( cs.maps[n] );
			Q_strncpyz( map, Info_ValueForKey( info, "map" ), sizeof( map ) );
			trap_Cvar_Set( va( "ui_csMap%i", i ), Info_ValueForKey( info, "longname" )[0] ? Info_ValueForKey( info, "longname" ) : map );
		} else {
			trap_Cvar_Set( va( "ui_csMap%i", i ), "" );
		}
	}
	trap_Cvar_Set( "ui_csPicked", cs.picked >= cs.page * CS_SLOTS && cs.picked < ( cs.page + 1 ) * CS_SLOTS
		? va( "%i", cs.picked - cs.page * CS_SLOTS ) : "-1" );
	trap_Cvar_Set( "ui_csPageText", va( "%i / %i", cs.page + 1, ( cs.numMaps + CS_SLOTS - 1 ) / CS_SLOTS > 0 ? ( cs.numMaps + CS_SLOTS - 1 ) / CS_SLOTS : 1 ) );
	trap_Cvar_Set( "ui_csPages", va( "%i", ( cs.numMaps + CS_SLOTS - 1 ) / CS_SLOTS ) );
	// the one preview: the selected map's levelshot
	if ( cs.picked >= 0 && cs.picked < cs.numMaps ) {
		info = UI_GetArenaInfoByNumber( cs.maps[cs.picked] );
		trap_Cvar_Set( "ui_csMapName", Info_ValueForKey( info, "longname" )[0] ? Info_ValueForKey( info, "longname" ) : Info_ValueForKey( info, "map" ) );
		trap_Cvar_Set( "ui_csShotSel", va( "levelshots/%s", Info_ValueForKey( info, "map" ) ) );
	} else {
		trap_Cvar_Set( "ui_csMapName", "" );
		trap_Cvar_Set( "ui_csShotSel", "" );
	}
}

// a map's shown name, colours removed, for sorting
static void CS_MapName( int arena, char *out, int size ) {
	const char	*info = UI_GetArenaInfoByNumber( arena );

	Q_strncpyz( out, Info_ValueForKey( info, "longname" )[0] ? Info_ValueForKey( info, "longname" )
		: Info_ValueForKey( info, "map" ), size );
	Q_CleanStr( out );
}

// the mode's maps: arenas that list its game type, or every arena when none
// do; sorted A to Z by the shown name
static void CS_SetMode( int mode ) {
	int		i, bits;

	cs.mode = mode;
	cs.numMaps = 0;
	for ( i = 0; i < UI_GetNumArenas() && cs.numMaps < CS_MAX_MAPS; i++ ) {
		bits = CS_TypeBits( Info_ValueForKey( UI_GetArenaInfoByNumber( i ), "type" ) );
		if ( bits & ( 1 << csModes[mode].gametype ) ) {
			cs.maps[cs.numMaps++] = i;
		}
	}
	if ( !cs.numMaps ) {
		for ( i = 0; i < UI_GetNumArenas() && i < CS_MAX_MAPS; i++ ) {
			cs.maps[cs.numMaps++] = i;
		}
	}
	{
		char	a[MAX_QPATH], b[MAX_QPATH];
		int		j, k, tmp;

		for ( j = 1; j < cs.numMaps; j++ ) {
			tmp = cs.maps[j];
			CS_MapName( tmp, a, sizeof( a ) );
			for ( k = j - 1; k >= 0; k-- ) {
				CS_MapName( cs.maps[k], b, sizeof( b ) );
				if ( Q_stricmp( b, a ) <= 0 ) {
					break;
				}
				cs.maps[k + 1] = cs.maps[k];
			}
			cs.maps[k + 1] = tmp;
		}
	}
	cs.page = 0;
	cs.picked = cs.numMaps ? 0 : -1;
	trap_Cvar_Set( "ui_csModeShown", csModes[mode].preset );
	CS_PublishPage();
}

// a bot that plays the champion (its model is the champion's), or any bot
static const char *CS_BotFor( int champion ) {
	int		i, n = UI_GetNumBots();

	for ( i = 0 ; champion >= 0 && i < n ; i++ ) {
		if ( BG_ChampionForModel( Info_ValueForKey( UI_GetBotInfoByNumber( i ), "model" ) ) == champion ) {
			return Info_ValueForKey( UI_GetBotInfoByNumber( i ), "name" );
		}
	}
	return n > 0 ? Info_ValueForKey( UI_GetBotInfoByNumber( rand() % n ), "name" ) : "sarge";
}

static void CS_PublishChampion( void ) {
	int c = (int)trap_Cvar_VariableValue( "ui_prChamp" );

	if ( c < 0 || c > bg_numChampions ) {
		c = 0;
	}
	trap_Cvar_Set( "ui_prChampName", c ? BG_String( bg_championList[c - 1].nameString ) : BG_String( STR_PR_RANDOM ) );
}

/*
=================
UI_CsShowMode / UI_CsPickedMap

The call-vote page's map list: the running mode's maps in the same list
(ui_csMap<n>, ui_csPick, one preview); the picked map's file name
=================
*/
void UI_CsShowMode( const char *preset ) {
	int		i;

	for ( i = 0; i < ARRAY_LEN( csModes ); i++ ) {
		if ( !Q_stricmp( preset, csModes[i].preset ) ) {
			CS_SetMode( i );
			return;
		}
	}
	CS_SetMode( 0 );
}

const char *UI_CsPickedMap( void ) {
	if ( cs.picked < 0 || cs.picked >= cs.numMaps ) {
		return "";
	}
	return Info_ValueForKey( UI_GetArenaInfoByNumber( cs.maps[cs.picked] ), "map" );
}

/*
=================
UI_PracticeRml

Practice: Create Server with bots on both sides and no server settings
=================
*/
void UI_PracticeRml( void ) {
	trap_Cvar_Register( NULL, "ui_prOpp", "3", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_prMates", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_prChamp", "0", CVAR_ARCHIVE );
	CS_PublishChampion();
	trap_Cvar_Set( "ui_csPractice", "1" );
	UI_CreateServerRml();
}

/*
=================
UI_CreateServerRml
=================
*/
void UI_CreateServerRml( void ) {
	int i, mode = 0;

	trap_Cvar_Register( NULL, "ui_csPractice", "0", CVAR_TEMP );
	trap_Cvar_Register( NULL, "ui_csBots", "3", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_csSkill", "3", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_csDedicated", "0", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "ui_csLastMode", "ffa", CVAR_ARCHIVE );
	trap_Cvar_Register( NULL, "sv_hostname", "noname", CVAR_ARCHIVE );
	for ( i = 0; i < ARRAY_LEN( csModes ); i++ ) {
		if ( !Q_stricmp( UI_Cvar_VariableString( "ui_csLastMode" ), csModes[i].preset ) ) {
			mode = i;
		}
	}
	CS_SetMode( mode );
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/createserver.rml\n" );
}

/*
=================
CS_Start

The game's commands are made ready, the players are shown on their pillars
for a few seconds (CS_LINEUP_MSEC), then the server starts
(UI_CreateServer_Frame). A dedicated server starts at once.
=================
*/
#define CS_LINEUP_MSEC	4000
#define CS_MAX_SHOWN	8

static char		csCommands[4096];
static int		csLaunchTime;
static int		csShown;

static void CS_Cmd( const char *text ) {
	Q_strcat( csCommands, sizeof( csCommands ), text );
}

// one pillar on the lineup page (the match-found page's cvars)
static void CS_ShowPlayer( const char *name, int champion, const char *team ) {
	if ( csShown >= CS_MAX_SHOWN || champion < 0 ) {
		return;
	}
	trap_Cvar_Set( va( "mm_p%i_name", csShown ), name );
	trap_Cvar_Set( va( "mm_p%i_champ", csShown ), bg_championList[champion].name );
	trap_Cvar_Set( va( "mm_p%i_champName", csShown ), BG_String( bg_championList[champion].nameString ) );
	trap_Cvar_Set( va( "mm_p%i_team", csShown ), team );
	trap_Cvar_Set( va( "mm_p%i_rating", csShown ), "" );
	csShown++;
}

static void CS_AddBot( const char *botName, int skill, const char *team ) {
	char	bot[MAX_QPATH];
	int		i;

	Q_strncpyz( bot, botName, sizeof( bot ) );		// (botName may be a buffer the lookups below reuse)
	CS_Cmd( va( "addbot %s %i%s%s\n", bot, skill, team[0] ? " " : "", team ) );
	for ( i = 0; i < UI_GetNumBots(); i++ ) {
		const char *info = UI_GetBotInfoByNumber( i );

		if ( !Q_stricmp( Info_ValueForKey( info, "name" ), bot ) ) {
			CS_ShowPlayer( bot, BG_ChampionForModel( Info_ValueForKey( info, "model" ) ), team[0] ? team : "red" );
			return;
		}
	}
}

static void CS_Start( void ) {
	const csMode_t	*m = &csModes[cs.mode];
	const char		*info;
	char			names[64][MAX_QPATH], name[64];
	int				bots, skill, i, n, numNames = 0, dedicated;

	if ( cs.picked < 0 || cs.picked >= cs.numMaps ) {
		return;
	}
	info = UI_GetArenaInfoByNumber( cs.maps[cs.picked] );
	trap_Cvar_Set( "ui_chFromCs", "0" );		// leaving the champion page for good
	csCommands[0] = 0;
	csShown = 0;
	skill = (int)trap_Cvar_VariableValue( "ui_csSkill" );
	trap_Cvar_Set( "ui_csLastMode", m->preset );

	// you first
	trap_Cvar_VariableStringBuffer( "name", name, sizeof( name ) );
	Q_CleanStr( name );
	CS_ShowPlayer( name, BG_ChampionForName( UI_Cvar_VariableString( "champion" ) ), "red" );

	// the lineup page stays up until the loading screen takes over (the
	// client hides menu pages while connecting), so the main menu never
	// shows in between; the menus close once the map is in
	CS_Cmd( va( "exec presets/%s.cfg\n", m->preset ) );
	CS_Cmd( va( "wait ; wait ; map %s\nwait 3\nui_close\n", Info_ValueForKey( info, "map" ) ) );

	if ( trap_Cvar_VariableValue( "ui_csPractice" ) ) {
		int		opp = (int)trap_Cvar_VariableValue( "ui_prOpp" ), mates = m->teams ? (int)trap_Cvar_VariableValue( "ui_prMates" ) : 0;
		int		champ = (int)trap_Cvar_VariableValue( "ui_prChamp" ) - 1;

		// no more than the mode holds: a duel is one opponent
		if ( opp > m->teamSize ) {
			opp = m->teamSize;
		}
		if ( mates > m->teamSize - 1 ) {
			mates = m->teamSize - 1;
		}
		if ( mates < 0 ) {
			mates = 0;
		}

		dedicated = 0;
		trap_Cvar_Set( "sv_public", "0" );
		trap_Cvar_SetValue( "sv_maxclients", 1 + opp + mates );
		if ( m->teams ) {
			trap_Cvar_Set( "g_localTeamPref", "red" );
		}
		for ( i = 0; i < mates; i++ ) {
			CS_AddBot( CS_BotFor( -1 ), skill, "red" );
		}
		for ( i = 0; i < opp; i++ ) {
			CS_AddBot( CS_BotFor( champ ), skill, m->teams ? "blue" : "" );
		}
		if ( m->teams ) {
			CS_Cmd( "wait 5; team red\n" );
		}
	} else {
		dedicated = (int)Com_Clamp( 0, 2, trap_Cvar_VariableValue( "ui_csDedicated" ) );
		bots = (int)trap_Cvar_VariableValue( "ui_csBots" );
		// a duel holds one bot; a team game fills both sides at most
		if ( bots > ( m->teams ? m->teamSize * 2 - 1 : m->teamSize ) ) {
			bots = m->teams ? m->teamSize * 2 - 1 : m->teamSize;
		}
		trap_Cvar_SetValue( "sv_maxclients", 16 );

		// bots: random characters, alternating teams
		for ( i = 0; i < UI_GetNumBots() && numNames < 64; i++ ) {
			Q_strncpyz( names[numNames++], Info_ValueForKey( UI_GetBotInfoByNumber( i ), "name" ), MAX_QPATH );
		}
		for ( i = 0; i < bots && numNames > 0; i++ ) {
			n = rand() % numNames;
			CS_AddBot( names[n], skill, m->teams ? ( ( i & 1 ) ? "red" : "blue" ) : "" );
			Q_strncpyz( names[n], names[--numNames], MAX_QPATH );
		}
	}
	trap_Cvar_SetValue( "dedicated", dedicated );
	trap_Cvar_SetValue( "mm_pcount", csShown );

	if ( dedicated ) {
		trap_Cmd_ExecuteText( EXEC_APPEND, csCommands );
		UI_ForceMenuOff();
		return;
	}
	// the lineup first, while the commands wait
	trap_Cvar_Set( "ui_lnMode", m->preset );
	trap_Cvar_Set( "ui_lnMap", Info_ValueForKey( info, "longname" )[0] ? Info_ValueForKey( info, "longname" ) : Info_ValueForKey( info, "map" ) );
	csLaunchTime = uis.realtime + CS_LINEUP_MSEC;
	trap_Cmd_ExecuteText( EXEC_NOW, "rml_show ui/prelineup.rml\n" );
}

/*
=================
UI_CreateServer_Frame

Starts the server once the lineup has shown
=================
*/
void UI_CreateServer_Frame( void ) {
	if ( !csLaunchTime ) {
		return;
	}
	trap_Cvar_SetValue( "ui_lnSeconds", csLaunchTime > uis.realtime ? ( csLaunchTime - uis.realtime + 999 ) / 1000 : 0 );
	if ( uis.realtime < csLaunchTime ) {
		return;
	}
	csLaunchTime = 0;
	trap_Cmd_ExecuteText( EXEC_APPEND, csCommands );
}

/*
=================
UI_CreateServerCommand
=================
*/
qboolean UI_CreateServerCommand( const char *cmd ) {
	int i, pages;

	if ( !Q_stricmp( cmd, "ui_csMode" ) ) {
		for ( i = 0; i < ARRAY_LEN( csModes ); i++ ) {
			if ( !Q_stricmp( UI_Argv( 1 ), csModes[i].preset ) ) {
				CS_SetMode( i );
			}
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_csPage" ) ) {
		pages = ( cs.numMaps + CS_SLOTS - 1 ) / CS_SLOTS;
		cs.page += atoi( UI_Argv( 1 ) );
		if ( cs.page >= pages ) {
			cs.page = pages - 1;
		}
		if ( cs.page < 0 ) {
			cs.page = 0;
		}
		CS_PublishPage();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_csPick" ) ) {
		i = cs.page * CS_SLOTS + atoi( UI_Argv( 1 ) );
		if ( i >= 0 && i < cs.numMaps ) {
			cs.picked = i;
			CS_PublishPage();
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_csBotsAdd" ) ) {
		i = (int)trap_Cvar_VariableValue( "ui_csBots" ) + atoi( UI_Argv( 1 ) );
		trap_Cvar_SetValue( "ui_csBots", Com_Clamp( 0, 15, i ) );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_prCount" ) ) {
		// ui_prCount <cvar> <+1|-1>: opponents or teammates, 0 to 7
		const char *cvar = !Q_stricmp( UI_Argv( 1 ), "mates" ) ? "ui_prMates" : "ui_prOpp";

		i = (int)trap_Cvar_VariableValue( cvar ) + atoi( UI_Argv( 2 ) );
		trap_Cvar_SetValue( cvar, Com_Clamp( !Q_stricmp( cvar, "ui_prOpp" ) ? 1 : 0, 7, i ) );
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_prChampStep" ) ) {
		i = (int)trap_Cvar_VariableValue( "ui_prChamp" ) + atoi( UI_Argv( 1 ) );
		if ( i < 0 ) {
			i = bg_numChampions;
		}
		if ( i > bg_numChampions ) {
			i = 0;
		}
		trap_Cvar_SetValue( "ui_prChamp", i );
		CS_PublishChampion();
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_csNext" ) ) {
		UI_ChampionsRmlFor( qtrue );		// the champion page, with Start
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "ui_csStart" ) ) {
		CS_Start();
		return qtrue;
	}
	return qfalse;
}
