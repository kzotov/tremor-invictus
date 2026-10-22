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
} mm;

static const char *mmModes[] = { "duel", "tdm", "tdm2v2", "ctf", "ca", "ca2v2", "sacrifice", "roundduel" };

static void SetState( const char *state ) {
	if ( strcmp( state, mm.lastState ) ) {
		Com_DPrintf( "matchmaker: %s\n", state );
	}
	Cvar_Set( "mm_state", state );
	if ( !strcmp( state, "ready_check" ) && strcmp( mm.lastState, "ready_check" ) ) {
		Cbuf_ExecuteText( EXEC_NOW, "rml_show ui/readycheck.rml\n" );
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

static void ReadStatus( const char *body ) {
	char state[32], value[256];

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
		SetState( "connecting" );
		mm.polling = qfalse;
		Cvar_Set( "mmtoken", token );
		Com_Printf( "matchmaker: match on %s\n", address );
		// now, ahead of anything waiting in the command buffer
		Cbuf_ExecuteText( EXEC_NOW, "rml_hide\n" );
		Cbuf_ExecuteText( EXEC_NOW, va( "connect %s\n", address ) );
	} else {
		// idle: the search ended (cancelled, or declined and blocked)
		mm.polling = qfalse;
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
	if ( mm.polling && cls.realtime >= mm.nextPoll ) {
		mm.nextPoll = cls.realtime + MM_POLL_MSEC;
		Queue( "status", "" );
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
	Cvar_Get( "mmtoken", "", CVAR_USERINFO | CVAR_TEMP );
	Cmd_AddCommand( "mm_search", CL_MM_Search_f );
	Cmd_AddCommand( "mm_cancel", CL_MM_Cancel_f );
	Cmd_AddCommand( "mm_accept", CL_MM_Accept_f );
	Cmd_AddCommand( "mm_decline", CL_MM_Decline_f );
	Cmd_AddCommand( "mm_profile", CL_MM_Profile_f );
}
