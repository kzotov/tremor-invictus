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
// g_ctf.c -- capture the flag as a match: two halves with the teams
// swapping bases at half time, sudden death on a tied score, and
// dropping or throwing a carried flag

#include "g_local.h"

#define CTF_HALF_TIME_MSEC		10000
#define CTF_THROW_SPEED			400
#define CTF_THROW_UP			200

/*
=================
G_DropCarriedFlag

Lets go of a carried flag: thrown forward (dropflag), or left where the
carrier is (a teleport). The thrower can't catch it again for a second.
=================
*/
void G_DropCarriedFlag( gentity_t *ent, qboolean thrown ) {
	gclient_t	*cl = ent->client;
	gitem_t		*item;
	gentity_t	*dropped;
	vec3_t		velocity, forward, origin;
	int			pw;

	if ( G_Sacrifice() ) {
		G_SacrificeDropSoul( ent, thrown );
		return;
	}

	if ( !cl || g_gametype.integer != GT_CTF ) {
		return;
	}
	if ( cl->ps.powerups[PW_REDFLAG] ) {
		pw = PW_REDFLAG;
	} else if ( cl->ps.powerups[PW_BLUEFLAG] ) {
		pw = PW_BLUEFLAG;
	} else {
		return;
	}
	item = BG_FindItemForPowerup( pw );
	if ( !item ) {
		return;
	}

	VectorCopy( cl->ps.origin, origin );
	if ( thrown ) {
		AngleVectors( cl->ps.viewangles, forward, NULL, NULL );
		origin[2] += cl->ps.viewheight;
		VectorScale( forward, CTF_THROW_SPEED, velocity );
		velocity[2] += CTF_THROW_UP;
	} else {
		VectorClear( velocity );
	}

	cl->ps.powerups[pw] = 0;
	dropped = LaunchItem( item, origin, velocity );
	dropped->enemy = ent;
	dropped->timestamp = level.time;
}

/*
=================
Cmd_DropFlag_f
=================
*/
void Cmd_DropFlag_f( gentity_t *ent ) {
	if ( ent->health <= 0 || level.halfTimeEnd ) {
		return;
	}
	G_DropCarriedFlag( ent, qtrue );
}

/*
=================
G_ResetMapItems

Every item back as at the start of the match, power-ups counting down to
their first appearance again (CTF half time, sacrifice rounds)
=================
*/
void G_ResetMapItems( void ) {
	gentity_t	*ent;
	int			i;

	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse || !ent->item || ent->item->giType == IT_TEAM ) {
			continue;
		}
		if ( ent->flags & FL_DROPPED_ITEM ) {
			G_FreeEntity( ent );
			continue;
		}
		if ( ent->item->giType == IT_POWERUP ) {
			G_SchedulePowerup( ent, g_powerupFirstSpawn.integer );
			continue;
		}
		if ( ent->think == RespawnItem ) {
			RespawnItem( ent );
		}
	}
}

/*
=================
G_CTFSwapHalves

The teams swap bases: everyone changes side, keeping their own score,
the team scores go with them, and everyone respawns in the new base
=================
*/
static void G_CTFSwapHalves( void ) {
	gentity_t	*ent;
	gclient_t	*cl;
	int			i, score;

	Team_ResetFlags();
	G_ResetMapItems();

	score = level.teamScores[TEAM_RED];
	level.teamScores[TEAM_RED] = level.teamScores[TEAM_BLUE];
	level.teamScores[TEAM_BLUE] = score;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		cl = &level.clients[i];
		if ( !ent->inuse || cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl->sess.sessionTeam == TEAM_RED ) {
			cl->sess.sessionTeam = TEAM_BLUE;
		} else if ( cl->sess.sessionTeam == TEAM_BLUE ) {
			cl->sess.sessionTeam = TEAM_RED;
		} else {
			continue;
		}
		cl->ps.powerups[PW_REDFLAG] = cl->ps.powerups[PW_BLUEFLAG] = 0;
		cl->pers.teamState.state = TEAM_BEGIN;
		ClientUserinfoChanged( i );
		ClientSpawn( ent );
	}
	CalculateRanks();
	G_EventLine( -1, 1, BG_String( STR_CTF_SECOND_HALF ) );
}

/*
=================
G_CTFCheckRules

Halves of g_ctfHalfLength minutes with a break between them; at the end a
tied score waits for the next capture. The capture limit, when set, ends
the match at once.
=================
*/
void G_CTFCheckRules( void ) {
	char	msg[MAX_STRING_CHARS];
	int		half, elapsed;

	if ( level.warmupTime ) {
		return;
	}

	if ( g_ctfCapturelimit.integer > 0 ) {
		if ( level.teamScores[TEAM_RED] >= g_ctfCapturelimit.integer ) {
			trap_SendServerCommand( -1, "print \"Red hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}
		if ( level.teamScores[TEAM_BLUE] >= g_ctfCapturelimit.integer ) {
			trap_SendServerCommand( -1, "print \"Blue hit the capturelimit.\n\"" );
			LogExit( "Capturelimit hit." );
			return;
		}
	}

	half = ( g_ctfHalfLength.integer > 0 ? g_ctfHalfLength.integer : 7 ) * 60000;
	elapsed = level.time - level.startTime;

	// half time: a break, then the swap
	if ( level.ctfHalf < 2 && !level.halfTimeEnd && elapsed >= half ) {
		level.halfTimeEnd = level.time + CTF_HALF_TIME_MSEC;
		Com_sprintf( msg, sizeof( msg ), BG_String( STR_CTF_HALF_TIME ), level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
		G_EventLine( -1, 1, msg );
		G_Announce( -1, "halftime" );
		trap_SendServerCommand( -1, va( "print \"%s\n\"", msg ) );
		return;
	}
	if ( level.halfTimeEnd ) {
		if ( level.time >= level.halfTimeEnd ) {
			level.halfTimeEnd = 0;
			level.ctfHalf = 2;
			G_CTFSwapHalves();
		}
		return;
	}

	// the end: two halves and the break between them
	if ( level.ctfHalf == 2 && elapsed >= 2 * half + CTF_HALF_TIME_MSEC ) {
		if ( ScoreIsTied() ) {
			return;		// sudden death: the next capture wins
		}
		trap_SendServerCommand( -1, "print \"Timelimit hit.\n\"" );
		LogExit( "Timelimit hit." );
	}
}
