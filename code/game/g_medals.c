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
#define MEDAL_DIEHARD_HEALTH	1		// exactly 1 health, armour aside
#define MEDAL_ASSIST_TIME		3000	// msec a teammate's hit counts as an assist
static int	rushKills[MAX_CLIENTS];		// Bull Rush hit kills in this rush
static int	berserkKills[MAX_CLIENTS];	// kills in this Berserk
static int	ghostEnd[MAX_CLIENTS];		// when the last Ghost Walk ended
static int	evasiveArmed[MAX_CLIENTS];	// this Ghost Walk began hurt and low
static int	evasiveCheck[MAX_CLIENTS];	// alive at this time: Evasive

#define MEDAL_LOW_HEALTH		25		// Reanimation and Evasive: pressed at this or less
#define MEDAL_EVASIVE_HURT		1000	// msec since an enemy's hit
#define MEDAL_EVASIVE_ALIVE		1000	// msec alive after Ghost Walk
#define MEDAL_SHADOWS_TIME		1000	// msec after Ghost Walk for From the Shadows
#define MEDAL_HOVERTANK_HEALTH	110

// group 2 (champion-medals.md: our readings of the names)
static int	useKills[MAX_CLIENTS];		// kills in this use of a timed ability
static int	championMedalTime[MAX_CLIENTS];	// when a champion medal was last given
static int	orbTeleTime[MAX_CLIENTS];	// Ranger's last teleport to the orb
static int	wallSeen[MAX_CLIENTS][MAX_CLIENTS];	// Visor saw them through a wall at this time
static qboolean	useDone[MAX_CLIENTS];		// this use's once-only medal given
static int	tridentVictim[MAX_CLIENTS], tridentHits[MAX_CLIENTS];
static int	totemHealed[MAX_CLIENTS];	// health Galena's totems gave this life
static int	salvo[MAX_CLIENTS], salvoTime[MAX_CLIENTS], salvoKills[MAX_CLIENTS], salvoKilled[MAX_CLIENTS];

#define MEDAL_SHUB_TIME			1000	// msec after an orb teleport
#define MEDAL_CONTROLLER_TIME	2000	// msec since seen through a wall
#define MEDAL_ROCK_DAMAGE		100
#define MEDAL_BLESSED_HEAL		50
#define MEDAL_SALVO_GAP			1500	// throws closer than this are one salvo
#define MEDAL_MORTAR_RANGE		1000
#define MEDAL_LAVA_TIME			3000	// msec off the ground
#define MEDAL_SLIDE_SPEED		330

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
	// a champion's own medal this frame: the plain Ability Kill gives way
	if ( medal == MEDAL_CHOOCHOO || ( medal >= MEDAL_BERSERKER && medal < MEDAL_NUM && medal != MEDAL_ABILITYKILL ) ) {
		championMedalTime[ent->s.number] = level.time;
	}

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
	evasiveArmed[ent->s.number] = evasiveCheck[ent->s.number] = 0;
	totemHealed[ent->s.number] = 0;
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
/*
=================
G_MedalsAbilityPress

An ability pressed, before it acts: Injection (Reanimation) and Ghost Walk
(Evasive) count the health it was pressed at
=================
*/
void G_MedalsAbilityPress( gentity_t *ent ) {
	int		c = ent->s.number, active = BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active;

	if ( active == ACTIVE_INJECTION && ent->health <= MEDAL_LOW_HEALTH ) {
		G_Medal( ent, MEDAL_REANIMATION );
	}
	if ( active == ACTIVE_GHOSTWALK ) {
		evasiveArmed[c] = ent->health <= MEDAL_LOW_HEALTH
			&& level.time - ent->client->lastDamageTime <= MEDAL_EVASIVE_HURT;
	}
}

