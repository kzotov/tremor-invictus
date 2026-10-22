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
// g_ca.c -- clan arena: team deathmatch in rounds, one life each, with
// everyone spawning on the full stack and no items on the map

#include "g_local.h"

#define CA_COUNTDOWN_MSEC	5000
#define CA_RESULT_MSEC		3000
#define CA_DAMAGE_PER_POINT	100

/*
=================
G_ClanArena
=================
*/
qboolean G_ClanArena( void ) {
	return g_gametype.integer == GT_TEAM && g_clanArena.integer;
}

/*
=================
G_CALoadout

The full stack: health and armour at the class's limits, every weapon at
its ammo cap, the rocket launcher in hand. Someone joining a round already
under way waits, out of play, for the next one.
=================
*/
void G_CALoadout( gentity_t *ent ) {
	gclient_t		*client = ent->client;
	const classInfo_t	*ci = G_ClassInfo( client );
	gitem_t			*item;

	if ( !level.warmupTime && !level.caSpawning && level.caState != CA_WAITING ) {
		ent->health = client->ps.stats[STAT_HEALTH] = 0;
		ent->takedamage = qfalse;
		ent->r.contents = 0;
		client->ps.eFlags |= EF_NODRAW;
		trap_UnlinkEntity( ent );
		return;
	}

	ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] + ci->maxHealthBonus;
	client->ps.stats[STAT_ARMOR] = G_MaxArmor( client );

	for ( item = bg_itemlist + 1 ; item->classname ; item++ ) {
		if ( item->giType != IT_WEAPON || item->giTag == WP_GRAPPLING_HOOK ) {
			continue;
		}
		client->ps.stats[STAT_WEAPONS] |= 1 << item->giTag;
		if ( item->giTag == WP_GAUNTLET ) {
			continue;
		}
		client->ps.ammo[item->giTag] = BG_WeaponTuned( item->giTag ) ? BG_WeaponParam( item->giTag, WPP_AMMOMAX ) : 200;
	}
	client->ps.weapon = WP_ROCKET_LAUNCHER;
	client->ps.weaponstate = WEAPON_READY;
	BG_SetAbility( &client->ps, ABILITY_IDLE, 0 );
}

/*
=================
G_CADamageDealt

A point for every 100 damage done to the other team
=================
*/
void G_CADamageDealt( gentity_t *attacker, gentity_t *targ, int damage ) {
	if ( level.warmupTime || level.caState != CA_FIGHTING || !attacker || !attacker->client
		|| !targ->client || attacker == targ || OnSameTeam( attacker, targ ) ) {
		return;
	}
	attacker->client->pers.caDamage += damage;
	while ( attacker->client->pers.caDamage >= CA_DAMAGE_PER_POINT ) {
		attacker->client->pers.caDamage -= CA_DAMAGE_PER_POINT;
		AddScore( attacker, targ->r.currentOrigin, 1 );
	}
}

/*
=================
G_CAStartRound

Everyone back to a spawn on the full stack, held there for the countdown
=================
*/
static void G_CAStartRound( void ) {
	char		msg[MAX_STRING_CHARS];
	gentity_t	*ent;
	int			i;

	level.caRound++;
	level.caState = CA_COUNTDOWN;
	level.caStateTime = level.time + CA_COUNTDOWN_MSEC;

	// leftover rockets and grenades from the last round
	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( ent->inuse && ent->s.eType == ET_MISSILE ) {
			G_FreeEntity( ent );
		}
	}

	level.caSpawning = qtrue;
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse || level.clients[i].pers.connected != CON_CONNECTED
			|| level.clients[i].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		ClientSpawn( ent );
	}
	level.caSpawning = qfalse;

	Com_sprintf( msg, sizeof( msg ), BG_String( STR_CA_ROUND ), level.caRound );
	trap_SendServerCommand( -1, va( "cp \"%s\"", msg ) );
	G_Announce( -1, "roundbegins" );
	level.caLastStanding[TEAM_RED] = level.caLastStanding[TEAM_BLUE] = qfalse;
}

/*
=================
G_CATeamStatus

Living players and their health plus armour, per team
=================
*/
static void G_CATeamStatus( int alive[TEAM_NUM_TEAMS], int stack[TEAM_NUM_TEAMS] ) {
	gentity_t	*ent;
	gclient_t	*cl;
	int			i;

	memset( alive, 0, sizeof( int ) * TEAM_NUM_TEAMS );
	memset( stack, 0, sizeof( int ) * TEAM_NUM_TEAMS );
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		cl = &level.clients[i];
		if ( !ent->inuse || cl->pers.connected != CON_CONNECTED || ent->health <= 0 ) {
			continue;
		}
		if ( cl->sess.sessionTeam != TEAM_RED && cl->sess.sessionTeam != TEAM_BLUE ) {
			continue;
		}
		alive[cl->sess.sessionTeam]++;
		stack[cl->sess.sessionTeam] += ent->health + cl->ps.stats[STAT_ARMOR];
	}
}

