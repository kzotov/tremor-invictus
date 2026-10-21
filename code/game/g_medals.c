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
// g_medals.c -- the QC medal set
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
#define MEDAL_RINGOUT_TIME		3000	// msec an enemy's hit still counts for a fall
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
	switch ( client->multiKills ) {
	case 3:		G_Medal( attacker, MEDAL_TRIPLEKILL ); break;
	case 5:		G_Medal( attacker, MEDAL_FIGHTER ); break;
	case 10:	G_Medal( attacker, MEDAL_AVATAR ); break;
	}
	switch ( ++client->streak ) {
	case 5:		G_Medal( attacker, MEDAL_SPREE ); break;
	case 10:	G_Medal( attacker, MEDAL_RAGE ); break;
	case 20:	G_Medal( attacker, MEDAL_FRENZY ); break;
	case 30:	G_Medal( attacker, MEDAL_RAMPAGE ); break;
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
	gentity_t	*enemy;

	victim->deathTime = level.time;
	victim->deathInflictor = inflictor ? inflictor->s.number : ENTITYNUM_NONE;

	if ( attacker && attacker->client && attacker != self && !OnSameTeam( self, attacker ) ) {
		G_MedalsKill( self, inflictor, attacker, mod, abilityOn );
		if ( victim->killedBy == attacker->s.number ) {
			victim->killedByStreak++;
		} else {
			victim->killedBy = attacker->s.number;
			victim->killedByStreak = 1;
		}
	} else if ( attacker == self && inflictor && inflictor->s.eType == ET_MISSILE && inflictor->medalKills > 0 ) {
		// blew up with the enemy it killed
		G_Medal( self, MEDAL_KAMIKAZE );
	} else if ( ( !attacker || !attacker->client ) && victim->lastEnemyHurtTime
		&& level.time - victim->lastEnemyHurtTime <= MEDAL_RINGOUT_TIME ) {
		// knocked into a pit or off the map
		switch ( mod ) {
		case MOD_TRIGGER_HURT: case MOD_FALLING: case MOD_LAVA: case MOD_SLIME: case MOD_WATER:
			enemy = &g_entities[victim->lastEnemyHurt];
			if ( enemy->inuse && enemy->client ) {
				G_Medal( enemy, MEDAL_RINGOUT );
			}
			break;
		}
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
G_MedalsRail

One railgun shot through several players
=================
*/
void G_MedalsRail( gentity_t *ent, int hits ) {
	if ( hits >= 2 ) {
		G_Medal( ent, MEDAL_DOUBLEWHAMMY );
	}
}