/*
=================
G_MedalsAbilityFrame

Each frame with the ability's state: starts and ends of Berserk and Ghost
Walk, Anarki's raised maximum health
=================
*/
void G_MedalsAbilityFrame( gentity_t *ent, int prevState, int state ) {
	int		c = ent->s.number, active = BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active;

	if ( prevState != ABILITY_ACTIVE && state == ABILITY_ACTIVE && active == ACTIVE_BERSERK ) {
		berserkKills[c] = 0;
	}
	// the timed abilities' per-use counts
	if ( prevState != ABILITY_ACTIVE && state == ABILITY_ACTIVE ) {
		useKills[c] = 0;
		useDone[c] = qfalse;
		tridentVictim[c] = -1;
		tridentHits[c] = 0;
	}
	// Controller: who Piercing Sight shows through a wall
	if ( state == ABILITY_ACTIVE && active == ACTIVE_PIERCINGSIGHT ) {
		vec3_t	eye;
		int		i;

		VectorCopy( ent->client->ps.origin, eye );
		eye[2] += ent->client->ps.viewheight;
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			gentity_t	*other = &g_entities[i];
			trace_t		tr;

			if ( i == c || !other->inuse || !other->client || other->health <= 0 || OnSameTeam( ent, other )
				|| other->client->sess.sessionTeam == TEAM_SPECTATOR ) {
				continue;
			}
			trap_Trace( &tr, eye, NULL, NULL, other->r.currentOrigin, c, MASK_SOLID );
			if ( tr.fraction < 1.0f ) {
				wallSeen[c][i] = level.time;
			}
		}
	}
	if ( prevState == ABILITY_ACTIVE && state != ABILITY_ACTIVE && active == ACTIVE_GHOSTWALK ) {
		ghostEnd[c] = level.time;
		if ( evasiveArmed[c] ) {
			evasiveCheck[c] = level.time + MEDAL_EVASIVE_ALIVE;
			evasiveArmed[c] = qfalse;
		}
	}
	if ( evasiveCheck[c] && level.time >= evasiveCheck[c] ) {
		evasiveCheck[c] = 0;
		if ( ent->health > 0 ) {
			G_Medal( ent, MEDAL_EVASIVE );
		}
	}
	if ( active == ACTIVE_INJECTION && !ent->client->pers.hovertankDone && ent->client->ps.stats[STAT_MAX_HEALTH] >= MEDAL_HOVERTANK_HEALTH ) {
		ent->client->pers.hovertankDone = qtrue;		// (once a match: the rise stays)
		G_Medal( ent, MEDAL_HOVERTANK );
	}
}

/*
=================
G_AbilityKillMod

A kill by an ability (abilities.md "Ability kills (medal)"): not the
Heavyweight stomp, plain telefrags, falls or hazards
=================
*/
static qboolean G_AbilityKillMod( gentity_t *inflictor, int mod ) {
	switch ( mod ) {
	case MOD_BULLRUSH: case MOD_ORB: case MOD_FIREBALL: case MOD_BURN: case MOD_ACID:
	case MOD_TOTEM: case MOD_TURRET: case MOD_LASER: case MOD_PLASMATRAIL: case MOD_DRONE:
	case MOD_SPINES: case MOD_STORM: case MOD_HOUND: case MOD_HOOKIMPACT: case MOD_GHOSTWALK:
	case MOD_ORBTELEFRAG: case MOD_BERSERK: case MOD_LEAPINGKICK:
		return qtrue;
	default:
		return inflictor && inflictor->classname && !strcmp( inflictor->classname, "swarm" );	// Keel's grenades
	}
}

void G_MedalsRushStart( gentity_t *ent ) {
	rushKills[ent->s.number] = 0;
}

/*
=================
Group 2 hooks, called from the abilities
=================
*/
void G_MedalsOrbTeleport( gentity_t *ent ) {
	orbTeleTime[ent->s.number] = level.time;
}

// Plague: three enemies poisoned by one Sorlag at once, once a spit
void G_MedalsPoison( gentity_t *attacker ) {
	int		i, count = 0, c;

	if ( !attacker || !attacker->client ) {
		return;
	}
	c = attacker->s.number;
	if ( useDone[c] ) {
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t	*cl = &level.clients[i];

		if ( i != c && cl->pers.connected == CON_CONNECTED && g_entities[i].health > 0
			&& cl->poisonEndTime > level.time && cl->poisonAttacker == c ) {
			count++;
		}
	}
	if ( count >= 3 ) {
		useDone[c] = qtrue;
		G_Medal( attacker, MEDAL_PLAGUE );
	}
}

