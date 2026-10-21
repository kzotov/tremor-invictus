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
void G_MapVoteStart( void ) {
	char		pool[64][MAX_QPATH];
	char		current[MAX_QPATH];
	char		list[MAX_CVAR_VALUE_STRING];
	char		*p, *token;
	int			numPool = 0, i, j;
	const char	*info;

	mapVoteCount = 0;
	mapVoteEnd = 0;
	memset( mapVoteOf, 0, sizeof( mapVoteOf ) );
	if ( !g_mapVote.integer ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "mapname", current, sizeof( current ) );

	trap_Cvar_VariableStringBuffer( "g_mapPool", list, sizeof( list ) );
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
	mapVoteEnd = level.intermissiontime + g_mapVoteDelay.integer + g_mapVoteTime.integer * 1000;
	G_MapVoteSend();
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
void Cmd_MapVote_f( gentity_t *ent ) {
	char	arg[8];
	int		n, clientNum = ent - g_entities;

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
