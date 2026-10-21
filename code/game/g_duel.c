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
// g_duel.c -- the classic timed duel: readyup and countdown, overtime,
// rated spawn points and a champion fixed for the match

#include "g_local.h"

#define DUEL_SPAWN_SPOTS		128
#define DUEL_SPAWN_KEEP_MIN		3
#define DUEL_SPAWN_KEEP_MAX		26
#define DUEL_SPAWN_CLEARANCE	64
#define DUEL_READY_HINT_MSEC	5000

/*
=================
G_DuelPlayersReady

True once both players have typed readyup (bots always are), or the
server said allready; then the countdown may start
=================
*/
qboolean G_DuelPlayersReady( void ) {
	gclient_t	*cl;
	qboolean	ready = qtrue;
	int			i;

	if ( !g_duelReadyUp.integer || level.duelForceReady ) {
		return qtrue;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != TEAM_FREE ) {
			continue;
		}
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			continue;
		}
		if ( !cl->pers.ready ) {
			ready = qfalse;
		}
	}

	// remind whoever is not ready yet
	if ( !ready && level.numPlayingClients == 2 && level.time >= level.duelReadyHintTime ) {
		level.duelReadyHintTime = level.time + DUEL_READY_HINT_MSEC;
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			cl = &level.clients[i];
			if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_FREE
				&& !cl->pers.ready && !( g_entities[i].r.svFlags & SVF_BOT ) ) {
				trap_SendServerCommand( i, va( "cp \"%s\"", BG_String( STR_DUEL_TYPE_READYUP ) ) );
			}
		}
	}
	return ready;
}

/*
=================
Cmd_ReadyUp_f

A player in warmup says they are ready, or no longer ready
=================
*/
void Cmd_ReadyUp_f( gentity_t *ent ) {
	gclient_t	*cl = ent->client;
	char		msg[MAX_STRING_CHARS];

	if ( g_gametype.integer != GT_TOURNAMENT || level.warmupTime >= 0 || cl->sess.sessionTeam != TEAM_FREE ) {
		return;
	}
	cl->pers.ready = !cl->pers.ready;
	Com_sprintf( msg, sizeof( msg ), BG_String( cl->pers.ready ? STR_DUEL_READY : STR_DUEL_NOT_READY ), cl->pers.netname );
	trap_SendServerCommand( -1, va( "print \"%s\n\"", msg ) );
	trap_SendServerCommand( -1, va( "cp \"%s\"", msg ) );
}

/*
=================
Svcmd_AllReady_f

Server: start the countdown without waiting for readyup
=================
*/
void Svcmd_AllReady_f( void ) {
	if ( g_gametype.integer == GT_TOURNAMENT && level.warmupTime < 0 ) {
		level.duelForceReady = qtrue;
	}
}

/*
=================
Svcmd_Abort_f

Server: stop a countdown or a running match and go back to warmup
=================
*/
void Svcmd_Abort_f( void ) {
	int		i;

	if ( g_gametype.integer != GT_TOURNAMENT ) {
		return;
	}
	trap_SendServerCommand( -1, va( "print \"%s\n\"", BG_String( STR_DUEL_ABORTED ) ) );
	if ( level.warmupTime > 0 ) {
		// still counting down
		level.warmupTime = -1;
		level.duelForceReady = qfalse;
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			level.clients[i].pers.ready = qfalse;
		}
		trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
		return;
	}
	if ( level.warmupTime == 0 ) {
		// a restart without the restarted flag comes back up in warmup
		trap_Cvar_Set( "g_restarted", "0" );
		trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
	}
}

/*
=================
G_DuelCheckTime

The time limit (10 minutes when none is set). A tie when it runs out adds
an overtime period, or with g_overtime 0 waits for the next frag.
True when the exit rules need go no further this frame.
=================
*/
qboolean G_DuelCheckTime( void ) {
	char	msg[MAX_STRING_CHARS];
	int		limit, minutes;

	if ( level.warmupTime ) {
		return qfalse;
	}
	minutes = g_timelimit.integer > 0 ? g_timelimit.integer : 10;
	limit = minutes * 60000 + level.overtimes * ( g_overtime.integer > 0 ? g_overtime.integer : 0 ) * 1000;
	if ( level.time - level.startTime < limit ) {
		return qfalse;
	}

	if ( !ScoreIsTied() ) {
		trap_SendServerCommand( -1, "print \"Timelimit hit.\n\"" );
		LogExit( "Timelimit hit." );
		return qtrue;
	}

	if ( g_overtime.integer > 0 ) {
		level.overtimes++;
		Com_sprintf( msg, sizeof( msg ), BG_String( STR_DUEL_OVERTIME ), ( g_overtime.integer + 59 ) / 60 );
		trap_SendServerCommand( -1, va( "cp \"%s\"", msg ) );
		trap_SendServerCommand( -1, va( "print \"%s\n\"", msg ) );
	} else if ( !level.overtimes ) {
		level.overtimes = 1;
		trap_SendServerCommand( -1, va( "cp \"%s\"", BG_String( STR_DUEL_SUDDEN_DEATH ) ) );
		trap_SendServerCommand( -1, va( "print \"%s\n\"", BG_String( STR_DUEL_SUDDEN_DEATH ) ) );
	}
	return qtrue;
}