// The Rock: the shield took 100 in one use
void G_MedalsShieldHit( gentity_t *ent, int before, int after ) {
	if ( before < MEDAL_ROCK_DAMAGE && after >= MEDAL_ROCK_DAMAGE ) {
		G_Medal( ent, MEDAL_THEROCK );
	}
}

// Trident: all three fireballs of one strike on the same enemy
void G_MedalsFireballHit( gentity_t *attacker, gentity_t *victim ) {
	int		c;

	if ( !attacker || !attacker->client || !victim->client || attacker == victim || OnSameTeam( attacker, victim ) ) {
		return;
	}
	c = attacker->s.number;
	if ( tridentVictim[c] != victim->s.number ) {
		tridentVictim[c] = victim->s.number;
		tridentHits[c] = 0;
	}
	if ( ++tridentHits[c] == 3 ) {
		G_Medal( attacker, MEDAL_TRIDENT );
	}
}

// Blessed: 50 health healed by Galena's totems in one life (herself and allies)
void G_MedalsTotemHeal( gentity_t *owner, int amount ) {
	int		c;

	if ( !owner || !owner->client || amount <= 0 ) {
		return;
	}
	c = owner->s.number;
	if ( totemHealed[c] < MEDAL_BLESSED_HEAL && totemHealed[c] + amount >= MEDAL_BLESSED_HEAL ) {
		G_Medal( owner, MEDAL_BLESSED );
	}
	totemHealed[c] += amount;
}

// Psi Radar: the drone shows two enemies at once, once a flight
void G_MedalsDroneReveal( gentity_t *ent, int shown ) {
	if ( shown >= 2 && !useDone[ent->s.number] ) {
		useDone[ent->s.number] = qtrue;
		G_Medal( ent, MEDAL_PSIRADAR );
	}
}

// Keel's throws: one salvo while they come close together
int G_MedalsSalvo( gentity_t *ent ) {
	int		c = ent->s.number;

	if ( level.time - salvoTime[c] > MEDAL_SALVO_GAP ) {
		salvo[c]++;
	}
	salvoTime[c] = level.time;
	return salvo[c];
}

