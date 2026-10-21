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
// g_admin.c -- administration: console commands, and players with rights
//
// From the server console or rcon: put, lock, unlock, mute, unmute, ban,
// mapmode, preset, players (forceshuffle, allready, abort and the engine's kick are
// elsewhere). Players listed in the access file use them through "admin
// <command>"; moderators may put, mute, unmute and abort.

#include "g_local.h"

char	*ConcatArgs( int start );

#define ACCESS_FILE			"access.txt"
#define MAX_ACCESS			64

typedef enum {
	ACCESS_NONE,
	ACCESS_MOD,
	ACCESS_ADMIN
} accessLevel_t;

typedef struct {
	char			id[64];		// cl_guid or IP address
	accessLevel_t	level;
} accessEntry_t;

static accessEntry_t	accessList[MAX_ACCESS];
static int				numAccess;

/*
=================
G_LoadAccess

The access file: one player per line, "<guid or ip> admin|mod", # comments
=================
*/
void G_LoadAccess( void ) {
	char			buf[8192];
	char			*p, *token;
	fileHandle_t	f;
	int				len;

	numAccess = 0;
	len = trap_FS_FOpenFile( ACCESS_FILE, &f, FS_READ );
	if ( len <= 0 ) {
		return;
	}
	if ( len >= sizeof( buf ) ) {
		len = sizeof( buf ) - 1;
	}
	trap_FS_Read( buf, len, f );
	buf[len] = 0;
	trap_FS_FCloseFile( f );

	p = buf;
	while ( numAccess < MAX_ACCESS ) {
		token = COM_ParseExt( &p, qtrue );
		if ( !token[0] ) {
			break;
		}
		if ( token[0] == '#' ) {
			SkipRestOfLine( &p );
			continue;
		}
		Q_strncpyz( accessList[numAccess].id, token, sizeof( accessList[numAccess].id ) );
		token = COM_ParseExt( &p, qfalse );
		if ( !Q_stricmp( token, "admin" ) ) {
			accessList[numAccess++].level = ACCESS_ADMIN;
		} else if ( !Q_stricmp( token, "mod" ) ) {
			accessList[numAccess++].level = ACCESS_MOD;
		}
		SkipRestOfLine( &p );
	}
	G_Printf( "%i entries in %s\n", numAccess, ACCESS_FILE );
}

/*
=================
G_AccessLevel
=================
*/
static accessLevel_t G_AccessLevel( gentity_t *ent ) {
	char	userinfo[MAX_INFO_STRING], ip[64];
	const char	*guid;
	char	*colon;
	int		i;

	if ( !ent->client || ( ent->r.svFlags & SVF_BOT ) ) {
		return ACCESS_NONE;
	}
	if ( ent->client->pers.localClient ) {
		return ACCESS_ADMIN;		// the host
	}
	trap_GetUserinfo( ent - g_entities, userinfo, sizeof( userinfo ) );
	guid = Info_ValueForKey( userinfo, "cl_guid" );
	Q_strncpyz( ip, Info_ValueForKey( userinfo, "ip" ), sizeof( ip ) );
	colon = strchr( ip, ':' );
	if ( colon ) {
		*colon = 0;
	}
	for ( i = 0 ; i < numAccess ; i++ ) {
		if ( ( guid[0] && !Q_stricmp( accessList[i].id, guid ) ) || !Q_stricmp( accessList[i].id, ip ) ) {
			return accessList[i].level;
		}
	}
	return ACCESS_NONE;
}

/*
=================
G_AdminTarget

A player by slot number or name (colours ignored)
=================
*/
static gclient_t *G_AdminTarget( const char *s ) {
	char	name[MAX_NETNAME], want[MAX_NETNAME];
	int		i;

	if ( s[0] >= '0' && s[0] <= '9' ) {
		i = atoi( s );
		if ( i >= 0 && i < level.maxclients && level.clients[i].pers.connected != CON_DISCONNECTED ) {
			return &level.clients[i];
		}
		return NULL;
	}
	Q_strncpyz( want, s, sizeof( want ) );
	Q_CleanStr( want );
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		Q_strncpyz( name, level.clients[i].pers.netname, sizeof( name ) );
		Q_CleanStr( name );
		if ( !Q_stricmp( name, want ) ) {
			return &level.clients[i];
		}
	}
	return NULL;
}

