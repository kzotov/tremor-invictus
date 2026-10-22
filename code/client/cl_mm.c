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
// cl_mm.c -- a player's side of matchmaking
//
// Find Match: the menus tick modes (mm_m_<mode> cvars) and run mm_search.
// While searching, the client asks the matchmaker for its status every
// second and mirrors it in mm_* cvars for the documents to show. A found
// match opens the ready check; once everyone accepts, the client connects
// to the reserved server with its one-time token.

#include "client.h"
#include "../qcommon/mm_common.h"

#define MM_KEY_FILE		"mmplayer.key"
#define MM_POLL_MSEC	1000
#define MM_FOUND_MSEC	6000	// the match-found screen before connecting
#define MM_MAX_SHOWN	8

static cvar_t	*mm_url;

static struct {
	qboolean	keyLoaded;
	byte		seed[32];
	char		pub[65];
	mmRequest_t	req;
	char		next[32];		// an action waiting for the request slot
	char		nextBody[256];
	int			nextPoll;
	qboolean	polling;		// searching, in a ready check, or connecting
	char		lastState[32];
	int			foundTime;		// the match-found screen went up; 0 when not showing
	char		address[128];	// the reserved server, connected to after the screen
	char		token[64];
} mm;

static const char *mmModes[] = { "duel", "tdm", "tdm2v2", "ctf", "ca", "ca2v2", "sacrifice", "roundduel" };

static void SetState( const char *state ) {
	if ( strcmp( state, mm.lastState ) ) {
		Com_DPrintf( "matchmaker: %s\n", state );
	}
	Cvar_Set( "mm_state", state );
	if ( !strcmp( state, "ready_check" ) && strcmp( mm.lastState, "ready_check" ) ) {
		Cbuf_ExecuteText( EXEC_NOW, "rml_show ui/readycheck.rml\n" );
		// call the player back to the game: a chime, and the Dock icon
		// bounces while the window is in the background
		S_StartLocalSound( S_RegisterSound( "sound/tremor/match_found.wav", qfalse ), CHAN_LOCAL_SOUND );
		IN_FlashWindow();
	} else if ( !strcmp( state, "found" ) && strcmp( mm.lastState, "found" ) ) {
		Cbuf_ExecuteText( EXEC_NOW, "rml_show ui/matchfound.rml\n" );
	} else if ( strcmp( state, "ready_check" ) && !strcmp( mm.lastState, "ready_check" ) ) {
		if ( !Q_stricmp( Cvar_VariableString( "rml_doc" ), "ui/readycheck.rml" ) ) {
			Cbuf_ExecuteText( EXEC_NOW, "rml_hide\n" );
			if ( clc.state != CA_ACTIVE ) {
				Cbuf_ExecuteText( EXEC_NOW, "rml_show ui/main.rml\n" );
			}
		}
	}
	Q_strncpyz( mm.lastState, state, sizeof( mm.lastState ) );
}

static qboolean EnsureKey( void ) {
	if ( !mm.keyLoaded ) {
		if ( !MM_LoadKey( MM_KEY_FILE, mm.seed, mm.pub ) ) {
			return qfalse;
		}
		mm.keyLoaded = qtrue;
		Cvar_Set( "mm_id", mm.pub );
	}
	return qtrue;
}

static void Queue( const char *what, const char *body ) {
	Q_strncpyz( mm.next, what, sizeof( mm.next ) );
	Q_strncpyz( mm.nextBody, body, sizeof( mm.nextBody ) );
}

static void CL_MM_Search_f( void ) {
	char	body[256];
	int		i, n = 0;

	Q_strncpyz( body, "{\"modes\":[", sizeof( body ) );
	for ( i = 0; i < (int)ARRAY_LEN( mmModes ); i++ ) {
		if ( Cvar_VariableIntegerValue( va( "mm_m_%s", mmModes[i] ) ) ) {
			Q_strcat( body, sizeof( body ), va( "%s\"%s\"", n ? "," : "", mmModes[i] ) );
			n++;
		}
	}
	Q_strcat( body, sizeof( body ), "]}" );
	if ( !n ) {
		Cvar_Set( "mm_error", "Tick at least one mode." );
		return;
	}
	Cvar_Set( "mm_error", "" );
	Queue( "queue", body );
}

static void CL_MM_Cancel_f( void ) {
	Queue( "leave", "" );
}

static void CL_MM_Accept_f( void ) {
	Cvar_Set( "mm_accepted", "true" );
	Queue( "ready", "{\"accept\":true}" );
}

static void CL_MM_Decline_f( void ) {
	Queue( "ready", "{\"accept\":false}" );
}

static void CL_MM_Profile_f( void ) {
	Queue( "ratings", "" );
}