/*
=================
G_MedalsChampionKill

Group 2: the champions' own medals for a kill of an enemy
=================
*/
static void G_MedalsChampionKill( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int mod ) {
	gclient_t	*client = attacker->client;
	int			c = attacker->s.number, active = BG_Champion( client->ps.stats[STAT_CHAMPION] )->active;
	qboolean	dot = !inflictor || inflictor->s.number == ENTITYNUM_WORLD;	// burn or poison ticking

	switch ( mod ) {
	// Eisen
	case MOD_TURRET:
		G_Medal( attacker, MEDAL_HANDYMAN );
		if ( inflictor && inflictor->activator && inflictor->activator->inuse && inflictor->activator->classname
			&& !strcmp( inflictor->activator->classname, "sentry_turret" ) && ++inflictor->activator->medalKills == 2 ) {
			G_Medal( attacker, MEDAL_FORTRESS );
		}
		if ( ++client->pers.turretKills == 3 ) {
			G_Medal( attacker, MEDAL_WEAPONSFACTORY );
		}
		break;
	// Strogg: the drone's gun, or its blast
	case MOD_DRONE:
		G_Medal( attacker, inflictor && inflictor->s.eType == ET_MISSILE ? MEDAL_AIRASSAULT : MEDAL_BOMBARDMENT );
		break;
	// Ranger
	case MOD_ORB:
		G_Medal( attacker, MEDAL_DISINTEGRATOR );
		break;
	case MOD_ORBTELEFRAG:
		G_Medal( attacker, MEDAL_DEADLYSLIPGATE );
		break;
	// Sorlag: poison after the pool or after the hit
	case MOD_ACID:
		if ( dot ) {
			G_Medal( attacker, self->client->poisonPool ? MEDAL_MELTDOWN : MEDAL_VENOMOUS );
		}
		break;
	// Clutch
	case MOD_LASER:
		G_Medal( attacker, MEDAL_DRILL );
		if ( active == ACTIVE_BARRIER && ++useKills[c] == 2 ) {
			G_Medal( attacker, MEDAL_SHOCKER );
		}
		break;
	// Galena
	case MOD_TOTEM:
		G_Medal( attacker, MEDAL_DAMNED );
		break;
	// Slash
	case MOD_PLASMATRAIL:
		G_Medal( attacker, MEDAL_SLALOM );
		if ( active == ACTIVE_PLASMATRAIL && ++useKills[c] == 2 ) {
			G_Medal( attacker, MEDAL_BLACKSWAN );
		}
		break;
	// Death Knight: the burn, or the ground fire
	case MOD_BURN:
		if ( active == ACTIVE_FLAMESTRIKE ) {
			G_Medal( attacker, dot ? MEDAL_INFERNO : MEDAL_SMELTER );
		}
		break;
	// Athena
	case MOD_HOOKIMPACT:
		G_Medal( attacker, MEDAL_SWINGER );
		break;
	default:
		break;
	}

	// Ranger: soon after a teleport to the orb
	if ( active == ACTIVE_ORB && mod != MOD_ORBTELEFRAG && orbTeleTime[c] && level.time - orbTeleTime[c] <= MEDAL_SHUB_TIME ) {
		G_Medal( attacker, MEDAL_SHUBSLAYER );
	}
	// Visor
	if ( G_AbilityActive( attacker, ACTIVE_PIERCINGSIGHT ) ) {
		G_Medal( attacker, MEDAL_SEER );
		if ( ++useKills[c] == 3 ) {
			G_Medal( attacker, MEDAL_SHOOTINGRANGE );
		}
	}
	if ( active == ACTIVE_PIERCINGSIGHT && wallSeen[c][self->s.number]
		&& level.time - wallSeen[c][self->s.number] <= MEDAL_CONTROLLER_TIME ) {
		wallSeen[c][self->s.number] = 0;
		G_Medal( attacker, MEDAL_CONTROLLER );
	}
	// Galena: anything killing an enemy by one of her standing totems
	if ( active == ACTIVE_TOTEM && mod != MOD_TOTEM ) {
		gentity_t	*t;
		int			i;

		for ( i = MAX_CLIENTS, t = &g_entities[MAX_CLIENTS] ; i < level.num_entities ; i++, t++ ) {
			if ( t->inuse && t->parent == attacker && t->classname && !strcmp( t->classname, "totem" ) && t->takedamage
				&& Distance( t->r.currentOrigin, self->r.currentOrigin ) <= g_totemHurtRadius.value ) {
				G_Medal( attacker, MEDAL_CURSED );
				break;
			}
		}
	}
	// B.J.
	if ( G_AbilityActive( attacker, ACTIVE_DUALWIELD ) ) {
		G_Medal( attacker, MEDAL_MACEDONIAN );
		if ( ++useKills[c] == 3 ) {
			G_Medal( attacker, MEDAL_GETPSYCHED );
		}
	}
	if ( active == ACTIVE_DUALWIELD && attacker->health > 0 && attacker->health <= MEDAL_LOW_HEALTH
		&& client->regenTime && level.time - client->regenTime <= 1000 ) {
		G_Medal( attacker, MEDAL_MEINLEBEN );
	}
	// Slash: on a slide
	if ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->moveStyle == MOVE_SLIDE && active == ACTIVE_PLASMATRAIL
		&& client->ps.groundEntityNum != ENTITYNUM_NONE && ( client->ps.pm_flags & PMF_DUCKED )
		&& client->ps.stats[STAT_MOVE_STATE] > 0
		&& sqrt( client->ps.velocity[0] * client->ps.velocity[0] + client->ps.velocity[1] * client->ps.velocity[1] ) > MEDAL_SLIDE_SPEED ) {
		G_Medal( attacker, MEDAL_SLIDER );
	}
	// Keel's grenades
	if ( inflictor && inflictor->classname && !strcmp( inflictor->classname, "swarm" ) ) {
		if ( mod == MOD_GRENADE ) {
			G_Medal( attacker, MEDAL_PINEAPPLE );
		}
		if ( Distance( attacker->r.currentOrigin, self->r.currentOrigin ) > MEDAL_MORTAR_RANGE ) {
			G_Medal( attacker, MEDAL_MORTAR );
		}
		if ( salvoKilled[c] != inflictor->medalSalvo ) {
			salvoKilled[c] = inflictor->medalSalvo;
			salvoKills[c] = 0;
		}
		if ( ++salvoKills[c] == 2 ) {
			G_Medal( attacker, MEDAL_NADESPAM );
		}
	}
	// Athena
	if ( active == ACTIVE_HOOK ) {
		int		state = BG_AbilityState( &client->ps );

		if ( state == ABILITY_HOOK_PULL || ( client->ps.pm_flags & PMF_GRAPPLE_PULL ) ) {
			G_Medal( attacker, MEDAL_ROPETRICK );
		}
		if ( client->ps.groundEntityNum == ENTITYNUM_NONE && level.time - client->groundTime >= MEDAL_LAVA_TIME ) {
			G_Medal( attacker, MEDAL_FLOORISLAVA );
		}
	}
}

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

	// Choo Choo: two enemies by Bull Rush hits in one rush, once, at the
	// second (the Heavyweight stomp is its own means of death)
	if ( mod == MOD_BULLRUSH && ++rushKills[attacker->s.number] == 2 ) {
		G_Medal( attacker, MEDAL_CHOOCHOO );
	}
	// Eisen's Shutdown: his turret killing a Clutch cuts his cooldown 20 %
	if ( mod == MOD_TURRET && BG_Champion( self->client->ps.stats[STAT_CHAMPION] )->active == ACTIVE_BARRIER ) {
		G_CutAbilityCooldown( attacker, 0.2f );
	}
	// the champions' own (champion-medals.md)
	if ( mod == MOD_BULLRUSH ) {
		G_Medal( attacker, MEDAL_ROADKILL );
	} else if ( mod == MOD_HEAVYWEIGHT ) {
		G_Medal( attacker, MEDAL_STOMPER );
	} else if ( mod == MOD_GHOSTWALK ) {
		G_Medal( attacker, MEDAL_INSIDEOUT );
	}
	if ( G_AbilityActive( attacker, ACTIVE_BERSERK ) ) {
		G_Medal( attacker, MEDAL_SLAYER );
		if ( ++berserkKills[attacker->s.number] == 3 ) {
			G_Medal( attacker, MEDAL_BERSERKER );
		}
	}
	if ( mod == MOD_SHOTGUN && BG_Champion( client->ps.stats[STAT_CHAMPION] )->active == ACTIVE_BERSERK
		&& client->ps.groundEntityNum == ENTITYNUM_NONE && ( client->ps.pm_flags & PMF_AIRMOVE_USED ) ) {
		G_Medal( attacker, MEDAL_DAISY );
	}
	if ( G_AbilityActive( attacker, ACTIVE_INJECTION ) ) {
		G_Medal( attacker, MEDAL_JUNKIE );
	}
	if ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->active == ACTIVE_GHOSTWALK && ghostEnd[attacker->s.number]
		&& level.time - ghostEnd[attacker->s.number] <= MEDAL_SHADOWS_TIME ) {
		G_Medal( attacker, MEDAL_FROMTHESHADOWS );
	}
	G_MedalsChampionKill( self, inflictor, attacker, mod );
	// Ability Kill only where the champion has no medal of its own for it
	if ( G_AbilityKillMod( inflictor, mod ) && championMedalTime[attacker->s.number] != level.time ) {
		G_Medal( attacker, MEDAL_ABILITYKILL );
	}

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
	} else if ( attacker->client->frameHealth > 0 && attacker->client->frameHealth <= MEDAL_DIEHARD_HEALTH ) {
		// judged on the health before this frame's damage (a rocket at
		// your own feet hits you before the kill lands)
		G_Medal( attacker, MEDAL_DIEHARD );
	}
	if ( G_HasPowerup( attacker ) && ++client->powerupKills == 5 ) {
		G_Medal( attacker, MEDAL_PUMASSACRE );
	}

	// weapon gods
	weapon = mod == MOD_RINGOUT ? WP_NONE : G_MedalWeapon( inflictor, attacker );		// (a push isn't a weapon kill)
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
