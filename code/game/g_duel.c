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
// rated spawn points and a champion fixed for the match. Team deathmatch
// shares the warmup, readyup, respawn and spawn rules.

#include "g_local.h"

#define WARMUP_AMMO		200		// warm-up: every weapon kept full

#define DUEL_SPAWN_SPOTS		128
#define DUEL_SPAWN_KEEP_MIN		3
#define DUEL_SPAWN_KEEP_MAX		26
#define DUEL_SPAWN_CLEARANCE	64
#define SPAWN_TEAMMATE_SCALE	1.5f	// teammates count this much further away than enemies

/*
=================
G_MatchMode

Modes played as matches: warmup with readyup, a countdown, and the
respawn and spawn rules below
=================
*/
qboolean G_MatchMode( void ) {
	return g_gametype.integer == GT_TOURNAMENT || g_gametype.integer == GT_TEAM || g_gametype.integer == GT_CTF;
}

/*
=================
G_MatchTimeLimit

Minutes; duel and TDM run 10 when no timelimit is set
=================
*/
int G_MatchTimeLimit( void ) {
	if ( g_timelimit.integer > 0 ) {
		return g_timelimit.integer;
	}
	return G_MatchMode() || g_gametype.integer == GT_FFA ? 10 : 0;
}

/*
=================
G_MaxPlayers

Players allowed in the game (0 = no limit); free-for-all holds 8 unless set
=================
*/
int G_MaxPlayers( void ) {
	if ( g_maxGameClients.integer > 0 ) {
		return g_maxGameClients.integer;
	}
	return g_gametype.integer == GT_FFA ? g_ffaMaxPlayers.integer : 0;
}

/*
=================
G_MatchFragLimit

Duel and TDM have frag limits of their own (none and 75)
=================
*/
int G_MatchFragLimit( void ) {
	if ( g_gametype.integer == GT_TOURNAMENT ) {
		return g_duelFraglimit.integer;
	}
	if ( g_gametype.integer == GT_TEAM ) {
		return g_tdmFraglimit.integer;
	}
	if ( g_gametype.integer == GT_FFA && g_fraglimit.integer <= 0 ) {
		return g_ffaFraglimit.integer;
	}
	return g_fraglimit.integer;
}

/*
=================
G_WarmupPractice

A match mode's warm-up and countdown: moving only, no damage or items
=================
*/
qboolean G_WarmupPractice( void ) {
	return !g_warmupDamage.integer && G_MatchMode() && level.warmupTime != 0 && !level.intermissiontime;
}

/*
=================
G_WarmupLoadout

Warm-up: every weapon the mode has, with endless ammo, to practise moving
=================
*/
void G_WarmupLoadout( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	int			w;

	if ( g_instagib.integer ) {
		return;		// the arcade loadout is the mode's whole loadout
	}
	for ( w = WP_GAUNTLET; w < WP_NUM_WEAPONS; w++ ) {
		if ( w == WP_GRAPPLING_HOOK || w == WP_BFG ) {
			continue;
		}
		client->ps.stats[STAT_WEAPONS] |= 1 << w;
		client->ps.ammo[w] = WARMUP_AMMO;
	}
}

/*
=================
G_WarmupAmmo

Keeps every carried weapon full during warm-up
=================
*/
void G_WarmupAmmo( gentity_t *ent ) {
	int w;

	for ( w = WP_MACHINEGUN; w < WP_NUM_WEAPONS; w++ ) {
		if ( ent->client->ps.stats[STAT_WEAPONS] & ( 1 << w ) ) {
			ent->client->ps.ammo[w] = WARMUP_AMMO;
		}
	}
}

/*
=================
Cmd_WarmupKill

kill in warm-up: back to a spawn point, no death counted
=================
*/
qboolean G_WarmupKill( gentity_t *ent ) {
	if ( !G_WarmupPractice() || ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return qfalse;
	}
	ClientSpawn( ent );
	return qtrue;
}

/*
=================
G_ReadyShare

Percent of players that must be ready: g_readyUpShare, or 100 in duel and
75 in team modes when it is 0
=================
*/
int G_ReadyShare( void ) {
	if ( g_readyUpShare.integer > 0 ) {
		return g_readyUpShare.integer > 100 ? 100 : g_readyUpShare.integer;
	}
	return g_gametype.integer == GT_TOURNAMENT ? 100 : 75;
}

