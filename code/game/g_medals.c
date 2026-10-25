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
// g_medals.c -- the arena medal set
//
// Quake III's own awards (excellent, impressive, humiliation, defend, assist,
// capture) stay where they were; these are the rest. Each one counts toward
// the end board and reaches the earner as "medal <id> <count>".

#include "g_local.h"

#define MEDAL_MULTI_TIME		3000	// msec between kills of one multi-kill
#define MEDAL_AIR_TIME			500		// msec off the ground to count as airborne
#define MEDAL_COMBO_TIME		1000	// msec from a hit to a railgun finish
#define MEDAL_POINT_BLANK		64		// shotgun gap between bodies
#define MEDAL_PRECISE_RANGE		1000	// rocket flight for Precise
#define MEDAL_STOPWATCH_TIME	1000	// msec after an item appears
#define MEDAL_DIEHARD_HEALTH	25
#define MEDAL_ASSIST_TIME		3000	// msec a teammate's hit counts as an assist

/*
=================
G_Medal

Counts a medal and tells the earner and anyone following them
=================
*/
static void G_Medal( gentity_t *ent, int medal ) {
	gclient_t	*client = ent->client;
	int			i, count;

	if ( !client || level.warmupTime || level.intermissiontime ) {
		return;
	}
	count = ++client->medals[medal];

	// Xaero banks a Leaping Kick on these medals with the matching weapon
	if ( ( medal == MEDAL_POINTBLANK && ( ent->s.weapon == WP_SHOTGUN || ent->s.weapon == WP_SUPER_SHOTGUN ) )
		|| ( ( medal == MEDAL_MIDAIR || medal == MEDAL_AIRBORNE || medal == MEDAL_PRECISE ) && ent->s.weapon == WP_ROCKET_LAUNCHER ) ) {
		G_BankKick( ent );
	}
	G_LogPrintf( "Medal: %i %i: %s %s\n", (int)( ent - g_entities ), medal, client->pers.netname, bg_medalInfo[medal].key );
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t	*cl = &level.clients[i];

		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl == client || ( cl->sess.spectatorState == SPECTATOR_FOLLOW && cl->sess.spectatorClient == ent - g_entities ) ) {
			trap_SendServerCommand( i, va( "medal %i %i", medal, count ) );
		}
	}
}

/*
=================
G_Announce

An announcer line for one client, or everyone with -1: the clients play
sound/feedback/<key>.wav from the install when it has it
=================
*/
void G_Announce( int clientNum, const char *key ) {
	trap_SendServerCommand( clientNum, va( "announce %s", key ) );
}

/*
=================
G_EventLine

An event line by string id with up to two arguments; the client picks its
priority (messages.md 2.4) and checks the id and the format
=================
*/
void G_EventLine( int clientNum, int id, const char *a, const char *b ) {
	char	sa[MAX_STRING_CHARS], sb[MAX_STRING_CHARS], *p;

	Q_strncpyz( sa, a ? a : "", sizeof( sa ) );
	Q_strncpyz( sb, b ? b : "", sizeof( sb ) );
	for ( p = sa ; *p ; p++ ) {
		if ( *p == '"' ) {
			*p = '\'';
		}
	}
	for ( p = sb ; *p ; p++ ) {
		if ( *p == '"' ) {
			*p = '\'';
		}
	}
	trap_SendServerCommand( clientNum, va( "evline %i \"%s\" \"%s\"", id, sa, sb ) );
}

/*
=================
G_BannerWord

FIGHT, SUDDEN DEATH and the like, by string id: the banner word slot
(hud.md 7.5)
=================
*/
void G_BannerWord( int id ) {
	trap_SendServerCommand( -1, va( "bword %i", id ) );
}

/*
=================
G_ServerLine

Map and admin text for the server line, quotes made safe
=================
*/
void G_ServerLine( int clientNum, const char *text ) {
	char	safe[MAX_STRING_CHARS], *p;

	// a quote would end the command's argument early
	Q_strncpyz( safe, text, sizeof( safe ) );
	for ( p = safe ; *p ; p++ ) {
		if ( *p == '"' ) {
			*p = '\'';
		}
	}
	trap_SendServerCommand( clientNum, va( "sline \"%s\"", safe ) );
}