/*
=================
G_TempBanned

True when an IP is under a ban that has not run out (g_tempBans holds
"ip untilSeconds" pairs)
=================
*/
qboolean G_TempBanned( const char *ipPort ) {
	char	list[MAX_CVAR_VALUE_STRING], ip[64];
	char	*p, *token, *colon;
	int		until;
	qtime_t	now;
	int		seconds = trap_RealTime( &now );

	Q_strncpyz( ip, ipPort, sizeof( ip ) );
	colon = strchr( ip, ':' );
	if ( colon ) {
		*colon = 0;
	}
	trap_Cvar_VariableStringBuffer( "g_tempBans", list, sizeof( list ) );
	p = list;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			return qfalse;
		}
		if ( !Q_stricmp( token, ip ) ) {
			until = atoi( COM_Parse( &p ) );
			if ( until > seconds ) {
				return qtrue;
			}
		} else {
			COM_Parse( &p );
		}
	}
}

/*
=================
G_AddTempBan
=================
*/
static void G_AddTempBan( const char *ipPort, int minutes ) {
	char	list[MAX_CVAR_VALUE_STRING], kept[MAX_CVAR_VALUE_STRING], ip[64], entry[96];
	char	*p, *token, *colon;
	int		until;
	qtime_t	now;
	int		seconds = trap_RealTime( &now );

	Q_strncpyz( ip, ipPort, sizeof( ip ) );
	colon = strchr( ip, ':' );
	if ( colon ) {
		*colon = 0;
	}
	// keep the bans that have not run out
	trap_Cvar_VariableStringBuffer( "g_tempBans", list, sizeof( list ) );
	kept[0] = 0;
	p = list;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		Q_strncpyz( entry, token, sizeof( entry ) );
		until = atoi( COM_Parse( &p ) );
		if ( until > seconds && Q_stricmp( entry, ip ) ) {
			Q_strcat( kept, sizeof( kept ), va( "%s %i ", entry, until ) );
		}
	}
	Q_strcat( kept, sizeof( kept ), va( "%s %i", ip, seconds + minutes * 60 ) );
	trap_Cvar_Set( "g_tempBans", kept );
}

/*
=================
G_AdminRun

One admin command with its arguments from argument 'first' on; replies go
to 'who' (-1 for the console). Returns qfalse if it is not one of ours.
=================
*/
static qboolean G_AdminRun( int first, int who ) {
	char		cmd[64], arg1[MAX_NETNAME], arg2[64];
	gclient_t	*cl;
	int			i, team;
	char		userinfo[MAX_INFO_STRING];

	trap_Argv( first, cmd, sizeof( cmd ) );
	trap_Argv( first + 1, arg1, sizeof( arg1 ) );
	trap_Argv( first + 2, arg2, sizeof( arg2 ) );

	if ( !Q_stricmp( cmd, "players" ) ) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			const char	*line;

			cl = &level.clients[i];
			if ( cl->pers.connected == CON_DISCONNECTED ) {
				continue;
			}
			line = va( "%2i %-4s %4i %4i %s%s\n", i,
				cl->sess.sessionTeam == TEAM_RED ? "red" : cl->sess.sessionTeam == TEAM_BLUE ? "blue"
				: cl->sess.sessionTeam == TEAM_SPECTATOR ? "spec" : "free",
				cl->ps.persistant[PERS_SCORE], cl->ps.ping, cl->pers.netname, cl->sess.muted ? " (muted)" : "" );
			if ( who >= 0 ) {
				trap_SendServerCommand( who, va( "print \"%s\"", line ) );
			} else {
				G_Printf( "%s", line );
			}
		}
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "put" ) ) {
		cl = G_AdminTarget( arg1 );
		if ( !cl ) {
			return qtrue;
		}
		level.forcingTeam = qtrue;
		SetTeam( &g_entities[cl - level.clients], !Q_stricmp( arg2, "spec" ) ? "spectator" : arg2 );
		level.forcingTeam = qfalse;
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "lock" ) || !Q_stricmp( cmd, "unlock" ) ) {
		team = !Q_stricmp( arg1, "red" ) ? TEAM_RED : !Q_stricmp( arg1, "blue" ) ? TEAM_BLUE : -1;
		if ( team < 0 ) {
			for ( team = TEAM_RED ; team <= TEAM_BLUE ; team++ ) {
				level.teamLocked[team] = !Q_stricmp( cmd, "lock" );
			}
		} else {
			level.teamLocked[team] = !Q_stricmp( cmd, "lock" );
		}
		trap_SendServerCommand( -1, va( "print \"%s %s\n\"", arg1[0] ? arg1 : "teams",
			!Q_stricmp( cmd, "lock" ) ? "locked" : "unlocked" ) );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "mute" ) || !Q_stricmp( cmd, "unmute" ) ) {
		cl = G_AdminTarget( arg1 );
		if ( cl ) {
			cl->sess.muted = !Q_stricmp( cmd, "mute" );
			trap_SendServerCommand( cl - level.clients, va( "print \"%s\n\"",
				BG_String( cl->sess.muted ? STR_ADMIN_MUTED : STR_ADMIN_UNMUTED ) ) );
		}
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "ban" ) ) {
		cl = G_AdminTarget( arg1 );
		if ( !cl ) {
			return qtrue;
		}
		i = cl - level.clients;
		trap_GetUserinfo( i, userinfo, sizeof( userinfo ) );
		if ( atoi( arg2 ) > 0 ) {
			G_AddTempBan( Info_ValueForKey( userinfo, "ip" ), atoi( arg2 ) );
		} else {
			trap_SendConsoleCommand( EXEC_APPEND, va( "addip %s\n", Info_ValueForKey( userinfo, "ip" ) ) );
		}
		trap_DropClient( i, BG_String( STR_ADMIN_BANNED ) );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "preset" ) ) {
		if ( !G_PresetName( arg1 ) ) {
			G_Printf( "preset: ffa, duel, tdm, tdm2v2, ctf, ca, sacrifice, roundduel, instagib, unholytrinity\n" );
			return qtrue;
		}
		trap_Cvar_VariableStringBuffer( "mapname", userinfo, sizeof( userinfo ) );
		trap_SendConsoleCommand( EXEC_APPEND, va( "exec presets/%s.cfg\nmap %s\n", arg1, userinfo ) );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "mapmode" ) ) {
		if ( G_PresetName( arg2 ) ) {
			trap_SendConsoleCommand( EXEC_APPEND, va( "exec presets/%s.cfg\nmap %s\n", arg2, arg1 ) );
			return qtrue;
		}
		trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", arg1 ) );
		return qtrue;
	}
	return qfalse;
}