/*
=================
G_CAEndRound
=================
*/
static void G_CAEndRound( int winner ) {
	int		id, i;
	gclient_t	*cl;

	// the scoreboard counts the rounds each player lived through
	for ( i = 0; i < level.maxclients; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR
			&& cl->ps.stats[STAT_HEALTH] > 0 && cl->ps.pm_type != PM_DEAD ) {
			cl->pers.modeStat[0]++;
		}
	}

	// first, since the scores below check the rules again
	level.caState = CA_RESULT;
	level.caStateTime = level.time + CA_RESULT_MSEC;

	if ( winner == TEAM_RED || winner == TEAM_BLUE ) {
		level.teamScores[winner]++;
		CalculateRanks();
		id = winner == TEAM_RED ? STR_CA_RED_WINS : STR_CA_BLUE_WINS;
	} else {
		id = STR_CA_DRAW;
	}
	trap_SendServerCommand( -1, va( "cp \"%s\"", BG_String( id ) ) );
	trap_SendServerCommand( -1, va( "print \"%s\n\"", BG_String( id ) ) );
	G_Announce( -1, winner == TEAM_RED ? "redwinsround" : winner == TEAM_BLUE ? "bluewinsround" : "rounddraw" );
}

/*
=================
G_CALastStanding

Tells a team's only survivor, once a round
=================
*/
static void G_CALastStanding( const int alive[TEAM_NUM_TEAMS] ) {
	int		team, i;

	for ( team = TEAM_RED ; team <= TEAM_BLUE ; team++ ) {
		if ( alive[team] != 1 || level.caLastStanding[team] ) {
			continue;
		}
		level.caLastStanding[team] = qtrue;
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if ( level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.sessionTeam == team
				&& g_entities[i].health > 0 ) {
				G_Announce( i, "laststanding" );
			}
		}
	}
}

/*
=================
G_CACheckRules

The round cycle: countdown, fighting until one team is wiped out or the
round time runs out (more players alive wins, then more health plus
armour, else a draw), the result, and the next round until a team has
won g_caRounds
=================
*/
void G_CACheckRules( void ) {
	int		alive[TEAM_NUM_TEAMS], stack[TEAM_NUM_TEAMS];

	if ( level.warmupTime ) {
		level.caState = CA_WAITING;
		return;
	}

	switch ( level.caState ) {
	case CA_WAITING:
		G_CAStartRound();
		break;

	case CA_COUNTDOWN:
		if ( level.time >= level.caStateTime ) {
			level.caState = CA_FIGHTING;
			level.caRoundStart = level.time;
			trap_SendServerCommand( -1, va( "cp \"%s\"", BG_String( STR_CA_FIGHT ) ) );
		}
		break;

	case CA_FIGHTING:
		G_CATeamStatus( alive, stack );
		G_CALastStanding( alive );
		if ( !alive[TEAM_RED] || !alive[TEAM_BLUE] ) {
			G_CAEndRound( alive[TEAM_RED] ? TEAM_RED : alive[TEAM_BLUE] ? TEAM_BLUE : TEAM_FREE );
		} else if ( g_caRoundTime.integer > 0 && level.time - level.caRoundStart >= g_caRoundTime.integer * 1000 ) {
			if ( alive[TEAM_RED] != alive[TEAM_BLUE] ) {
				G_CAEndRound( alive[TEAM_RED] > alive[TEAM_BLUE] ? TEAM_RED : TEAM_BLUE );
			} else if ( stack[TEAM_RED] != stack[TEAM_BLUE] ) {
				G_CAEndRound( stack[TEAM_RED] > stack[TEAM_BLUE] ? TEAM_RED : TEAM_BLUE );
			} else {
				G_CAEndRound( TEAM_FREE );
			}
		}
		break;

	case CA_RESULT:
		if ( level.time < level.caStateTime ) {
			break;
		}
		if ( level.teamScores[TEAM_RED] >= g_caRounds.integer || level.teamScores[TEAM_BLUE] >= g_caRounds.integer ) {
			LogExit( "Roundlimit hit." );
			break;
		}
		G_CAStartRound();
		break;
	}
}