/*
=================
G_SysMsg

A system message by string id (with up to two arguments): the clients
show it in their own words, connect lines at the top right, the rest as
grey chat lines; -1 sends to everyone
=================
*/
void G_SysMsg( int clientNum, int id, const char *a, const char *b ) {
	char	sa[MAX_STRING_CHARS], sb[MAX_STRING_CHARS], *p;

	// quotes would end the arguments early
	Q_strncpyz( sa, a ? a : "", sizeof( sa ) );
	Q_strncpyz( sb, b ? b : "", sizeof( sb ) );
	for ( p = sa ; *p ; p++ ) {
		if ( *p == '"' ) {
			*p = '\'';
		}
	}
	for ( p = sb ; *p ; p++ ) {
		if ( *p == '"' ) {
			*p = '\'';
		}
	}
	trap_SendServerCommand( clientNum, va( "sysmsg %i \"%s\" \"%s\"", id, sa, sb ) );
}

/*
=================
G_MedalWeapon

The weapon behind a kill: the missile's, or the attacker's in hand
=================
*/
static int G_MedalWeapon( gentity_t *inflictor, gentity_t *attacker ) {
	if ( inflictor && inflictor != attacker && inflictor->s.eType == ET_MISSILE ) {
		return inflictor->s.weapon;
	}
	return attacker->s.weapon;
}

static qboolean G_Airborne( gentity_t *ent ) {
	return ent->client->ps.groundEntityNum == ENTITYNUM_NONE
		&& level.time - ent->client->groundTime >= MEDAL_AIR_TIME;
}

static qboolean G_HasPowerup( gentity_t *ent ) {
	return ent->client->ps.powerups[PW_QUAD] > level.time || ent->client->ps.powerups[PW_BATTLESUIT] > level.time;
}

/*
=================
G_MedalOffense

gaps.md GAP-17 Offense (team objectives); the rule is ours (d): a kill
of an enemy within the flag-defence radius of that enemy's flag base,
with the base in sight of either player
=================
*/
void G_MedalOffense( gentity_t *attacker, gentity_t *targ ) {
	gentity_t	*flag = NULL;
	const char	*c;
	vec3_t		v;

	if ( g_gametype.integer != GT_CTF || !attacker->client || !targ->client
		|| attacker->client->sess.sessionTeam == targ->client->sess.sessionTeam ) {
		return;
	}
	switch ( targ->client->sess.sessionTeam ) {
	case TEAM_RED:	c = "team_CTF_redflag"; break;
	case TEAM_BLUE:	c = "team_CTF_blueflag"; break;
	default:		return;
	}
	while ( ( flag = G_Find( flag, FOFS( classname ), c ) ) != NULL ) {
		if ( !( flag->flags & FL_DROPPED_ITEM ) ) {
			break;
		}
	}
	if ( !flag ) {
		return;
	}
	VectorSubtract( targ->r.currentOrigin, flag->r.currentOrigin, v );
	if ( VectorLength( v ) < CTF_TARGET_PROTECT_RADIUS && trap_InPVS( flag->r.currentOrigin, targ->r.currentOrigin ) ) {
		G_Medal( attacker, MEDAL_OFFENSE );
		return;
	}
	VectorSubtract( attacker->r.currentOrigin, flag->r.currentOrigin, v );
	if ( VectorLength( v ) < CTF_TARGET_PROTECT_RADIUS && trap_InPVS( flag->r.currentOrigin, attacker->r.currentOrigin ) ) {
		G_Medal( attacker, MEDAL_OFFENSE );
	}
}

/*
=================
G_MedalsSpawn

A new life
=================
*/
void G_MedalsSpawn( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	client->streak = 0;
	client->lifeDamage = 0;
	client->powerupKills = 0;
	client->tankDone = qfalse;
	client->groundTime = level.time;
	client->lastEnemyHurtTime = 0;
	memset( client->hurtTime, 0, sizeof( client->hurtTime ) );
	memset( client->comboTime, 0, sizeof( client->comboTime ) );
}