/*
=================
G_PresetName

True for the name of a mode preset (presets/<name>.cfg)
=================
*/
qboolean G_PresetName( const char *name ) {
	static const char	*presets[] = { "ffa", "duel", "tdm", "tdm2v2", "ctf", "ca", "sacrifice", "roundduel",
		"instagib", "unholytrinity" };
	int		i;

	for ( i = 0 ; i < ARRAY_LEN( presets ) ; i++ ) {
		if ( !Q_stricmp( name, presets[i] ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
G_AdminConsoleCommand

The server console's side; qfalse when the command is not one of these
=================
*/
qboolean G_AdminConsoleCommand( void ) {
	return G_AdminRun( 0, -1 );
}

/*
=================
Cmd_Admin_f

"admin <command> ..." from a player in the access file
=================
*/
void Cmd_Admin_f( gentity_t *ent ) {
	static const char	*modCommands[] = { "put", "mute", "unmute", "abort", "players" };
	accessLevel_t	access = G_AccessLevel( ent );
	char			cmd[64], arg[MAX_STRING_CHARS];
	int				i, clientNum = ent - g_entities;

	trap_Argv( 1, cmd, sizeof( cmd ) );
	if ( access == ACCESS_NONE ) {
		trap_SendServerCommand( clientNum, va( "print \"%s\n\"", BG_String( STR_ADMIN_NORIGHTS ) ) );
		return;
	}
	if ( access == ACCESS_MOD ) {
		for ( i = 0 ; i < ARRAY_LEN( modCommands ) ; i++ ) {
			if ( !Q_stricmp( cmd, modCommands[i] ) ) {
				break;
			}
		}
		if ( i == ARRAY_LEN( modCommands ) ) {
			trap_SendServerCommand( clientNum, va( "print \"%s\n\"", BG_String( STR_ADMIN_NORIGHTS ) ) );
			return;
		}
	}
	if ( G_AdminRun( 1, clientNum ) ) {
		G_LogPrintf( "Admin: %i %s: %s\n", clientNum, ent->client->pers.netname, ConcatArgs( 1 ) );
		return;
	}

	// the other server commands (kick, abort, allready, forceshuffle, ...)
	// run on the server console; admins only, one command, no separators
	if ( access != ACCESS_ADMIN ) {
		return;
	}
	Q_strncpyz( arg, ConcatArgs( 1 ), sizeof( arg ) );
	for ( i = 0 ; arg[i] ; i++ ) {
		if ( arg[i] == ';' || arg[i] == '\n' || arg[i] == '\r' ) {
			return;
		}
	}
	if ( !Q_stricmp( cmd, "kick" ) || !Q_stricmp( cmd, "clientkick" ) || !Q_stricmp( cmd, "abort" )
		|| !Q_stricmp( cmd, "allready" ) || !Q_stricmp( cmd, "forceshuffle" ) || !Q_stricmp( cmd, "map_restart" )
		|| !Q_stricmp( cmd, "cointoss" ) ) {
		G_LogPrintf( "Admin: %i %s: %s\n", clientNum, ent->client->pers.netname, arg );
		trap_SendConsoleCommand( EXEC_APPEND, va( "%s\n", arg ) );
	}
}
