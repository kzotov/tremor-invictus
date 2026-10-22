/*
===========================================================================
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
// sv_mm.c -- a game server's side of matchmaking
//
// With sv_matchmaker set, the server registers with the matchmaker, asks
// every few seconds whether a match has been reserved on it, loads the
// match's mode and map, lets in only the players holding the match's
// one-time tokens, puts them on their teams and reports the result.

#include "server.h"
#include "../qcommon/mm_common.h"

#define MM_KEY_FILE			"mmserver.key"
#define MM_REGISTER_MSEC	30000
#define MM_POLL_MSEC		2000
#define MM_MAX_PLAYERS		16

static cvar_t	*sv_matchmaker;		// the service's address; empty: off
static cvar_t	*sv_mmAddress;		// where players reach this server
static cvar_t	*sv_mmModes;		// modes this server offers

typedef struct {
	char	key[65];
	char	token[33];
	char	team[8];
	int		client;					// -1 while not connected
	qboolean	left;				// connected once and dropped
} mmSlot_t;

static struct {
	qboolean		keyLoaded;
	byte			seed[32];
	char			pub[65];
	mmRequest_t		req;
	int				nextRegister;
	int				nextPoll;
	int				nextResult;

	char			match[64];		// empty: no reservation
	char			mode[16];
	int				reservedTime;
	mmSlot_t		slots[MM_MAX_PLAYERS];
	int				numSlots;
	qboolean		resultPending;
	char			resultBody[1024];
} mm;

static const char *mmModes[] = { "duel", "tdm", "tdm2v2", "ctf", "ca", "ca2v2", "sacrifice", "roundduel" };

static const char *PoolName( const char *mode ) {
	if ( !strcmp( mode, "tdm2v2" ) ) {
		return "tdm";
	}
	if ( !strcmp( mode, "ca2v2" ) ) {
		return "ca";
	}
	return mode;
}

// "mode": ["map", ...] from mappools/<mode>.txt, or every map there is
static void AddModeMaps( char *out, int size, const char *mode ) {
	char	path[MAX_QPATH], *text, *p, *tok;
	char	**files;
	int		i, n, first = 1;

	Q_strcat( out, size, va( "\"%s\":[", mode ) );
	Com_sprintf( path, sizeof( path ), "mappools/%s.txt", PoolName( mode ) );
	if ( FS_ReadFile( path, (void **)&text ) > 0 ) {
		p = text;
		while ( 1 ) {
			tok = COM_Parse( &p );
			if ( !tok[0] ) {
				break;
			}
			Q_strcat( out, size, va( "%s\"%s\"", first ? "" : ",", tok ) );
			first = 0;
		}
		FS_FreeFile( text );
	}
	if ( first ) {
		files = FS_ListFiles( "maps", ".bsp", &n );
		for ( i = 0; i < n; i++ ) {
			COM_StripExtension( files[i], path, sizeof( path ) );
			Q_strcat( out, size, va( "%s\"%s\"", first ? "" : ",", path ) );
			first = 0;
		}
		FS_FreeFileList( files );
	}
	Q_strcat( out, size, "]" );
}

static const char *Address( void ) {
	if ( sv_mmAddress->string[0] ) {
		return sv_mmAddress->string;
	}
	return va( "127.0.0.1:%s", Cvar_VariableString( "net_port" ) );
}

static void Register( void ) {
	char	body[4096];
	int		i, first = 1;

	Com_sprintf( body, sizeof( body ), "{\"address\":\"%s\",\"name\":\"%s\",\"maps\":{",
		Address(), Cvar_VariableString( "sv_hostname" ) );
	for ( i = 0; i < (int)ARRAY_LEN( mmModes ); i++ ) {
		if ( !strstr( va( " %s ", sv_mmModes->string ), va( " %s ", mmModes[i] ) ) ) {
			continue;
		}
		if ( !first ) {
			Q_strcat( body, sizeof( body ), "," );
		}
		AddModeMaps( body, sizeof( body ), mmModes[i] );
		first = 0;
	}
	Q_strcat( body, sizeof( body ), "}}" );
	MM_Start( &mm.req, sv_matchmaker->string, mm.seed, mm.pub, "POST", "/servers/register", body, "register" );
}

// the next quoted string in an array, advancing p
static qboolean NextString( const char **p, char *out, int size ) {
	const char *s = strchr( *p, '"' ), *e;

	if ( !s ) {
		return qfalse;
	}
	e = strchr( s + 1, '"' );
	if ( !e ) {
		return qfalse;
	}
	Q_strncpyz( out, s + 1, ( e - s < size ) ? e - s : size );
	*p = e + 1;
	return qtrue;
}

static void AddTeam( const char *json, const char *team ) {
	char		list[2048];
	const char	*p;
	mmSlot_t	*s;

	MM_JsonValue( json, team, list, sizeof( list ) );
	p = list;
	while ( mm.numSlots < MM_MAX_PLAYERS ) {
		s = &mm.slots[mm.numSlots];
		if ( !NextString( &p, s->key, sizeof( s->key ) ) ) {
			break;
		}
		Q_strncpyz( s->team, team, sizeof( s->team ) );
		s->client = -1;
		s->left = qfalse;
		MM_JsonValue( strstr( json, "\"tokens\"" ) ? strstr( json, "\"tokens\"" ) : "", s->key, s->token, sizeof( s->token ) );
		mm.numSlots++;
	}
}

static void StartMatch( const char *json ) {
	char map[MAX_QPATH];

	MM_JsonValue( json, "match", mm.match, sizeof( mm.match ) );
	MM_JsonValue( json, "mode", mm.mode, sizeof( mm.mode ) );
	MM_JsonValue( json, "map", map, sizeof( map ) );
	mm.numSlots = 0;
	AddTeam( json, "red" );
	AddTeam( json, "blue" );
	mm.reservedTime = svs.time;
	mm.resultPending = qfalse;
	Cvar_Set( "sv_mmMatch", mm.match );
	Com_Printf( "matchmaker: match %s, %s on %s, %d players\n", mm.match, mm.mode, map, mm.numSlots );

	// a player can be quicker than this server's next look at the matchmaker
	{
		int i;

		for ( i = 0; i < sv_maxclients->integer; i++ ) {
			if ( svs.clients[i].state >= CS_CONNECTED ) {
				SV_MM_ClientConnected( i, svs.clients[i].userinfo );
			}
		}
	}

	if ( !map[0] ) {
		Q_strncpyz( map, Cvar_VariableString( "mapname" ), sizeof( map ) );
	}
	// everyone currently here is not part of it
	Cbuf_AddText( va( "exec presets/%s.cfg\n", PoolName( mm.mode ) ) );
	if ( !strcmp( mm.mode, "ca2v2" ) || !strcmp( mm.mode, "tdm2v2" ) ) {
		Cbuf_AddText( "set g_teamSize 2\n" );
	}
	// matchmade: a timed start, never more than 90 s of warm-up
	Cbuf_AddText( "set g_readyUp 0\nset g_warmupMax 90\n" );
	Cbuf_AddText( va( "map %s\n", map ) );
}

static void SendResult( const char *winner ) {
	char	left[1200];
	int		i;

	left[0] = 0;
	for ( i = 0; i < mm.numSlots; i++ ) {
		if ( mm.slots[i].left || mm.slots[i].client < 0 ) {
			Q_strcat( left, sizeof( left ), va( "%s\"%s\"", left[0] ? "," : "", mm.slots[i].key ) );
		}
	}
	Com_sprintf( mm.resultBody, sizeof( mm.resultBody ), "{\"match\":\"%s\",\"winner\":\"%s\",\"left_early\":[%s]}",
		mm.match, winner, left );
	mm.resultPending = qtrue;
	Com_Printf( "matchmaker: result %s\n", mm.resultBody );
	mm.match[0] = 0;
	Cvar_Set( "sv_mmMatch", "" );
}

/*
==================
SV_MM_Result_f

mm_result red|blue|draw|<client number>, from the game at the end of a match
==================
*/
static void SV_MM_Result_f( void ) {
	const char	*w = Cmd_Argv( 1 );
	int			i;

	if ( !mm.match[0] ) {
		return;
	}
	if ( !strcmp( w, "red" ) || !strcmp( w, "blue" ) || !strcmp( w, "draw" ) ) {
		SendResult( w );
		return;
	}
	for ( i = 0; i < mm.numSlots; i++ ) {
		if ( mm.slots[i].client == atoi( w ) && mm.slots[i].client >= 0 && w[0] ) {
			SendResult( mm.slots[i].key );
			return;
		}
	}
	SendResult( "draw" );
}