/*
=================
G_MedalsDamage

Damage an enemy took from a player
=================
*/
void G_MedalsDamage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, int take, int mod ) {
	gclient_t	*client;
	int			before;
	float		gap;

	if ( !targ->client || !attacker || !attacker->client || attacker == targ || OnSameTeam( targ, attacker ) || take <= 0 ) {
		return;
	}
	client = attacker->client;
	if ( mod == MOD_TELEFRAG ) {
		take = ( targ->health > 0 ? targ->health : 0 ) + targ->client->ps.stats[STAT_ARMOR];
	}
	targ->client->hurtTime[attacker->s.number] = level.time;
	if ( mod != MOD_RAILGUN ) {
		targ->client->comboTime[attacker->s.number] = level.time;
	}
	targ->client->lastEnemyHurt = attacker->s.number;
	targ->client->lastEnemyHurtTime = level.time;

	before = client->lifeDamage;
	client->lifeDamage += take;
	if ( before < 1000 && client->lifeDamage >= 1000 ) {
		G_Medal( attacker, MEDAL_NETMASTER );
	}
	before = client->matchDamage;
	client->matchDamage += take;
	if ( before < 5000 && client->matchDamage >= 5000 ) {
		G_Medal( attacker, MEDAL_DAMAGEDEALER );
	}

	if ( mod == MOD_ROCKET && G_Airborne( targ ) ) {
		G_Medal( attacker, MEDAL_AIRBORNE );
	}
	if ( mod == MOD_SHOTGUN && client->pointBlankTime != level.time ) {
		gap = Distance( targ->r.currentOrigin, attacker->r.currentOrigin ) - targ->r.maxs[0] - attacker->r.maxs[0];
		if ( gap <= MEDAL_POINT_BLANK ) {
			client->pointBlankTime = level.time;
			G_Medal( attacker, MEDAL_POINTBLANK );
		}
	}
}

/*
=================
G_MedalsKill

An enemy killed by a player
=================
*/
static void G_MedalsKill( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int mod, qboolean abilityOn ) {
	gclient_t	*client = attacker->client;
	gclient_t	*victim = self->client;
	int			weapon, i, god;
	static const int	godKills[][3] = {
		{ WP_GAUNTLET, 10, MEDAL_GOD_GAUNTLET }, { WP_MACHINEGUN, 15, MEDAL_GOD_MG },
		{ WP_SHOTGUN, 15, MEDAL_GOD_SG }, { WP_NAILGUN, 15, MEDAL_GOD_NG },
		{ WP_TRIBOLT, 15, MEDAL_GOD_TB }, { WP_ROCKET_LAUNCHER, 20, MEDAL_GOD_RL },
		{ WP_LIGHTNING, 20, MEDAL_GOD_LG }, { WP_RAILGUN, 20, MEDAL_GOD_RG }
	};

	if ( !level.firstBlood && !level.warmupTime ) {
		level.firstBlood = qtrue;
		G_Medal( attacker, MEDAL_FIRSTBLOOD );
	}

	// in a row and without dying
	if ( level.time - client->medalKillTime <= MEDAL_MULTI_TIME ) {
		client->multiKills++;
	} else {
		client->multiKills = 1;
	}
	client->medalKillTime = level.time;
	// 2 is Excellent, the classic award
	switch ( client->multiKills ) {
	case 3:		G_Medal( attacker, MEDAL_TRIPLEKILL ); break;
	case 4:		G_Medal( attacker, MEDAL_OVERKILL ); break;
	case 5:		G_Medal( attacker, MEDAL_FIGHTER ); break;
	case 6:		G_Medal( attacker, MEDAL_CRUSHER ); break;
	case 7:		G_Medal( attacker, MEDAL_EXTERMINATOR ); break;
	case 8:		G_Medal( attacker, MEDAL_DEMOLISHER ); break;
	case 9:		G_Medal( attacker, MEDAL_GRIMREAPER ); break;
	case 10:	G_Medal( attacker, MEDAL_AVATAR ); break;
	}
	switch ( ++client->streak ) {
	case 5:		G_Medal( attacker, MEDAL_SPREE ); break;
	case 10:	G_Medal( attacker, MEDAL_RAGE ); break;
	case 15:	G_Medal( attacker, MEDAL_FURY ); break;
	case 20:	G_Medal( attacker, MEDAL_FRENZY ); break;
	case 25:	G_Medal( attacker, MEDAL_RIOT ); break;
	case 30:	G_Medal( attacker, MEDAL_RAMPAGE ); break;
	case 40:	G_Medal( attacker, MEDAL_MERCILESS ); break;
	case 50:	G_Medal( attacker, MEDAL_FANATICISM ); break;
	case 60:	G_Medal( attacker, MEDAL_SADISM ); break;
	case 75:	G_Medal( attacker, MEDAL_EXECUTIONER ); break;
	}
	if ( victim->streak >= 5 ) {
		G_Medal( attacker, MEDAL_SHOWSTOPPER );
	}

	// how
	if ( mod == MOD_ROCKET && G_Airborne( self ) ) {
		G_Medal( attacker, MEDAL_MIDAIR );
	}
	if ( mod == MOD_ROCKET && inflictor && inflictor != attacker
		&& Distance( inflictor->s.pos.trBase, self->r.currentOrigin ) > MEDAL_PRECISE_RANGE ) {
		G_Medal( attacker, MEDAL_PRECISE );
	}
	if ( mod == MOD_RAILGUN && victim->comboTime[attacker->s.number]
		&& level.time - victim->comboTime[attacker->s.number] <= MEDAL_COMBO_TIME ) {
		G_Medal( attacker, MEDAL_COMBOKILL );
	}
	if ( inflictor && inflictor != attacker && inflictor->s.eType == ET_MISSILE && ++inflictor->medalKills == 2 ) {
		G_Medal( attacker, MEDAL_COLLATERAL );
	}
	if ( mod == MOD_TELEFRAG ) {
		G_Medal( attacker, MEDAL_PERFORATED );
	}

	// whom
	if ( abilityOn ) {
		G_Medal( attacker, MEDAL_DENIED );
	}
	if ( G_HasPowerup( self ) ) {
		G_Medal( attacker, MEDAL_PARTYBREAKER );
	}
	if ( client->killedBy == self->s.number && client->killedByStreak >= 3 ) {
		G_Medal( attacker, MEDAL_REVENGE );
		client->killedByStreak = 0;
	}

	// in what state
	if ( attacker->health <= 0 ) {
		if ( client->deathTime == level.time && inflictor && client->deathInflictor == inflictor->s.number ) {
			G_Medal( attacker, MEDAL_KAMIKAZE );
		} else {
			G_Medal( attacker, MEDAL_FROMTHEGRAVE );
		}
	} else if ( attacker->health < MEDAL_DIEHARD_HEALTH ) {
		G_Medal( attacker, MEDAL_DIEHARD );
	}
	if ( G_HasPowerup( attacker ) && ++client->powerupKills == 5 ) {
		G_Medal( attacker, MEDAL_PUMASSACRE );
	}

	// weapon gods
	weapon = G_MedalWeapon( inflictor, attacker );
	if ( weapon > WP_NONE && weapon < WP_NUM_WEAPONS ) {
		client->weaponKills[weapon]++;
		for ( i = 0 ; i < ARRAY_LEN( godKills ) ; i++ ) {
			if ( godKills[i][0] == weapon && client->weaponKills[weapon] == godKills[i][1] ) {
				god = godKills[i][2];
				G_Medal( attacker, god );
			}
		}
	}

	// teammates who hurt the victim just before
	if ( g_gametype.integer >= GT_TEAM ) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			gentity_t	*mate = &g_entities[i];

			if ( mate == attacker || !mate->inuse || !mate->client || !OnSameTeam( mate, attacker )
				|| !victim->hurtTime[i] || level.time - victim->hurtTime[i] > MEDAL_ASSIST_TIME ) {
				continue;
			}
			if ( ++mate->client->assists == 10 ) {
				G_Medal( mate, MEDAL_ASSISTANT );
			}
		}
	}
}