static void CL_MM_PartyCreate_f( void ) {
	Queue( "party_create", "" );
}

// mm_party_join <code>; the menu's code field runs it on every edit, so
// anything but a whole code is ignored
static void CL_MM_PartyJoin_f( void ) {
	const char	*code = Cmd_Argv( 1 );
	int			i;

	if ( strlen( code ) != 6 ) {
		return;
	}
	for ( i = 0; i < 6; i++ ) {
		if ( !isalnum( (unsigned char)code[i] ) ) {
			return;
		}
	}
	Queue( "party_join", va( "{\"code\":\"%s\"}", code ) );
}

static void CL_MM_PartyLeave_f( void ) {
	Queue( "party_leave", "" );
}

static void StartNext( void ) {
	const char *method = "POST", *path;

	if ( !strcmp( mm.next, "queue" ) ) {
		path = "/queue";
	} else if ( !strcmp( mm.next, "leave" ) ) {
		method = "DELETE";
		path = "/queue";
	} else if ( !strcmp( mm.next, "ready" ) ) {
		path = "/ready";
	} else if ( !strcmp( mm.next, "ratings" ) ) {
		path = "/ratings";
	} else if ( !strcmp( mm.next, "party_create" ) ) {
		path = "/party/create";
	} else if ( !strcmp( mm.next, "party_join" ) ) {
		path = "/party/join";
	} else if ( !strcmp( mm.next, "party_leave" ) ) {
		path = "/party/leave";
	} else {
		path = "/status";
	}
	MM_Start( &mm.req, mm_url->string, mm.seed, mm.pub, method, path, mm.nextBody, mm.next );
	mm.next[0] = 0;
}

// "ratings":[{"mode":"duel","rating":1623,"games":12},{"mode":"tdm","placement":3}]
static void ReadRatings( const char *body ) {
	char		value[64], mode[32];
	const char	*p;
	int			i;

	for ( i = 0; i < (int)ARRAY_LEN( mmModes ); i++ ) {
		Cvar_Set( va( "mm_rating_%s", mmModes[i] ), "-" );
	}
	p = strstr( body, "\"ratings\"" );
	while ( p && ( p = strchr( p, '{' ) ) != NULL ) {
		MM_JsonValue( p, "mode", mode, sizeof( mode ) );
		if ( MM_JsonValue( p, "rating", value, sizeof( value ) ) && strchr( p, '}' ) > strstr( p, "\"rating\"" ) ) {
			Cvar_Set( va( "mm_rating_%s", mode ), value );
		} else if ( MM_JsonValue( p, "placement", value, sizeof( value ) ) ) {
			Cvar_Set( va( "mm_rating_%s", mode ), va( "%s/10", value ) );
		}
		p++;
	}
}

// "party":{"code":"ABC123","leader":"<key>","members":["<key>",...]} or null
static void ReadParty( const char *body ) {
	char		party[2048], value[128];
	const char	*p;
	int			members = 0;

	if ( !MM_JsonValue( body, "party", party, sizeof( party ) ) || party[0] != '{' ) {
		Cvar_Set( "mm_party_code", "" );
		Cvar_Set( "mm_party_size", "0" );
		Cvar_Set( "mm_party_leader", "1" );
		return;
	}
	MM_JsonValue( party, "code", value, sizeof( value ) );
	Cvar_Set( "mm_party_code", value );
	MM_JsonValue( party, "leader", value, sizeof( value ) );
	Cvar_Set( "mm_party_leader", !strcmp( value, mm.pub ) ? "1" : "0" );
	// each member is a quoted key: count the opening quotes
	p = strstr( party, "\"members\"" );
	p = p ? strchr( p, '[' ) : NULL;
	while ( p && ( p = strchr( p + 1, '"' ) ) != NULL ) {
		members++;
		p = strchr( p + 1, '"' );
	}
	Cvar_Set( "mm_party_size", va( "%d", members ) );
}

