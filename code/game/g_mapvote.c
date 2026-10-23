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
// g_mapvote.c -- the end-of-match map vote
//
// After the podium, players pick the next map from three of the mode's
// maps. CS_MAPVOTE carries "endTime map1 map2 map3 votes1 votes2 votes3";
// "mapvote <1-3>" casts or changes a vote.
//
// With g_lobby 1 the vote moves to the pre-match lobby (lobby.md) instead,
// which also holds the champion pick; see the end of this file.

#include "g_local.h"

#define MAPVOTE_CHOICES		3
#define MAPVOTE_CHANGE_GAP	2000	// msec between changes of one player's vote

static char		mapVoteMaps[MAPVOTE_CHOICES][MAX_QPATH];
static int		mapVoteCount;		// maps on offer, 0 when no vote
static int		mapVoteEnd;			// level.time the vote closes
static int		mapVoteOf[MAX_CLIENTS];		// 1-3, 0 = none
static int		mapVoteTime[MAX_CLIENTS];	// level.time of the last change

/*
=================
G_ArenaFitsMode

True when an arena's "type" list names the running mode
=================
*/
static qboolean G_ArenaFitsMode( const char *info ) {
	const char	*type = Info_ValueForKey( info, "type" );

	if ( !type[0] ) {
		return g_gametype.integer == GT_FFA;
	}
	if ( G_ListHasWord( type, G_ModeName() ) ) {
		return qtrue;
	}
	switch ( g_gametype.integer ) {
	case GT_FFA:		return G_ListHasWord( type, "ffa" );
	case GT_TOURNAMENT:	return G_ListHasWord( type, "tourney" ) || G_ListHasWord( type, "duel" );
	case GT_TEAM:		return G_ListHasWord( type, "team" );
	case GT_CTF:		return G_ListHasWord( type, "ctf" );
	default:			return qfalse;
	}
}

/*
=================
G_MapVoteSend
=================
*/
static void G_MapVoteSend( void ) {
	int		votes[MAPVOTE_CHOICES];
	int		i;

	memset( votes, 0, sizeof( votes ) );
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED && mapVoteOf[i] > 0 ) {
			votes[mapVoteOf[i] - 1]++;
		}
	}
	trap_SetConfigstring( CS_MAPVOTE, va( "%i %s %s %s %i %i %i", mapVoteEnd,
		mapVoteCount > 0 ? mapVoteMaps[0] : "-", mapVoteCount > 1 ? mapVoteMaps[1] : "-",
		mapVoteCount > 2 ? mapVoteMaps[2] : "-", votes[0], votes[1], votes[2] ) );
}

/*
=================
G_MapVoteStart

Picks up to three of the mode's maps, not the current one, from g_mapPool
when it is set, otherwise from every arena that lists the mode
=================
*/
static void G_MapVoteChoose( void );

void G_MapVoteStart( void ) {
	mapVoteCount = 0;
	mapVoteEnd = 0;
	memset( mapVoteOf, 0, sizeof( mapVoteOf ) );
	if ( !g_mapVote.integer || g_lobby.integer ) {
		return;		// (the lobby votes before the next match instead)
	}
	G_MapVoteChoose();
	if ( !mapVoteCount ) {
		return;
	}
	mapVoteEnd = level.intermissiontime + g_mapVoteDelay.integer + g_mapVoteTime.integer * 1000;
	G_MapVoteSend();
}