/*
=================
G_MedalsDeath

Called from player_die before the victim's state is reset; abilityOn is
whether their active ability was running
=================
*/
void G_MedalsDeath( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int mod, qboolean abilityOn ) {
	gclient_t	*victim = self->client;

	victim->deathTime = level.time;
	victim->deathInflictor = inflictor ? inflictor->s.number : ENTITYNUM_NONE;

	if ( attacker && attacker->client && attacker != self && !OnSameTeam( self, attacker ) ) {
		G_MedalsKill( self, inflictor, attacker, mod, abilityOn );
		// knocked into a pit or off the map (player_die made it the pusher's kill)
		if ( mod == MOD_RINGOUT ) {
			G_Medal( attacker, MEDAL_RINGOUT );
		}
		if ( victim->killedBy == attacker->s.number ) {
			victim->killedByStreak++;
		} else {
			victim->killedBy = attacker->s.number;
			victim->killedByStreak = 1;
		}
	} else if ( attacker == self && inflictor && inflictor->s.eType == ET_MISSILE && inflictor->medalKills > 0 ) {
		// blew up with the enemy it killed
		G_Medal( self, MEDAL_KAMIKAZE );
	}
	victim->streak = 0;
}

/*
=================
G_MedalsPickup

After an item is taken
=================
*/
void G_MedalsPickup( gentity_t *ent, gentity_t *item ) {
	gclient_t	*client = ent->client;
	int			cls = BG_ItemTimerClass( item->item );

	// a major item taken as it appears
	if ( cls && item->medalSpawnTime && level.time - item->medalSpawnTime <= MEDAL_STOPWATCH_TIME ) {
		G_Medal( ent, MEDAL_STOPWATCH );
	}
	if ( item->item->giType == IT_POWERUP ) {
		if ( level.powerupClient == ent->s.number + 1 ) {
			level.powerupStreak++;
		} else {
			level.powerupClient = ent->s.number + 1;
			level.powerupStreak = 1;
		}
		if ( level.powerupStreak == 3 ) {
			G_Medal( ent, MEDAL_DOMINATING );
		}
		client->powerupKills = 0;
	}
	if ( !client->tankDone && client->ps.stats[STAT_HEALTH] > client->ps.stats[STAT_MAX_HEALTH]
		&& client->ps.stats[STAT_ARMOR] > bg_classInfo[BG_Champion( client->ps.stats[STAT_CHAMPION] )->bodyClass].baseArmor ) {
		client->tankDone = qtrue;
		G_Medal( ent, MEDAL_TANK );
	}
}