void SV_MM_Init( void ) {
	sv_matchmaker = Cvar_Get( "sv_matchmaker", "", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_matchmaker, "Matchmaker address (http://host:port) this server takes matches from; empty: none" );
	sv_mmAddress = Cvar_Get( "sv_mmAddress", "", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_mmAddress, "Address players use to reach this server; empty: 127.0.0.1 and net_port" );
	sv_mmModes = Cvar_Get( "sv_mmModes", "duel tdm tdm2v2 ctf ca ca2v2 sacrifice roundduel", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_mmModes, "Matchmaking modes this server offers" );
	Cvar_Get( "sv_mmMatch", "", CVAR_ROM | CVAR_SERVERINFO );
	Cmd_AddCommand( "mm_result", SV_MM_Result_f );
}

/*
==================
SV_MM_CheckConnect

While a match is reserved, only its players get in; returns the reason
to refuse, or NULL
==================
*/
const char *SV_MM_CheckConnect( const char *userinfo, netadr_t from ) {
	const char	*token;
	int			i;

	if ( !mm.match[0] || NET_IsLocalAddress( from ) ) {
		return NULL;
	}
	token = Info_ValueForKey( userinfo, "mmtoken" );
	for ( i = 0; i < mm.numSlots; i++ ) {
		if ( token[0] && !strcmp( token, mm.slots[i].token ) ) {
			return NULL;
		}
	}
	return "This server is reserved for a match.";
}