/*
=================
ReadPlayers

The match's players for the match-found screen: mm_p<n>_name, _champ (the
stage draws it), _champName, _team and _rating, reds first; mm_pcount
=================
*/
static void ReadPlayers( const char *body ) {
	char		players[4096], obj[512], value[64];
	const char	*p, *end;
	int			pass, n = 0;

	if ( !MM_JsonValue( body, "players", players, sizeof( players ) ) ) {
		return;
	}
	for ( pass = 0; pass < 2; pass++ ) {
		for ( p = players; ( p = strchr( p, '{' ) ) != NULL && n < MM_MAX_SHOWN; p = end ) {
			end = strchr( p, '}' );
			if ( !end ) {
				break;
			}
			Q_strncpyz( obj, p, end - p + 2 < (int)sizeof( obj ) ? end - p + 2 : (int)sizeof( obj ) );
			MM_JsonValue( obj, "team", value, sizeof( value ) );
			if ( strcmp( value, pass ? "blue" : "red" ) ) {
				continue;
			}
			Cvar_Set( va( "mm_p%i_team", n ), value );
			MM_JsonValue( obj, "name", value, sizeof( value ) );
			Cvar_Set( va( "mm_p%i_name", n ), value );
			MM_JsonValue( obj, "champion", value, sizeof( value ) );
			if ( !value[0] ) {
				Q_strncpyz( value, "ranger", sizeof( value ) );
			}
			Cvar_Set( va( "mm_p%i_champ", n ), value );
			value[0] = toupper( (unsigned char)value[0] );
			Cvar_Set( va( "mm_p%i_champName", n ), value );
			MM_JsonValue( obj, "rating", value, sizeof( value ) );
			Cvar_Set( va( "mm_p%i_rating", n ), strcmp( value, "null" ) ? value : "" );
			n++;
		}
	}
	Cvar_Set( "mm_pcount", va( "%i", n ) );
}

static void ReadStatus( const char *body ) {
	char state[32], value[256];

	ReadParty( body );
	MM_JsonValue( body, "state", state, sizeof( state ) );
	if ( !strcmp( state, "searching" ) ) {
		MM_JsonValue( body, "waiting", value, sizeof( value ) );
		Cvar_Set( "mm_elapsed", va( "%d:%02d", atoi( value ) / 60, atoi( value ) % 60 ) );
		SetState( "searching" );
	} else if ( !strcmp( state, "ready_check" ) ) {
		MM_JsonValue( body, "mode", value, sizeof( value ) );
		Cvar_Set( "mm_mode", value );
		MM_JsonValue( body, "seconds", value, sizeof( value ) );
		Cvar_Set( "mm_seconds", value );
		MM_JsonValue( body, "accepted", value, sizeof( value ) );
		Cvar_Set( "mm_accepted", value );
		SetState( "ready_check" );
	} else if ( !strcmp( state, "connect" ) ) {
		char address[128], token[64];

		MM_JsonValue( body, "map", value, sizeof( value ) );
		Cvar_Set( "mm_map", value );
		MM_JsonValue( body, "address", address, sizeof( address ) );
		MM_JsonValue( body, "token", token, sizeof( token ) );
		if ( !address[0] || !strcmp( address, "null" ) ) {
			SetState( "waiting_server" );		// accepted; no server free yet
			return;
		}
		ReadPlayers( body );
		Q_strncpyz( mm.address, address, sizeof( mm.address ) );
		Q_strncpyz( mm.token, token, sizeof( mm.token ) );
		if ( !mm.foundTime ) {
			mm.foundTime = cls.realtime ? cls.realtime : 1;
		}
		SetState( "found" );
	} else {
		// idle: the search ended (cancelled, or declined and blocked);
		// party members keep listening for the leader's search
		mm.polling = Cvar_VariableString( "mm_party_code" )[0] != 0;
		SetState( "idle" );
	}
}

static void Done( void ) {
	const char *what = mm.req.what;

	if ( mm.req.state == MMR_FAILED ) {
		Cvar_Set( "mm_error", "The matchmaker can't be reached." );
		mm.polling = qfalse;
		SetState( "idle" );
		return;
	}
	if ( mm.req.status != 200 ) {
		char error[128];

		MM_JsonValue( mm.req.body, "error", error, sizeof( error ) );
		Com_DPrintf( "matchmaker: %s: %d %s\n", what, mm.req.status, error );
		Cvar_Set( "mm_error", error[0] ? error : va( "Matchmaker error %d", mm.req.status ) );
		if ( !strcmp( what, "queue" ) ) {
			mm.polling = qfalse;
			SetState( "idle" );
		}
		return;
	}
	if ( !strcmp( what, "queue" ) ) {
		mm.polling = qtrue;
		mm.nextPoll = 0;
		Cvar_Set( "mm_elapsed", "0:00" );
		SetState( "searching" );
	} else if ( !strcmp( what, "leave" ) ) {
		mm.polling = qfalse;
		SetState( "idle" );
	} else if ( !strcmp( what, "status" ) ) {
		ReadStatus( mm.req.body );
	} else if ( !strcmp( what, "ratings" ) ) {
		ReadRatings( mm.req.body );
	} else if ( !strcmp( what, "ready" ) ) {
		mm.nextPoll = 0;
	} else if ( !strcmp( what, "party_create" ) || !strcmp( what, "party_join" ) || !strcmp( what, "party_leave" ) ) {
		Cvar_Set( "mm_error", "" );
		mm.polling = qtrue;		// the next status fills in the party
		mm.nextPoll = 0;
	}
}