/*
=================
G_SendAwards

At the end of a match: the most accurate, most damage, best net damage,
most frags with each weapon, and a perfect match (no deaths, duel and
free-for-all), as "awards <type> <client> <value> ..." to everyone
=================
*/
void G_SendAwards( void ) {
	static const int	weapons[] = { WP_GAUNTLET, WP_MACHINEGUN, WP_HEAVY_MACHINEGUN, WP_SHOTGUN, WP_SUPER_SHOTGUN,
		WP_NAILGUN, WP_TRIBOLT, WP_ROCKET_LAUNCHER, WP_LIGHTNING, WP_RAILGUN };
	char		msg[MAX_STRING_CHARS];
	int			best[4], bestValue[4], i, w, value;
	gclient_t	*cl;

	Q_strncpyz( msg, "awards", sizeof( msg ) );
	for ( i = 0 ; i < 4 ; i++ ) {
		best[i] = -1;
		bestValue[i] = -999999;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		// accuracy in tenths of a percent, from 10 shots up
		value = cl->accuracy_shots >= 10 ? cl->accuracy_hits * 1000 / cl->accuracy_shots : -1;
		if ( value > bestValue[0] ) {
			best[0] = i;
			bestValue[0] = value;
		}
		if ( cl->pers.damageGiven > bestValue[1] ) {
			best[1] = i;
			bestValue[1] = cl->pers.damageGiven;
		}
		value = cl->pers.damageGiven - cl->pers.damageTaken;
		if ( value > bestValue[2] ) {
			best[2] = i;
			bestValue[2] = value;
		}
		// perfect: no deaths, at least one frag, duel or free-for-all
		if ( g_gametype.integer < GT_TEAM && !cl->ps.persistant[PERS_KILLED] && cl->ps.persistant[PERS_SCORE] > 0 ) {
			Q_strcat( msg, sizeof( msg ), va( " perfect %i 0", i ) );
		}
	}
	if ( best[0] >= 0 && bestValue[0] >= 0 ) {
		Q_strcat( msg, sizeof( msg ), va( " accuracy %i %i", best[0], bestValue[0] ) );
	}
	if ( best[1] >= 0 && bestValue[1] > 0 ) {
		Q_strcat( msg, sizeof( msg ), va( " damage %i %i", best[1], bestValue[1] ) );
	}
	if ( best[2] >= 0 && bestValue[2] > 0 ) {
		Q_strcat( msg, sizeof( msg ), va( " net %i %i", best[2], bestValue[2] ) );
	}
	for ( w = 0 ; w < ARRAY_LEN( weapons ) ; w++ ) {
		best[3] = -1;
		bestValue[3] = 0;
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			cl = &level.clients[i];
			if ( cl->pers.connected == CON_CONNECTED && cl->weaponKills[weapons[w]] > bestValue[3] ) {
				best[3] = i;
				bestValue[3] = cl->weaponKills[weapons[w]];
			}
		}
		if ( best[3] >= 0 ) {
			Q_strcat( msg, sizeof( msg ), va( " w%i %i %i", weapons[w], best[3], bestValue[3] ) );
		}
	}
	trap_SendServerCommand( -1, msg );
}

/*
=================
G_MedalsRail

One railgun shot through several players
=================
*/
void G_MedalsRail( gentity_t *ent, int hits ) {
	if ( hits >= 2 ) {
		G_Medal( ent, MEDAL_DOUBLEWHAMMY );
	}
}

/*
=================
G_RoundInfo

The top bar's round (Clan Arena, Sacrifice, round duel), duel overtime
period and sudden death, in CS_ROUNDINFO
=================
*/
void G_RoundInfo( int round, int overtime, qboolean suddenDeath ) {
	trap_SetConfigstring( CS_ROUNDINFO, va( "%i %i %i", round, overtime, suddenDeath ) );
}