/*
=================
G_MapVoteChoose

Three maps at random from the mode's pool, never the current one
=================
*/
static void G_MapVoteChoose( void ) {
	char		pool[64][MAX_QPATH];
	char		current[MAX_QPATH];
	char		list[MAX_CVAR_VALUE_STRING];
	char		*p, *token;
	int			numPool = 0, i, j;
	const char	*info;

	mapVoteCount = 0;
	trap_Cvar_VariableStringBuffer( "mapname", current, sizeof( current ) );

	trap_Cvar_VariableStringBuffer( "g_mapPool", list, sizeof( list ) );
	if ( !list[0] ) {
		// the mode's pool file, mappools/<mode>.txt
		fileHandle_t	f;
		int				len = trap_FS_FOpenFile( va( "mappools/%s.txt", G_ModeName() ), &f, FS_READ );

		if ( len > 0 ) {
			if ( len >= sizeof( list ) ) {
				len = sizeof( list ) - 1;
			}
			trap_FS_Read( list, len, f );
			list[len] = 0;
		}
		if ( len >= 0 ) {
			trap_FS_FCloseFile( f );
		}
	}
	p = list;
	if ( list[0] ) {
		while ( numPool < 64 ) {
			token = COM_Parse( &p );
			if ( !token[0] ) {
				break;
			}
			if ( Q_stricmp( token, current ) ) {
				Q_strncpyz( pool[numPool++], token, MAX_QPATH );
			}
		}
	} else {
		for ( i = 0 ; i < G_ArenaCount() && numPool < 64 ; i++ ) {
			info = G_ArenaInfo( i );
			if ( G_ArenaFitsMode( info ) && Q_stricmp( Info_ValueForKey( info, "map" ), current ) ) {
				Q_strncpyz( pool[numPool++], Info_ValueForKey( info, "map" ), MAX_QPATH );
			}
		}
	}
	if ( !numPool ) {
		return;
	}

	// three at random
	for ( i = 0 ; i < MAPVOTE_CHOICES && numPool > 0 ; i++ ) {
		j = rand() % numPool;
		Q_strncpyz( mapVoteMaps[mapVoteCount++], pool[j], MAX_QPATH );
		Q_strncpyz( pool[j], pool[numPool - 1], MAX_QPATH );
		numPool--;
	}
}

/*
=================
G_MapVoteOpen
=================
*/
static qboolean G_MapVoteOpen( void ) {
	return mapVoteCount > 0 && level.intermissiontime && level.time < mapVoteEnd;
}

/*
=================
G_MapVoteActive

True while the vote is running or yet to close: the intermission waits for it
=================
*/
qboolean G_MapVoteActive( void ) {
	return G_MapVoteOpen();
}

/*
=================
Cmd_MapVote_f
=================
*/
static qboolean G_LobbyVoteOpen( void );
static void G_LobbySend( void );

void Cmd_MapVote_f( gentity_t *ent ) {
	char	arg[8];
	int		n, clientNum = ent - g_entities;

	if ( G_LobbyVoteOpen() ) {
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR && g_gametype.integer != GT_TOURNAMENT ) {
			return;		// players only; in duel the queue votes too
		}
		trap_Argv( 1, arg, sizeof( arg ) );
		n = atoi( arg );
		if ( n >= 1 && n <= mapVoteCount && n != mapVoteOf[clientNum]
			&& ( !mapVoteOf[clientNum] || level.time - mapVoteTime[clientNum] >= MAPVOTE_CHANGE_GAP ) ) {
			mapVoteOf[clientNum] = n;
			mapVoteTime[clientNum] = level.time;
			G_LobbySend();
		}
		return;
	}
	if ( !G_MapVoteOpen() || level.time < level.intermissiontime + g_mapVoteDelay.integer ) {
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR && g_gametype.integer != GT_TOURNAMENT ) {
		return;		// players only; in duel the queue votes too
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	n = atoi( arg );
	if ( n < 1 || n > mapVoteCount || n == mapVoteOf[clientNum] ) {
		return;
	}
	if ( mapVoteOf[clientNum] && level.time - mapVoteTime[clientNum] < MAPVOTE_CHANGE_GAP ) {
		return;
	}
	mapVoteOf[clientNum] = n;
	mapVoteTime[clientNum] = level.time;
	G_MapVoteSend();
}

/*
=================
G_MapVoteWinner

The map with the most votes, a random one of those tied; NULL without a vote
=================
*/
const char *G_MapVoteWinner( void ) {
	int		votes[MAPVOTE_CHOICES], best = -1, tied = 0, pick, i;

	if ( !mapVoteCount ) {
		return NULL;
	}
	memset( votes, 0, sizeof( votes ) );
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED && mapVoteOf[i] > 0 ) {
			votes[mapVoteOf[i] - 1]++;
		}
	}
	for ( i = 0 ; i < mapVoteCount ; i++ ) {
		if ( votes[i] > best ) {
			best = votes[i];
			tied = 1;
		} else if ( votes[i] == best ) {
			tied++;
		}
	}
	pick = rand() % tied;
	for ( i = 0 ; i < mapVoteCount ; i++ ) {
		if ( votes[i] == best && pick-- == 0 ) {
			return mapVoteMaps[i];
		}
	}
	return mapVoteMaps[0];
}

