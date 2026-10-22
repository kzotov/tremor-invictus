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
//   ui_csStart			start the server
//

#include "ui_local.h"

#define CS_SLOTS		12		// maps on a page
#define CS_MAX_MAPS		256

typedef struct {
	const char	*preset;		// presets/<preset>.cfg
	int			stringKey;		// unused when 0; names come from the document
	int			gametype;		// which arenas suit it
	qboolean	teams;
} csMode_t;

static const csMode_t	csModes[] = {
	{ "ffa", 0, GT_FFA, qfalse },
	{ "duel", 0, GT_TOURNAMENT, qfalse },
	{ "tdm", 0, GT_TEAM, qtrue },
	{ "tdm2v2", 0, GT_TEAM, qtrue },
	{ "ctf", 0, GT_CTF, qtrue },
	{ "ca", 0, GT_TEAM, qtrue },
	{ "sacrifice", 0, GT_TEAM, qtrue },
	{ "roundduel", 0, GT_TOURNAMENT, qfalse },
	{ "instagib", 0, GT_FFA, qfalse },
	{ "unholytrinity", 0, GT_FFA, qfalse }
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

// the page of maps into ui_csMap<n> (name) and ui_csShot<n> (levelshot)
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
			trap_Cvar_Set( va( "ui_csShot%i", i ), va( "levelshots/%s", map ) );
		} else {
			trap_Cvar_Set( va( "ui_csMap%i", i ), "" );
			trap_Cvar_Set( va( "ui_csShot%i", i ), "" );
		}
	}
	trap_Cvar_Set( "ui_csPicked", cs.picked >= cs.page * CS_SLOTS && cs.picked < ( cs.page + 1 ) * CS_SLOTS
		? va( "%i", cs.picked - cs.page * CS_SLOTS ) : "-1" );
	trap_Cvar_Set( "ui_csPageText", va( "%i / %i", cs.page + 1, ( cs.numMaps + CS_SLOTS - 1 ) / CS_SLOTS > 0 ? ( cs.numMaps + CS_SLOTS - 1 ) / CS_SLOTS : 1 ) );
	if ( cs.picked >= 0 && cs.picked < cs.numMaps ) {
		info = UI_GetArenaInfoByNumber( cs.maps[cs.picked] );
		trap_Cvar_Set( "ui_csMapName", Info_ValueForKey( info, "longname" )[0] ? Info_ValueForKey( info, "longname" ) : Info_ValueForKey( info, "map" ) );
	} else {
		trap_Cvar_Set( "ui_csMapName", "" );
	}
}

// the mode's maps: arenas that list its game type, or every arena when none do
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
	cs.page = 0;
	cs.picked = cs.numMaps ? 0 : -1;
	trap_Cvar_Set( "ui_csModeShown", csModes[mode].preset );
	CS_PublishPage();
}

/*
=================
UI_CreateServerRml
=================
*/
void UI_CreateServerRml( void ) {
	int i, mode = 0;

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
=================
*/
static void CS_Start( void ) {
	const csMode_t	*m = &csModes[cs.mode];
	const char		*info;
	char			names[64][MAX_QPATH];
	int				bots, skill, i, n, numNames = 0;

	if ( cs.picked < 0 || cs.picked >= cs.numMaps ) {
		return;
	}
	info = UI_GetArenaInfoByNumber( cs.maps[cs.picked] );
	bots = (int)trap_Cvar_VariableValue( "ui_csBots" );
	skill = (int)trap_Cvar_VariableValue( "ui_csSkill" );
	trap_Cvar_Set( "ui_csLastMode", m->preset );

	trap_Cvar_SetValue( "dedicated", Com_Clamp( 0, 2, trap_Cvar_VariableValue( "ui_csDedicated" ) ) );
	trap_Cvar_SetValue( "sv_maxclients", 16 );
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "rml_hide\nexec presets/%s.cfg\n", m->preset ) );
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", Info_ValueForKey( info, "map" ) ) );

	// bots: random characters, alternating teams
	for ( i = 0; i < UI_GetNumBots() && numNames < 64; i++ ) {
		Q_strncpyz( names[numNames++], Info_ValueForKey( UI_GetBotInfoByNumber( i ), "name" ), MAX_QPATH );
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, "wait 3\n" );
	for ( i = 0; i < bots && numNames > 0; i++ ) {
		n = rand() % numNames;
		if ( m->teams ) {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "addbot %s %i %s\n", names[n], skill, ( i & 1 ) ? "red" : "blue" ) );
		} else {
			trap_Cmd_ExecuteText( EXEC_APPEND, va( "addbot %s %i\n", names[n], skill ) );
		}
		Q_strncpyz( names[n], names[--numNames], MAX_QPATH );
	}
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
	if ( !Q_stricmp( cmd, "ui_csStart" ) ) {
		CS_Start();
		return qtrue;
	}
	return qfalse;
}