void CL_MM_Frame( void ) {
	if ( !mm_url ) {
		return;
	}
	if ( !MM_Poll( &mm.req ) ) {
		return;
	}
	if ( mm.req.state == MMR_DONE || mm.req.state == MMR_FAILED ) {
		Done();
		mm.req.state = MMR_IDLE;
	}
	if ( mm.next[0] ) {
		if ( EnsureKey() ) {
			StartNext();
		} else {
			mm.next[0] = 0;
			Cvar_Set( "mm_error", "Can't make a player key." );
		}
		return;
	}
	if ( mm.foundTime ) {
		int left = MM_FOUND_MSEC - ( cls.realtime - mm.foundTime );

		Cvar_Set( "mm_seconds", va( "%i", left > 0 ? ( left + 999 ) / 1000 : 0 ) );
		if ( left <= 0 ) {
			mm.foundTime = 0;
			mm.polling = qfalse;
			SetState( "connecting" );
			Cvar_Set( "mmtoken", mm.token );
			Com_Printf( "matchmaker: match on %s\n", mm.address );
			// now, ahead of anything waiting in the command buffer
			Cbuf_ExecuteText( EXEC_NOW, "rml_hide\n" );
			Cbuf_ExecuteText( EXEC_NOW, va( "connect %s\n", mm.address ) );
			return;
		}
	}
	if ( mm.polling && cls.realtime >= mm.nextPoll ) {
		char	name[64], champion[32], body[160];
		int		i, j;

		mm.nextPoll = cls.realtime + MM_POLL_MSEC;
		// what the others see on the match-found screen
		Q_strncpyz( name, Cvar_VariableString( "name" ), sizeof( name ) );
		Q_CleanStr( name );
		for ( i = j = 0; name[i]; i++ ) {
			if ( name[i] != '"' && name[i] != '\\' ) {
				name[j++] = name[i];
			}
		}
		name[j] = 0;
		Q_strncpyz( champion, Cvar_VariableString( "champion" ), sizeof( champion ) );
		for ( i = 0; champion[i]; i++ ) {
			if ( !isalnum( (unsigned char)champion[i] ) ) {
				champion[i] = 0;
				break;
			}
		}
		Com_sprintf( body, sizeof( body ), "{\"name\":\"%s\",\"champion\":\"%s\"}", name, champion );
		Queue( "status", body );
	}
}

void CL_MM_Init( void ) {
	int i;

	mm_url = Cvar_Get( "mm_url", "http://127.0.0.1:27960", CVAR_ARCHIVE );
	Cvar_SetDescription( mm_url, "Matchmaker address (http://host:port)" );
	for ( i = 0; i < (int)ARRAY_LEN( mmModes ); i++ ) {
		Cvar_Get( va( "mm_m_%s", mmModes[i] ), i == 0 ? "1" : "0", CVAR_ARCHIVE );
		Cvar_Get( va( "mm_rating_%s", mmModes[i] ), "-", CVAR_TEMP );
	}
	Cvar_Get( "mm_state", "idle", CVAR_TEMP );
	Cvar_Get( "mm_elapsed", "0:00", CVAR_TEMP );
	Cvar_Get( "mm_seconds", "15", CVAR_TEMP );
	Cvar_Get( "mm_mode", "", CVAR_TEMP );
	Cvar_Get( "mm_map", "", CVAR_TEMP );
	Cvar_Get( "mm_accepted", "false", CVAR_TEMP );
	Cvar_Get( "mm_error", "", CVAR_TEMP );
	Cvar_Get( "mm_id", "", CVAR_TEMP );
	Cvar_Get( "mm_party_code", "", CVAR_TEMP );
	Cvar_Get( "mm_pcount", "0", CVAR_TEMP );
	Cvar_Get( "mm_party_size", "0", CVAR_TEMP );
	Cvar_Get( "mm_party_leader", "1", CVAR_TEMP );
	Cvar_Get( "mmtoken", "", CVAR_USERINFO | CVAR_TEMP );
	EnsureKey();		// so the menus can show the player id

	Cmd_AddCommand( "mm_search", CL_MM_Search_f );
	Cmd_AddCommand( "mm_cancel", CL_MM_Cancel_f );
	Cmd_AddCommand( "mm_accept", CL_MM_Accept_f );
	Cmd_AddCommand( "mm_decline", CL_MM_Decline_f );
	Cmd_AddCommand( "mm_profile", CL_MM_Profile_f );
	Cmd_AddCommand( "mm_party_create", CL_MM_PartyCreate_f );
	Cmd_AddCommand( "mm_party_join", CL_MM_PartyJoin_f );
	Cmd_AddCommand( "mm_party_leave", CL_MM_PartyLeave_f );
}