void SV_MM_ClientConnected( int clientNum, const char *userinfo ) {
	const char	*token = Info_ValueForKey( userinfo, "mmtoken" );
	int			i;

	if ( !mm.match[0] || !token[0] ) {
		return;
	}
	for ( i = 0; i < mm.numSlots; i++ ) {
		if ( !strcmp( token, mm.slots[i].token ) ) {
			mm.slots[i].client = clientNum;
			mm.slots[i].left = qfalse;
		}
	}
}

void SV_MM_ClientEntered( int clientNum ) {
	int i;

	for ( i = 0; mm.match[0] && i < mm.numSlots; i++ ) {
		if ( mm.slots[i].client == clientNum ) {
			Cbuf_AddText( va( "forceteam %d %s\n", clientNum,
				( !strcmp( mm.mode, "duel" ) || !strcmp( mm.mode, "roundduel" ) ) ? "free" : mm.slots[i].team ) );
		}
	}
}

void SV_MM_ClientDropped( int clientNum ) {
	int i;

	for ( i = 0; mm.match[0] && i < mm.numSlots; i++ ) {
		if ( mm.slots[i].client == clientNum ) {
			mm.slots[i].left = qtrue;
		}
	}
}

// players who never came within the connect time forfeit; a duel with
// one player there is theirs
static void CheckConnectTime( void ) {
	int i, here = 0, last = -1;

	if ( !mm.match[0] || svs.time - mm.reservedTime < 60000 ) {
		return;
	}
	for ( i = 0; i < mm.numSlots; i++ ) {
		if ( mm.slots[i].client >= 0 && !mm.slots[i].left ) {
			here++;
			last = i;
		}
	}
	if ( here == mm.numSlots ) {
		mm.reservedTime = svs.time + 0x3fffffff;	// everyone came, stop checking
		return;
	}
	if ( !strcmp( mm.mode, "duel" ) || !strcmp( mm.mode, "roundduel" ) || here == 0 ) {
		SendResult( here == 1 ? mm.slots[last].key : "draw" );
		return;
	}
	mm.reservedTime = svs.time + 0x3fffffff;		// team modes play on short-handed
}

void SV_MM_Frame( void ) {
	int now;

	if ( !sv_matchmaker || !sv_matchmaker->string[0] || !com_sv_running->integer ) {
		return;
	}
	if ( !mm.keyLoaded ) {
		if ( !MM_LoadKey( MM_KEY_FILE, mm.seed, mm.pub ) ) {
			Cvar_Set( "sv_matchmaker", "" );
			return;
		}
		mm.keyLoaded = qtrue;
		Com_Printf( "matchmaker: server key %s\n", mm.pub );
	}

	CheckConnectTime();

	if ( !MM_Poll( &mm.req ) ) {
		return;
	}
	if ( mm.req.state == MMR_DONE || mm.req.state == MMR_FAILED ) {
		if ( !strcmp( mm.req.what, "reservation" ) && mm.req.status == 200 ) {
			char id[64];

			MM_JsonValue( mm.req.body, "match", id, sizeof( id ) );
			if ( strcmp( id, "null" ) && id[0] && strcmp( id, mm.match ) && !mm.resultPending ) {
				StartMatch( mm.req.body );
			}
		} else if ( !strcmp( mm.req.what, "result" ) ) {
			if ( mm.req.status == 200 || mm.req.status == 403 ) {
				mm.resultPending = qfalse;		// taken, or not ours any more
			}
		} else if ( !strcmp( mm.req.what, "register" ) && mm.req.status != 200 ) {
			mm.nextRegister = 0;
			Com_DPrintf( "matchmaker: register failed (%d)\n", mm.req.status );
		} else if ( mm.req.status == 404 ) {
			mm.nextRegister = 0;			// the service restarted: register again
		}
		mm.req.state = MMR_IDLE;
	}

	now = Sys_Milliseconds();
	if ( mm.resultPending ) {
		if ( now < mm.nextResult ) {
			return;
		}
		mm.nextResult = now + MM_POLL_MSEC;
		MM_Start( &mm.req, sv_matchmaker->string, mm.seed, mm.pub, "POST", "/servers/result", mm.resultBody, "result" );
	} else if ( now >= mm.nextRegister ) {
		mm.nextRegister = now + MM_REGISTER_MSEC;
		Register();
	} else if ( now >= mm.nextPoll ) {
		mm.nextPoll = now + MM_POLL_MSEC;
		MM_Start( &mm.req, sv_matchmaker->string, mm.seed, mm.pub, "POST", "/servers/reservation", "", "reservation" );
	}
}