/*
=================
G_DuelCheckFraglimit

Duel has no frag limit unless g_duelFraglimit sets one
=================
*/
void G_DuelCheckFraglimit( void ) {
	gclient_t	*cl;
	int			i;

	if ( g_duelFraglimit.integer <= 0 || level.warmupTime ) {
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != TEAM_FREE ) {
			continue;
		}
		if ( cl->ps.persistant[PERS_SCORE] >= g_duelFraglimit.integer ) {
			LogExit( "Fraglimit hit." );
			trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " hit the fraglimit.\n\"", cl->pers.netname ) );
			return;
		}
	}
}

/*
=================
G_DuelSpawnChampion

The champion to spawn as: any pick during warmup, the first one of the
match once it is running
=================
*/
int G_DuelSpawnChampion( gclient_t *client ) {
	if ( g_gametype.integer != GT_TOURNAMENT || level.warmupTime ) {
		client->pers.championLocked = qfalse;
		return client->pers.champion;
	}
	if ( !client->pers.championLocked ) {
		client->pers.championLocked = qtrue;
		client->pers.lockedChampion = client->pers.champion;
	} else if ( client->pers.champion != client->pers.lockedChampion ) {
		trap_SendServerCommand( client - level.clients, va( "print \"%s\n\"", BG_String( STR_DUEL_CHAMPION_LOCKED ) ) );
		client->pers.champion = client->pers.lockedChampion;
	}
	return client->pers.lockedChampion;
}

/*
=================
G_SelectDuelSpawnPoint

Spots are rated by how far they are from the opponent (the largest of the
x, y and z gaps); the safer half is kept, at least 3 and at most 26, spots
right next to a player are left out, and one is picked at random. The
first spawn of a match uses the spots the map marks as initial.
=================
*/
gentity_t *G_SelectDuelSpawnPoint( gentity_t *ent, qboolean initial, vec3_t origin, vec3_t angles, qboolean isbot ) {
	gentity_t	*spot, *list[DUEL_SPAWN_SPOTS];
	float		rating[DUEL_SPAWN_SPOTS], r, d, t;
	gentity_t	*opponent = NULL, *other;
	int			num, keep, i, j, pass;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( other != ent && other->inuse && other->client && other->client->sess.sessionTeam == TEAM_FREE
			&& other->client->pers.connected == CON_CONNECTED && other->health > 0 ) {
			opponent = other;
			break;
		}
	}

	num = 0;
	for ( pass = initial ? 0 : 1 ; pass < 2 && !num ; pass++ ) {
		spot = NULL;
		while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL && num < DUEL_SPAWN_SPOTS ) {
			if ( pass == 0 && !( spot->spawnflags & 1 ) ) {
				continue;
			}
			if ( ( ( spot->flags & FL_NO_BOTS ) && isbot ) || ( ( spot->flags & FL_NO_HUMANS ) && !isbot ) ) {
				continue;
			}
			if ( SpotWouldTelefrag( spot ) ) {
				continue;
			}

			// never right next to a player
			for ( j = 0 ; j < level.maxclients ; j++ ) {
				other = &g_entities[j];
				if ( other != ent && other->inuse && other->client && other->health > 0
					&& other->client->sess.sessionTeam == TEAM_FREE
					&& Distance( other->r.currentOrigin, spot->s.origin ) < DUEL_SPAWN_CLEARANCE ) {
					break;
				}
			}
			if ( j < level.maxclients ) {
				continue;
			}

			r = 0;
			if ( opponent ) {
				for ( j = 0 ; j < 3 ; j++ ) {
					d = fabs( spot->s.origin[j] - opponent->r.currentOrigin[j] );
					if ( d > r ) {
						r = d;
					}
				}
			}

			// insert, safest first
			for ( i = num ; i > 0 && rating[i - 1] < r ; i-- ) {
				rating[i] = rating[i - 1];
				list[i] = list[i - 1];
			}
			rating[i] = r;
			list[i] = spot;
			num++;
		}
	}

	if ( !num ) {
		return SelectSpawnPoint( ent->client->ps.origin, origin, angles, isbot );
	}

	keep = num / 2;
	if ( keep < DUEL_SPAWN_KEEP_MIN ) {
		keep = DUEL_SPAWN_KEEP_MIN;
	}
	if ( keep > DUEL_SPAWN_KEEP_MAX ) {
		keep = DUEL_SPAWN_KEEP_MAX;
	}
	if ( keep > num ) {
		keep = num;
	}
	t = random() * keep;
	i = (int)t;
	if ( i >= keep ) {
		i = keep - 1;
	}

	VectorCopy( list[i]->s.origin, origin );
	origin[2] += 9;
	VectorCopy( list[i]->s.angles, angles );
	return list[i];
}