/*
================================================================================

THE PRE-MATCH LOBBY (lobby.md), g_lobby 1

Before each match the server holds warm-up for: "voting in" 5 s, the map
vote (g_lobbyVoteTime), the reveal 3 s, then the champion phase and a 5 s
countdown. A winning map other than the current one is loaded and the
lobby goes on there with the champion phase (g_lobbyState "pick"); once the
match is under way g_lobbyState is "done", and the intermission clears it
for the next match. Champions are chosen as always (the champion userinfo);
"lobbylock" locks the choice in. CS_LOBBY carries
"phase endTime pickMode winner map1 map2 map3 votes1 votes2 votes3" and then
"client:champion:locked" for each player (champion -1 while a blind pick
is hidden).

================================================================================
*/

typedef enum {
	LOBBY_OFF,
	LOBBY_WAIT,		// for the first player
	LOBBY_SOON,		// "voting in"
	LOBBY_VOTE,
	LOBBY_REVEAL,
	LOBBY_PICK,
	LOBBY_START		// the countdown
} lobbyPhase_t;

#define LOBBY_SOON_MSEC		5000	// (d)
#define LOBBY_REVEAL_MSEC	3000	// (d)
#define LOBBY_START_MSEC	5000	// (d)
#define LOBBY_DUEL_PICK		20000	// classic duel's blind pick (d)

static int		lobbyPhase;
static int		lobbyEnd;
static int		lobbyWinner;		// 1-3, 0 before the reveal
static qboolean	lobbyLocked[MAX_CLIENTS];

/*
=================
G_LobbyPickMode

0 free pick, 1 classic duel's blind pick, 2 none (round duel drafts in the
match; a forced champion leaves nothing to pick)
=================
*/
static int G_LobbyPickMode( void ) {
	if ( g_roundDuel.integer || g_randomChampion.integer ) {
		return 2;
	}
	return g_gametype.integer == GT_TOURNAMENT ? 1 : 0;
}

static qboolean G_LobbyVoteOpen( void ) {
	return lobbyPhase == LOBBY_VOTE && mapVoteCount > 0;
}

/*
=================
G_LobbyHidesPicks

A classic duel's blind pick: every champion hidden until the pick ends
=================
*/
qboolean G_LobbyHidesPicks( void ) {
	return lobbyPhase != LOBBY_OFF && lobbyPhase < LOBBY_START && G_LobbyPickMode() == 1;
}

/*
=================
G_LobbyFrozenChampion

A classic duel's lobby pick, kept from its reveal to the end of the match
(g_lobbyPicks "client:champion ..."); -1 when nothing is held
=================
*/
int G_LobbyFrozenChampion( int clientNum ) {
	char	picks[MAX_CVAR_VALUE_STRING], *p, *token;
	int		client, champion;

	if ( !g_lobby.integer || G_LobbyPickMode() != 1 || ( lobbyPhase != LOBBY_START && lobbyPhase != LOBBY_OFF ) ) {
		return -1;
	}
	trap_Cvar_VariableStringBuffer( "g_lobbyPicks", picks, sizeof( picks ) );
	p = picks;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			return -1;
		}
		if ( sscanf( token, "%i:%i", &client, &champion ) == 2 && client == clientNum
			&& champion >= 0 && champion < bg_numChampions ) {
			return champion;
		}
	}
}

/*
=================
G_LobbyActive

True while the lobby holds the match back
=================
*/
qboolean G_LobbyActive( void ) {
	return lobbyPhase != LOBBY_OFF;
}

/*
=================
G_LobbySend
=================
*/
static void G_LobbySend( void ) {
	static char	last[MAX_STRING_CHARS];
	char		text[MAX_STRING_CHARS];
	int			votes[MAPVOTE_CHOICES], i, mode = G_LobbyPickMode();
	gclient_t	*cl;

	if ( lobbyPhase == LOBBY_OFF ) {
		text[0] = 0;
	} else {
		memset( votes, 0, sizeof( votes ) );
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if ( level.clients[i].pers.connected == CON_CONNECTED && mapVoteOf[i] > 0 && mapVoteOf[i] <= mapVoteCount ) {
				votes[mapVoteOf[i] - 1]++;
			}
		}
		Com_sprintf( text, sizeof( text ), "%i %i %i %i %s %s %s %i %i %i", lobbyPhase, lobbyEnd, mode, lobbyWinner,
			mapVoteCount > 0 ? mapVoteMaps[0] : "-", mapVoteCount > 1 ? mapVoteMaps[1] : "-",
			mapVoteCount > 2 ? mapVoteMaps[2] : "-", votes[0], votes[1], votes[2] );
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			cl = &level.clients[i];
			if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR ) {
				continue;
			}
			Q_strcat( text, sizeof( text ), va( " %i:%i:%i", i,
				G_LobbyHidesPicks() ? -1 : cl->pers.champion, lobbyLocked[i] ) );
		}
	}
	if ( strcmp( text, last ) ) {
		Q_strncpyz( last, text, sizeof( last ) );
		trap_SetConfigstring( CS_LOBBY, text );
	}
}