/*
=================
G_WarmupPublish

"readyUp share ready players needed startTime" for the warm-up HUD in
CS_READYUP; startTime is the level time a timed start or g_warmupMax ends,
0 when none
=================
*/
static void G_WarmupPublish( int ready, int players, int needed, int startTime ) {
	char	text[64];

	Com_sprintf( text, sizeof( text ), "%i %i %i %i %i %i", g_readyUp.integer, G_ReadyShare(), ready, players, needed,
		startTime );
	if ( strcmp( text, level.readyUpString ) ) {
		Q_strncpyz( level.readyUpString, text, sizeof( level.readyUpString ) );
		trap_SetConfigstring( CS_READYUP, text );
	}
}

/*
=================
G_DuelPlayersReady

True once every player has typed readyup (bots always are), or the
server said allready; then the countdown may start
=================
*/
qboolean G_DuelPlayersReady( void ) {
	gclient_t	*cl;
	int			i, ready = 0, humans = 0, needed, bots = 0, startTime = 0;
	int			readyTeam[TEAM_NUM_TEAMS];
	qboolean	go;

	memset( readyTeam, 0, sizeof( readyTeam ) );
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			bots++;
			readyTeam[cl->sess.sessionTeam]++;
			continue;
		}
		humans++;
		if ( cl->pers.ready ) {
			ready++;
			readyTeam[cl->sess.sessionTeam]++;
		}
	}
	needed = ( humans * G_ReadyShare() + 99 ) / 100;

	// the wait for the start: a timed start, or the longest warm-up
	if ( level.numPlayingClients < 2 ) {
		level.readyWaitStart = 0;
	} else if ( !level.readyWaitStart ) {
		level.readyWaitStart = level.time;
	}
	if ( level.readyWaitStart ) {
		if ( !g_readyUp.integer ) {
			startTime = level.readyWaitStart + g_warmupTime.integer * 1000;
		} else if ( g_warmupMax.integer > 0 ) {
			startTime = level.readyWaitStart + g_warmupMax.integer * 1000;
		}
	}

	if ( level.duelForceReady ) {
		go = qtrue;
	} else if ( !g_readyUp.integer ) {
		go = level.readyWaitStart && level.time >= startTime;
	} else {
		go = ready >= needed;
		// team modes: a ready player on each team
		if ( g_gametype.integer >= GT_TEAM && ( !readyTeam[TEAM_RED] || !readyTeam[TEAM_BLUE] ) ) {
			go = qfalse;
		}
		if ( startTime && level.time >= startTime ) {
			go = qtrue;
		}
	}
	G_WarmupPublish( ready, humans, needed, startTime );

	// the scoreboard's ready ticks
	{
		int bits = 0;

		for ( i = 0 ; i < level.maxclients && i < 32 ; i++ ) {
			cl = &level.clients[i];
			if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR
				&& ( cl->pers.ready || ( g_entities[i].r.svFlags & SVF_BOT ) ) ) {
				bits |= 1 << i;
			}
		}
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if ( level.clients[i].pers.connected == CON_CONNECTED ) {
				level.clients[i].ps.stats[STAT_CLIENTS_READY] = bits;
			}
		}
	}

	return go;
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

	if ( !G_MatchMode() || level.warmupTime >= 0 || cl->sess.sessionTeam == TEAM_SPECTATOR ) {
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
	if ( G_MatchMode() && level.warmupTime < 0 ) {
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

	if ( !G_MatchMode() ) {
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
	minutes = G_MatchTimeLimit();
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
		G_Announce( -1, "overtime" );
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

	if ( G_MatchFragLimit() <= 0 || level.warmupTime ) {
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != TEAM_FREE ) {
			continue;
		}
		if ( cl->ps.persistant[PERS_SCORE] >= G_MatchFragLimit() ) {
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
	if ( G_RoundDuel() && !level.warmupTime ) {
		return G_RoundDuelChampion( client );
	}
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
G_SpawnRating

How safe a spot is: the distance to the nearest living player (the largest
of the x, y and z gaps), with teammates counting 1.5 times further away
=================
*/
static float G_SpawnRating( gentity_t *ent, gentity_t *spot ) {
	gentity_t	*other;
	float		best = -1, r, d;
	int			i, j;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( other == ent || !other->inuse || !other->client || other->health <= 0
			|| other->client->pers.connected != CON_CONNECTED
			|| other->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		r = 0;
		for ( j = 0 ; j < 3 ; j++ ) {
			d = fabs( spot->s.origin[j] - other->r.currentOrigin[j] );
			if ( d > r ) {
				r = d;
			}
		}
		if ( OnSameTeam( ent, other ) ) {
			r *= SPAWN_TEAMMATE_SCALE;
		}
		if ( best < 0 || r < best ) {
			best = r;
		}
	}
	return best < 0 ? 0 : best;
}

/*
=================
G_SelectDuelSpawnPoint

Spots are rated as above; the safer half is kept, at least 3 and at most
26, spots right next to a player are left out, and one is picked at
random. The first spawn of a match uses the spots the map marks as
initial. In TDM, maps with red and blue spawns keep each team to its own.
=================
*/
gentity_t *G_SelectDuelSpawnPoint( gentity_t *ent, qboolean initial, vec3_t origin, vec3_t angles, qboolean isbot ) {
	gentity_t	*spot, *list[DUEL_SPAWN_SPOTS], *other;
	float		rating[DUEL_SPAWN_SPOTS], r, t;
	const char	*classname = "info_player_deathmatch";
	int			num, keep, i, j, pass;

	if ( g_gametype.integer == GT_TEAM ) {
		const char	*teamSpot = ent->client->sess.sessionTeam == TEAM_RED ? "team_CTF_redspawn" : "team_CTF_bluespawn";

		if ( G_Find( NULL, FOFS( classname ), teamSpot ) ) {
			classname = teamSpot;
			initial = qfalse;
		}
	}

	num = 0;
	for ( pass = initial ? 0 : 1 ; pass < 2 && !num ; pass++ ) {
		spot = NULL;
		while ( ( spot = G_Find( spot, FOFS( classname ), classname ) ) != NULL && num < DUEL_SPAWN_SPOTS ) {
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
					&& other->client->sess.sessionTeam != TEAM_SPECTATOR
					&& Distance( other->r.currentOrigin, spot->s.origin ) < DUEL_SPAWN_CLEARANCE ) {
					break;
				}
			}
			if ( j < level.maxclients ) {
				continue;
			}

			r = G_SpawnRating( ent, spot );

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

/*
=================
G_SendDuelHud

Twice a second, both duel players' health and armour to spectators, for
the bars at the top of their screen
=================
*/
void G_SendDuelHud( void ) {
	static int	lastTime;
	char		msg[128];
	gclient_t	*cl;
	int			i, n = 0;

	if ( g_gametype.integer != GT_TOURNAMENT || level.time - lastTime < 500 ) {
		if ( level.time < lastTime ) {
			lastTime = 0;
		}
		return;
	}
	lastTime = level.time;

	Q_strncpyz( msg, "dh", sizeof( msg ) );
	for ( i = 0 ; i < level.maxclients && n < 2 ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != TEAM_FREE ) {
			continue;
		}
		Q_strcat( msg, sizeof( msg ), va( " %i %i %i", i, cl->ps.stats[STAT_HEALTH], cl->ps.stats[STAT_ARMOR] ) );
		n++;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_SPECTATOR
			&& !( g_entities[i].r.svFlags & SVF_BOT ) ) {
			trap_SendServerCommand( i, msg );
		}
	}
}

/*
==============================================================================

TIMEOUTS

One 60 s timeout per player in duel and per team in team deathmatch.
Everything holds still: players are frozen, and every clock (the match
clock, item and power-up respawns, projectiles, movers, ability recharge,
held power-ups, respawn delays) is pushed forward frame by frame.

==============================================================================
*/

#define TIMEOUT_MSEC	60000

/*
=================
G_TimeoutAllowed
=================
*/
static qboolean G_TimeoutAllowed( void ) {
	const char	*mode = G_ModeName();

	return !strcmp( mode, "duel" ) || !strcmp( mode, "tdm" );
}

/*
=================
G_EndTimeout
=================
*/
static void G_EndTimeout( void ) {
	level.timeoutEnd = 0;
	trap_SetConfigstring( CS_TIMEOUT, "0" );
	trap_SetConfigstring( CS_LEVEL_START_TIME, va( "%i", level.startTime ) );
	G_UpdateItemTimers();
	trap_SendServerCommand( -1, va( "cp \"%s\n\"", BG_String( STR_TO_OVER ) ) );
}

/*
=================
G_TimeoutShift
=================
*/
static void G_TimeoutShift( int *time, int msec ) {
	if ( *time > 0 && *time < 0x7f000000 ) {
		*time += msec;
	}
}

/*
=================
G_TimeoutFrame

Called each frame of a timeout with the frame's length
=================
*/
void G_TimeoutFrame( int msec ) {
	gentity_t	*ent;
	gclient_t	*cl;
	int			i, p;

	if ( level.time >= level.timeoutEnd ) {
		G_EndTimeout();
		return;
	}

	for ( i = 0, ent = g_entities ; i < level.num_entities ; i++, ent++ ) {
		if ( !ent->inuse ) {
			continue;
		}
		G_TimeoutShift( &ent->nextthink, msec );
		if ( ent->s.pos.trType != TR_STATIONARY && ent->s.pos.trType != TR_INTERPOLATE ) {
			ent->s.pos.trTime += msec;
		}
		if ( ent->s.apos.trType != TR_STATIONARY && ent->s.apos.trType != TR_INTERPOLATE ) {
			ent->s.apos.trTime += msec;
		}
		cl = ent->client;
		if ( !cl ) {
			continue;
		}
		for ( p = PW_NONE + 1 ; p < PW_NUM_POWERUPS ; p++ ) {
			G_TimeoutShift( &cl->ps.powerups[p], msec );
		}
		G_TimeoutShift( &cl->respawnTime, msec );
		G_TimeoutShift( &cl->lastDamageTime, msec );
		G_TimeoutShift( &cl->burnEndTime, msec );
		G_TimeoutShift( &cl->burnNextTime, msec );
		G_TimeoutShift( &cl->poisonEndTime, msec );
		G_TimeoutShift( &cl->poisonNextTime, msec );
	}

	level.startTime += msec;
	if ( level.time - level.timeoutClockTime >= 1000 ) {
		level.timeoutClockTime = level.time;
		trap_SetConfigstring( CS_LEVEL_START_TIME, va( "%i", level.startTime ) );
	}
}

/*
=================
Cmd_Timeout_f
=================
*/
void Cmd_Timeout_f( gentity_t *ent ) {
	gclient_t	*cl = ent->client;
	int			team = cl->sess.sessionTeam;

	if ( !G_TimeoutAllowed() ) {
		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", BG_String( STR_TO_ONLYMODES ) ) );
		return;
	}
	if ( team == TEAM_SPECTATOR || level.timeoutEnd || level.warmupTime || level.intermissiontime ) {
		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", BG_String( STR_TO_NOTNOW ) ) );
		return;
	}
	if ( g_gametype.integer >= GT_TEAM ? level.teamTimeoutUsed[team] : cl->timeoutUsed ) {
		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", BG_String( STR_TO_USED ) ) );
		return;
	}
	if ( g_gametype.integer >= GT_TEAM ) {
		level.teamTimeoutUsed[team] = qtrue;
	} else {
		cl->timeoutUsed = qtrue;
	}
	level.timeoutEnd = level.time + TIMEOUT_MSEC;
	level.timeoutClockTime = level.time;
	trap_SetConfigstring( CS_TIMEOUT, va( "%i", level.timeoutEnd ) );
	trap_SendServerCommand( -1, va( "cp \"%s\n\"", va( (char *)BG_String( STR_TO_CALLED ), cl->pers.netname ) ) );
}

/*
=================
Cmd_Timein_f

Any player in the match may end the timeout early
=================
*/
void Cmd_Timein_f( gentity_t *ent ) {
	if ( !level.timeoutEnd || ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}
	level.timeoutEnd = level.time;
	G_EndTimeout();
}