/*
=================
G_LobbyInit

At every map load: a new lobby, the champion phase after a map change,
or nothing once the match has begun
=================
*/
void G_LobbyInit( int restart ) {
	char	state[16];

	lobbyPhase = LOBBY_OFF;
	lobbyWinner = 0;
	memset( lobbyLocked, 0, sizeof( lobbyLocked ) );
	trap_SetConfigstring( CS_LOBBY, "" );
	trap_Cvar_VariableStringBuffer( "g_lobbyState", state, sizeof( state ) );
	// a real map load (not the warm-up's restart) ends a finished lobby's
	// hold, however the last match ended (vote, nextmap, map, abort); the
	// lobby's own change of map comes with "pick"
	if ( !restart && !Q_stricmp( state, "done" ) ) {
		trap_Cvar_Set( "g_lobbyState", "" );
		trap_Cvar_Set( "g_lobbyPicks", "" );
		state[0] = 0;
	}
	if ( !g_lobby.integer || g_gametype.integer == GT_SINGLE_PLAYER ) {
		return;
	}
	if ( !Q_stricmp( state, "done" ) ) {
		return;
	}
	lobbyPhase = Q_stricmp( state, "pick" ) ? LOBBY_WAIT : LOBBY_PICK;
	lobbyEnd = 0;		// the pick's clock starts with the first player
	G_LobbySend();
}

/*
=================
G_LobbyClientGone

A slot that empties (or is taken by a new client) loses its lock and any
held pick
=================
*/
void G_LobbyClientGone( int clientNum ) {
	char	picks[MAX_CVAR_VALUE_STRING], out[MAX_CVAR_VALUE_STRING], *p, *token;
	int		client, champion;

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}
	lobbyLocked[clientNum] = qfalse;
	mapVoteOf[clientNum] = 0;
	trap_Cvar_VariableStringBuffer( "g_lobbyPicks", picks, sizeof( picks ) );
	if ( !picks[0] ) {
		return;
	}
	out[0] = 0;
	p = picks;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		if ( sscanf( token, "%i:%i", &client, &champion ) == 2 && client != clientNum ) {
			Q_strcat( out, sizeof( out ), va( "%i:%i ", client, champion ) );
		}
	}
	trap_Cvar_Set( "g_lobbyPicks", out );
}

/*
=================
G_LobbyEndMatch

The intermission: the next match gets a lobby of its own
=================
*/
void G_LobbyEndMatch( void ) {
	if ( g_lobby.integer ) {
		trap_Cvar_Set( "g_lobbyState", "" );
		trap_Cvar_Set( "g_lobbyPicks", "" );
	}
}

/*
=================
Cmd_LobbyLock_f
=================
*/
void Cmd_LobbyLock_f( gentity_t *ent ) {
	if ( lobbyPhase == LOBBY_PICK ) {
		lobbyLocked[ent - g_entities] = qtrue;
		G_LobbySend();
	}
}

/*
=================
G_LobbyThink
=================
*/
void G_LobbyThink( void ) {
	int			i, humans = 0, locked = 0;
	gclient_t	*cl;
	char		current[MAX_QPATH];

	// classic duel: a duellist who came in after the reveal, before the
	// match, has no held pick: both pick again, blind
	if ( g_lobby.integer && G_LobbyPickMode() == 1 && ( lobbyPhase == LOBBY_START
		|| ( lobbyPhase == LOBBY_OFF && level.warmupTime != 0 && !level.intermissiontime ) ) ) {
		char	picks[MAX_CVAR_VALUE_STRING];

		trap_Cvar_VariableStringBuffer( "g_lobbyPicks", picks, sizeof( picks ) );
		if ( picks[0] ) {
			for ( i = 0 ; i < level.maxclients ; i++ ) {
				cl = &level.clients[i];
				if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR
					&& G_LobbyFrozenChampion( i ) < 0 ) {
					break;
				}
			}
			if ( i < level.maxclients ) {
				trap_Cvar_Set( "g_lobbyPicks", "" );
				trap_Cvar_Set( "g_lobbyState", "pick" );
				memset( lobbyLocked, 0, sizeof( lobbyLocked ) );
				lobbyPhase = LOBBY_PICK;
				lobbyEnd = 0;
			}
		}
	}

	if ( lobbyPhase == LOBBY_OFF ) {
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			lobbyLocked[i] = qtrue;		// bots pick at once
			continue;
		}
		humans++;
		locked += lobbyLocked[i];
	}

	switch ( lobbyPhase ) {
	case LOBBY_WAIT:
		if ( humans ) {
			lobbyPhase = LOBBY_SOON;
			lobbyEnd = level.time + LOBBY_SOON_MSEC;
			memset( mapVoteOf, 0, sizeof( mapVoteOf ) );
			G_MapVoteChoose();
		}
		break;
	case LOBBY_SOON:
		if ( level.time >= lobbyEnd ) {
			lobbyPhase = mapVoteCount ? LOBBY_VOTE : LOBBY_PICK;
			lobbyEnd = mapVoteCount ? level.time + g_lobbyVoteTime.integer * 1000 : 0;
		}
		break;
	case LOBBY_VOTE:
		if ( level.time >= lobbyEnd ) {
			const char	*winner = G_MapVoteWinner();

			lobbyWinner = 0;
			for ( i = 0 ; i < mapVoteCount ; i++ ) {
				if ( winner && !Q_stricmp( winner, mapVoteMaps[i] ) ) {
					lobbyWinner = i + 1;
				}
			}
			lobbyPhase = LOBBY_REVEAL;
			lobbyEnd = level.time + LOBBY_REVEAL_MSEC;
		}
		break;
	case LOBBY_REVEAL:
		if ( level.time >= lobbyEnd ) {
			trap_Cvar_VariableStringBuffer( "mapname", current, sizeof( current ) );
			if ( lobbyWinner && Q_stricmp( mapVoteMaps[lobbyWinner - 1], current ) ) {
				// the champion phase goes on on the winning map
				trap_Cvar_Set( "g_lobbyState", "pick" );
				trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", mapVoteMaps[lobbyWinner - 1] ) );
				lobbyPhase = LOBBY_OFF;		// (the map load starts it again)
				return;
			}
			lobbyPhase = LOBBY_PICK;
			lobbyEnd = 0;
		}
		break;
	case LOBBY_PICK:
		if ( !humans ) {
			break;
		}
		if ( !lobbyEnd ) {
			if ( G_LobbyPickMode() == 2 ) {
				lobbyEnd = level.time;		// nothing to pick here
			} else {
				lobbyEnd = level.time + ( G_LobbyPickMode() == 1 ? LOBBY_DUEL_PICK : g_lobbyPickTime.integer * 1000 );
			}
		}
		// the time runs out, or everyone has locked in
		if ( level.time >= lobbyEnd || locked == humans ) {
			for ( i = 0 ; i < level.maxclients ; i++ ) {
				lobbyLocked[i] = qtrue;		// the current choice stays
			}
			lobbyPhase = LOBBY_START;
			lobbyEnd = level.time + LOBBY_START_MSEC;
			// a blind pick is held from here to the match's end and shown to
			// all now
			if ( G_LobbyPickMode() == 1 ) {
				char	picks[MAX_CVAR_VALUE_STRING];

				picks[0] = 0;
				for ( i = 0 ; i < level.maxclients ; i++ ) {
					cl = &level.clients[i];
					if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR ) {
						Q_strcat( picks, sizeof( picks ), va( "%i:%i ", i, cl->pers.champion ) );
					}
				}
				trap_Cvar_Set( "g_lobbyPicks", picks );
				for ( i = 0 ; i < level.maxclients ; i++ ) {
					if ( level.clients[i].pers.connected == CON_CONNECTED ) {
						ClientUserinfoChanged( i );
					}
				}
			}
		}
		break;
	case LOBBY_START:
		if ( level.time >= lobbyEnd ) {
			lobbyPhase = LOBBY_OFF;
			trap_Cvar_Set( "g_lobbyState", "done" );
		}
		break;
	}
	G_LobbySend();
}
