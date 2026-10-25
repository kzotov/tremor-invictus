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
// g_ability.c -- champion abilities that put things in the world:
// orb, grenades, fireballs, acid, totems and turrets, plus burning and poison

#include "g_local.h"

#define	ABILITY_FRAME		50		// think interval for live ability entities
#define	ABILITY_PRESTEP		25		// start 25 ms along the path, as missiles do

void G_DroneEnd( gentity_t *ent, qboolean launch );

/*
==============================================================================

HELPERS

==============================================================================
*/

/*
================
G_AbilityEyes

Origin and direction from the centre of the player's view
================
*/
void G_AbilityEyes( gentity_t *ent, vec3_t start, vec3_t forward, vec3_t right ) {
	VectorCopy( ent->client->ps.origin, start );
	start[2] += ent->client->ps.viewheight;
	AngleVectors( ent->client->ps.viewangles, forward, right, NULL );
	VectorMA( start, 8, forward, start );
	SnapVector( start );
}

/*
================
G_AbilityDone

The game ends a running ability (its projectile is gone or it was used up)
================
*/
void G_AbilityDone( gentity_t *ent ) {
	int		state;

	if ( !ent || !ent->client ) {
		return;
	}
	state = BG_AbilityState( &ent->client->ps );
	if ( state == ABILITY_ACTIVE || state == ABILITY_WINDUP ) {
		BG_SetAbility( &ent->client->ps, ABILITY_IDLE, BG_AbilityCharges( &ent->client->ps ) );
		ent->client->ps.stats[STAT_ABILITY_TIME] = 0;
	}
}

/*
================
G_AbilityEnemy

True if other is a live player the owner may hurt
================
*/
qboolean G_AbilityEnemy( gentity_t *owner, gentity_t *other ) {
	if ( !other->client || other->health <= 0 || other == owner ) {
		return qfalse;
	}
	if ( other->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return qfalse;
	}
	return !OnSameTeam( owner, other );
}

/*
================
G_AbilityMarked / G_AbilityMark

One-time effects per player per entity (contact damage, totem heals)
================
*/
qboolean G_AbilityMarked( gentity_t *ent, int clientNum ) {
	return ( ent->abilityMask[clientNum >> 5] & ( 1 << ( clientNum & 31 ) ) ) != 0;
}

void G_AbilityMark( gentity_t *ent, int clientNum ) {
	ent->abilityMask[clientNum >> 5] |= 1 << ( clientNum & 31 );
}

/*
================
G_AbilityMissile

A thrown or fired ability entity using the missile code for movement
================
*/
gentity_t *G_AbilityMissile( gentity_t *owner, const char *classname, vec3_t start, vec3_t dir,
	float speed, int trType, int weapon ) {
	gentity_t	*ent;

	ent = G_Spawn();
	ent->classname = (char *)classname;
	ent->s.eType = ET_MISSILE;
	ent->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	ent->s.weapon = weapon;
	ent->r.ownerNum = owner->s.number;
	ent->parent = owner;
	ent->clipmask = MASK_SHOT;
	ent->target_ent = NULL;

	ent->s.pos.trType = trType;
	ent->s.pos.trTime = level.time - ABILITY_PRESTEP;
	VectorCopy( start, ent->s.pos.trBase );
	VectorScale( dir, speed, ent->s.pos.trDelta );
	SnapVector( ent->s.pos.trDelta );
	VectorCopy( start, ent->r.currentOrigin );
	return ent;
}

/*
================
G_AbilitySetAngles
================
*/
static void G_AbilitySetAngles( gentity_t *ent, vec3_t angles ) {
	VectorCopy( angles, ent->s.angles );
	VectorCopy( angles, ent->s.apos.trBase );
	ent->s.apos.trType = TR_STATIONARY;
	VectorCopy( angles, ent->r.currentAngles );
}

/*
================
G_AbilityStop

Turns a moving ability entity into a stationary one at point
================
*/
void G_AbilityStop( gentity_t *ent, vec3_t point ) {
	vec3_t	origin;

	VectorCopy( point, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );
	ent->s.pos.trType = TR_STATIONARY;
	VectorClear( ent->s.pos.trDelta );
}

/*
==============================================================================

BURNING AND POISON

Damage over time: a fixed total spread over a fixed time, one point at a
time. Getting hit again restarts the timer; it does not stack.

==============================================================================
*/

/*
================
G_Ignite
================
*/
void G_Ignite( gentity_t *target, gentity_t *attacker ) {
	gclient_t	*client = target->client;

	if ( !client || target->health <= 0 ) {
		return;
	}
	if ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_FIREPROOF ) {
		return;
	}
	client->burnEndTime = level.time + g_burnTime.integer;
	client->burnAttacker = attacker ? attacker->s.number : ENTITYNUM_WORLD;
	if ( client->burnNextTime < level.time ) {
		client->burnNextTime = level.time;
	}
}

/*
================
G_Poison
================
*/
void G_Poison( gentity_t *target, gentity_t *attacker ) {
	gclient_t	*client = target->client;

	if ( !client || target->health <= 0 ) {
		return;
	}
	if ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_ACIDPROOF ) {
		return;
	}
	client->poisonEndTime = level.time + g_poisonTime.integer;
	client->poisonAttacker = attacker ? attacker->s.number : ENTITYNUM_WORLD;
	if ( client->poisonNextTime < level.time ) {
		client->poisonNextTime = level.time;
	}
}

/*
================
G_ClearDamageOverTime

Health pickups and heals put out fire and poison
================
*/
void G_ClearDamageOverTime( gentity_t *ent ) {
	if ( ent->client ) {
		ent->client->burnEndTime = 0;
		ent->client->poisonEndTime = 0;
	}
}

/*
================
G_DamageOverTimeTick
================
*/
static void G_DamageOverTimeTick( gentity_t *ent, int *endTime, int *nextTime, int attackerNum,
	int total, int duration, int mod ) {
	int		interval;

	if ( *endTime <= level.time || total <= 0 || !g_damageOverTime.integer ) {
		return;
	}
	interval = duration / total;
	if ( interval < 1 ) {
		interval = 1;
	}
	while ( *nextTime <= level.time && ent->health > 0 ) {
		*nextTime += interval;
		G_Damage( ent, NULL, &g_entities[attackerNum], NULL, NULL, 1, DAMAGE_NO_KNOCKBACK, mod );
	}
}

/*
==============================================================================

DIRE ORB

Flies straight until it hits a surface, hurts each enemy it passes through
once, and explodes a moment after stopping. Pressing again teleports the
owner to it.

==============================================================================
*/

#define ORB_TELEFRAG_RANGE		32

static void G_OrbExplode( gentity_t *orb ) {
	gentity_t	*owner = orb->parent;

	// no longer shootable, then full in the middle, down to 10 at the edge
	orb->takedamage = qfalse;
	orb->die = NULL;
	orb->r.contents = 0;
	orb->splashDamage = g_orbSplashDamage.integer;
	orb->splashRadius = g_orbSplashRadius.integer;
	orb->splashMin = 10;
	orb->wpTuned = qtrue;
	orb->wpKnock = orb->wpKnockSelf = -1;
	orb->splashMethodOfDeath = MOD_ORB;
	orb->splashSparesOwner = qtrue;
	if ( owner && owner->client && owner->client->abilityEnt == orb ) {
		owner->client->abilityEnt = NULL;
		G_AbilityDone( owner );
	}
	G_ExplodeMissile( orb );
}

static void G_OrbThink( gentity_t *orb ) {
	int			touch[MAX_GENTITIES];
	vec3_t		mins, maxs;
	gentity_t	*other;
	int			i, num;

	// contact damage on enemies it passes through
	VectorSet( mins, -16, -16, -16 );
	VectorSet( maxs, 16, 16, 16 );
	VectorAdd( orb->r.currentOrigin, mins, mins );
	VectorAdd( orb->r.currentOrigin, maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( !G_AbilityEnemy( orb->parent, other ) || G_AbilityMarked( orb, other->s.number ) ) {
			continue;
		}
		G_AbilityMark( orb, other->s.number );
		G_Damage( other, orb, orb->parent, NULL, NULL, g_orbContactDamage.integer, 0, MOD_ORB );
	}

	// abilityTime: end of the flight, then end of the fuse once stopped
	if ( level.time >= orb->abilityTime ) {
		G_OrbExplode( orb );
		return;
	}
	orb->nextthink = level.time + ABILITY_FRAME;
}

/*
================
G_OrbDestroyed

Shot down by another player: a small blast, and its owner gets half the
cooldown back
================
*/
static void G_OrbDestroyed( gentity_t *orb, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	gentity_t	*owner = orb->parent;

	orb->takedamage = qfalse;
	orb->die = NULL;
	if ( !attacker || attacker == owner || !owner || !owner->client || !attacker->client
		|| !G_AbilityEnemy( owner, attacker ) ) {
		orb->health = 1;
		orb->takedamage = qtrue;
		orb->die = G_OrbDestroyed;
		return;		// only an enemy's shots count (champion-audit.md)
	}
	G_RadiusDamage( orb->r.currentOrigin, attacker, g_orbShotDamage.integer, g_orbShotRadius.integer, NULL, MOD_ORB );
	G_CutAbilityCooldown( owner, 0.5f );
	if ( owner->client->abilityEnt == orb ) {
		owner->client->abilityEnt = NULL;
		G_AbilityDone( owner );
	}
	G_AddEvent( orb, EV_MISSILE_MISS, DirToByte( orb->movedir ) );
	orb->s.eType = ET_GENERAL;
	orb->freeAfterEvent = qtrue;
	orb->think = NULL;
	orb->nextthink = 0;
	trap_LinkEntity( orb );
}

static void G_OrbImpact( gentity_t *orb, trace_t *trace ) {
	vec3_t	point;

	// stop just off the surface and arm the fuse
	VectorMA( trace->endpos, 2, trace->plane.normal, point );
	G_AbilityStop( orb, point );
	orb->abilityImpact = NULL;
	orb->abilityTime = level.time + g_orbFuse.integer;
}

static void G_FireOrb( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*orb;

	if ( ent->client->abilityEnt ) {
		G_FreeEntity( ent->client->abilityEnt );
	}
	G_AbilityEyes( ent, start, forward, right );
	orb = G_AbilityMissile( ent, "dire_orb", start, forward, g_orbSpeed.value, TR_LINEAR, WP_BFG );
	orb->s.generic1 = AV_ORB;
	orb->clipmask = MASK_SOLID;			// flies through players
	orb->abilityImpact = G_OrbImpact;
	orb->think = G_OrbThink;
	orb->nextthink = level.time + ABILITY_FRAME;
	orb->abilityTime = level.time + g_orbLife.integer;
	orb->timestamp = level.time;
	// other players can shoot it down
	VectorSet( orb->r.mins, -8, -8, -8 );
	VectorSet( orb->r.maxs, 8, 8, 8 );
	orb->r.contents = CONTENTS_CORPSE;
	orb->takedamage = qtrue;
	orb->health = 1;
	orb->die = G_OrbDestroyed;
	VectorCopy( forward, orb->movedir );
	ent->client->abilityEnt = orb;
}

/*
================
G_OrbTeleport

Moves the owner to the orb, stepping back along its path out of walls.
An enemy at the spot takes telefrag damage.
================
*/
static void G_OrbTeleport( gentity_t *ent ) {
	gentity_t	*orb = ent->client->abilityEnt;

	gentity_t	*tent, *other;
	trace_t		tr;
	vec3_t		spot, mins, maxs;
	int			touch[MAX_GENTITIES];
	int			i, num;

	if ( !orb ) {
		G_AbilityDone( ent );
		return;
	}

	VectorCopy( orb->r.currentOrigin, spot );
	for ( i = 0 ; i < 16 ; i++ ) {
		trap_Trace( &tr, spot, ent->r.mins, ent->r.maxs, spot, ent->s.number, MASK_PLAYERSOLID & ~CONTENTS_BODY );
		if ( !tr.startsolid && !tr.allsolid ) {
			break;
		}
		VectorMA( spot, -16, orb->movedir, spot );
		spot[2] += 4;
	}
	if ( i == 16 ) {
		return;		// nowhere to stand; keep the orb
	}

	// inhuman haste: faster for a while after each teleport that happens
	if ( BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_HASTE ) {
		ent->client->hasteEndTime = level.time + 3000;
	}

	// a carried flag stays behind where the carrier was
	G_DropCarriedFlag( ent, qfalse );

	tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
	tent->s.clientNum = ent->s.clientNum;
	tent->s.eventParm = 1;		// the orb's rings
	tent = G_TempEntity( spot, EV_PLAYER_TELEPORT_IN );
	tent->s.clientNum = ent->s.clientNum;
	tent->s.eventParm = 1;

	trap_UnlinkEntity( ent );
	VectorCopy( spot, ent->client->ps.origin );
	VectorClear( ent->client->ps.velocity );
	ent->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// telefrag enemies within 32 u of the spot
	VectorSet( mins, spot[0] - ORB_TELEFRAG_RANGE, spot[1] - ORB_TELEFRAG_RANGE, spot[2] - ORB_TELEFRAG_RANGE );
	VectorSet( maxs, spot[0] + ORB_TELEFRAG_RANGE, spot[1] + ORB_TELEFRAG_RANGE, spot[2] + ORB_TELEFRAG_RANGE );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( G_AbilityEnemy( ent, other ) && Distance( other->r.currentOrigin, spot ) <= ORB_TELEFRAG_RANGE ) {
			G_Damage( other, ent, ent, NULL, NULL, g_orbTelefragDamage.integer, DAMAGE_NO_PROTECTION, MOD_ORBTELEFRAG );
		}
	}

	BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );

	ent->client->abilityEnt = NULL;
	G_FreeEntity( orb );
	G_AbilityDone( ent );

	// the teleport starts the throw's firing lockout again
	if ( ent->client->ps.weaponTime < BG_AbilityParam( ACTIVE_ORB, AP_LOCKOUT ) ) {
		ent->client->ps.weaponTime = BG_AbilityParam( ACTIVE_ORB, AP_LOCKOUT );
	}
}

/*
==============================================================================

GRENADE SWARM

==============================================================================
*/

/*
================
G_GrenadeImpact

Walls and floors bounce it, and its second bounce bursts it
(G_BounceMissile counts them); any player bursts it
================
*/
static void G_GrenadeImpact( gentity_t *gren, trace_t *trace ) {
	gentity_t	*other = &g_entities[trace->entityNum];

	if ( !other->takedamage ) {
		G_BounceMissile( gren, trace );
		G_AddEvent( gren, EV_GRENADE_BOUNCE, 0 );
		return;
	}
	gren->abilityImpact = NULL;
	gren->s.eFlags &= ~( EF_BOUNCE | EF_BOUNCE_HALF );
	G_MissileImpact( gren, trace );
}

static void G_ThrowGrenade( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*gren;

	G_AbilityEyes( ent, start, forward, right );
	forward[2] += 0.1f;		// the aim lifted a little (champion-audit.md)
	VectorNormalize( forward );
	gren = fire_grenade( ent, start, forward );
	gren->classname = "swarm";		// bursts on its second bounce (G_BounceMissile)
	gren->count = 0;
	gren->s.pos.trType = TR_GRAVITY_KEEL;	// retune.md: gravity 560
	VectorScale( forward, g_swarmSpeed.value, gren->s.pos.trDelta );
	SnapVector( gren->s.pos.trDelta );
	gren->nextthink = level.time + g_swarmFuse.integer;
	gren->damage = g_swarmDamage.integer;
	gren->splashDamage = g_swarmDamage.integer;
	gren->splashRadius = g_swarmRadius.integer;
	gren->wpTuned = qtrue;
	gren->splashMin = g_swarmDamage.integer * 0.2f;		// full to 12 u, then down to 20%
	gren->splashFull = 12;
	// the splash hits Keel too, pushing him three times as hard, so
	// grenade jumps work (champion-audit.md, from the data)
	gren->splashSparesOwner = qfalse;
	gren->wpKnock = g_swarmKnock.value;
	gren->wpKnockSelf = g_swarmKnock.value * 3.0f;
	gren->abilityImpact = G_GrenadeImpact;
	gren->count = 0;
}

/*
==============================================================================

FLAME STRIKE

Three fireballs in a fan. Each burns what it hits. Pressing again before
they land pulls them together into one bigger explosion.

==============================================================================
*/

#define FIREBALL_IMPULSE	30

static void G_FireballBurst( gentity_t *ball, vec3_t origin, int damage, int radius, gentity_t *ignore ) {
	int			touch[MAX_GENTITIES];
	vec3_t		mins, maxs;
	gentity_t	*other;
	int			i, num;

	// 100% at the centre to 40% at the edge; the push from an impulse of 30,
	// Death Knight pushed x1.467 by his own
	G_SplashParams( damage * 0.4f, FIREBALL_IMPULSE, FIREBALL_IMPULSE * 1.467f );
	G_RadiusDamage( origin, ball->parent, damage, radius, ignore, MOD_FIREBALL );		// (not the one hit directly)

	// set everything in the blast burning
	VectorSet( mins, -radius, -radius, -radius );
	VectorSet( maxs, radius, radius, radius );
	VectorAdd( origin, mins, mins );
	VectorAdd( origin, maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( other->client && Distance( origin, other->r.currentOrigin ) <= radius ) {
			G_Ignite( other, ball->parent );
		}
	}
}

static void G_FireballFinish( gentity_t *ball, vec3_t origin ) {
	gentity_t	*owner = ball->parent;
	vec3_t		dir = { 0, 0, 1 };
	int			i;

	G_SetOrigin( ball, origin );
	ball->s.eType = ET_GENERAL;
	G_AddEvent( ball, EV_MISSILE_MISS, DirToByte( dir ) );
	ball->freeAfterEvent = qtrue;
	ball->abilityImpact = NULL;
	ball->think = NULL;
	trap_LinkEntity( ball );

	if ( owner && owner->client ) {
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( owner->client->fireballs[i] == ball ) {
				owner->client->fireballs[i] = NULL;
			}
		}
		if ( !owner->client->fireballs[0] && !owner->client->fireballs[1] && !owner->client->fireballs[2] ) {
			G_AbilityDone( owner );
		}
	}
}

static void G_FireballImpact( gentity_t *ball, trace_t *trace ) {
	gentity_t	*other = &g_entities[trace->entityNum];

	if ( other->takedamage ) {
		G_NextKnockback( FIREBALL_IMPULSE );
		G_Damage( other, ball, ball->parent, NULL, trace->endpos, g_fireballDamage.integer, 0, MOD_FIREBALL );
		if ( other->client ) {
			G_Ignite( other, ball->parent );
		}
	}
	G_FireballBurst( ball, trace->endpos, g_fireballSplash.integer, g_fireballRadius.integer,
		other->takedamage ? other : NULL );
	G_FireballFinish( ball, trace->endpos );
}

static void G_FireballExpire( gentity_t *ball ) {
	vec3_t	origin;

	BG_EvaluateTrajectory( &ball->s.pos, level.time, origin );
	G_FireballBurst( ball, origin, g_fireballSplash.integer, g_fireballRadius.integer, NULL );
	G_FireballFinish( ball, origin );
}

/*
================
G_GroundFire

champion-audit.md: fireballs leave a fire trail on the floor, laid only
within 200 u of it; each touch burns an enemy for 6, as a burn (once per
patch per player)
================
*/
#define GROUNDFIRE_REACH	200
#define GROUNDFIRE_STEP		100		// msec between patches along the flight
#define GROUNDFIRE_LIFE		3000
#define GROUNDFIRE_RADIUS	32
#define GROUNDFIRE_DAMAGE	6
#define GROUNDFIRE_MAX		24		// patches alive per Death Knight

static void G_GroundFireThink( gentity_t *fire ) {
	int			touch[MAX_GENTITIES], num, i;
	vec3_t		mins, maxs;
	gentity_t	*other;

	if ( level.time >= fire->abilityTime || !fire->parent || !fire->parent->client ) {
		G_FreeEntity( fire );
		return;
	}
	fire->nextthink = level.time + ABILITY_FRAME;
	VectorSet( mins, -GROUNDFIRE_RADIUS, -GROUNDFIRE_RADIUS, 0 );
	VectorSet( maxs, GROUNDFIRE_RADIUS, GROUNDFIRE_RADIUS, 48 );
	VectorAdd( fire->r.currentOrigin, mins, mins );
	VectorAdd( fire->r.currentOrigin, maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( !other->client || other->health <= 0 || !G_AbilityEnemy( fire->parent, other )
			|| G_AbilityMarked( fire, other->s.number ) ) {
			continue;
		}
		if ( BG_Champion( other->client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_FIREPROOF ) {
			continue;
		}
		G_AbilityMark( fire, other->s.number );
		G_Damage( other, fire, fire->parent, NULL, NULL, GROUNDFIRE_DAMAGE, DAMAGE_NO_KNOCKBACK, MOD_BURN );
	}
}

static void G_FireballThink( gentity_t *ball ) {
	vec3_t		origin, down;
	trace_t		tr;
	gentity_t	*fire;

	if ( level.time >= ball->abilityTime ) {
		G_FireballExpire( ball );
		return;
	}
	ball->nextthink = level.time + GROUNDFIRE_STEP;
	BG_EvaluateTrajectory( &ball->s.pos, level.time, origin );
	VectorCopy( origin, down );
	down[2] -= GROUNDFIRE_REACH;
	trap_Trace( &tr, origin, NULL, NULL, down, ball->s.number, MASK_SOLID );
	if ( tr.fraction >= 1 || tr.startsolid || tr.plane.normal[2] < 0.7f ) {
		return;		// too high above the floor, or no floor
	}
	if ( !ball->parent || !ball->parent->client ) {
		return;
	}
	// at most GROUNDFIRE_MAX patches per Death Knight: the oldest goes
	{
		gentity_t	*other, *oldest = NULL;
		int			i, count = 0;

		for ( i = MAX_CLIENTS, other = &g_entities[MAX_CLIENTS] ; i < level.num_entities ; i++, other++ ) {
			if ( other->inuse && other->parent == ball->parent && other->classname && !strcmp( other->classname, "ground_fire" ) ) {
				count++;
				if ( !oldest || other->abilityTime < oldest->abilityTime ) {
					oldest = other;
				}
			}
		}
		if ( count >= GROUNDFIRE_MAX && oldest ) {
			G_FreeEntity( oldest );
		}
	}
	// a stationary ability missile, drawn by its visual (as the plasma trail)
	VectorSet( down, 0, 0, 1 );
	fire = G_AbilityMissile( ball->parent, "ground_fire", tr.endpos, down, 0, TR_STATIONARY, WP_ROCKET_LAUNCHER );
	fire->s.generic1 = AV_GROUNDFIRE;
	fire->clipmask = 0;
	fire->abilityImpact = NULL;
	fire->abilityTime = level.time + GROUNDFIRE_LIFE;
	fire->think = G_GroundFireThink;
	fire->nextthink = level.time + ABILITY_FRAME;
}

static void G_FireFlameStrike( gentity_t *ent ) {
	vec3_t		start, forward, right, dir;
	gentity_t	*ball;
	int			i;
	float		spread;

	G_AbilityEyes( ent, start, forward, right );
	spread = tan( DEG2RAD( g_fireballSpread.value ) );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( forward, ( i - 1 ) * spread, right, dir );
		VectorNormalize( dir );
		ball = G_AbilityMissile( ent, "fireball", start, dir, g_fireballSpeed.value, TR_LINEAR, WP_ROCKET_LAUNCHER );
		ball->s.generic1 = AV_FIREBALL;
		ball->abilityImpact = G_FireballImpact;
		ball->think = G_FireballThink;		// lays fire on the floor below as it flies
		ball->nextthink = level.time + GROUNDFIRE_STEP;
		ball->abilityTime = level.time + 10000;
		ent->client->fireballs[i] = ball;
	}
}

/*
================
G_FlameConverge

Re-press: all fireballs meet at the middle one and explode together
================
*/
static void G_FlameConverge( gentity_t *ent ) {
	gentity_t	*ball;
	vec3_t		point, origin;
	int			i, count;

	count = 0;
	VectorClear( point );
	for ( i = 0 ; i < 3 ; i++ ) {
		ball = ent->client->fireballs[i];
		if ( ball ) {
			BG_EvaluateTrajectory( &ball->s.pos, level.time, origin );
			VectorAdd( point, origin, point );
			count++;
		}
	}
	if ( !count ) {
		G_AbilityDone( ent );
		return;
	}
	VectorScale( point, 1.0f / count, point );

	ball = NULL;
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( ent->client->fireballs[i] ) {
			if ( !ball ) {
				ball = ent->client->fireballs[i];
			} else {
				G_FreeEntity( ent->client->fireballs[i] );
				ent->client->fireballs[i] = NULL;
			}
		}
	}
	G_FireballBurst( ball, point, g_flameConvergeDamage.integer, g_flameConvergeRadius.integer, NULL );
	G_FireballFinish( ball, point );
}

/*
==============================================================================

ACID SPIT

A few acid blobs in quick succession. A blob hurts and pushes what it hits
and leaves a pool where it lands; touching acid poisons.

==============================================================================
*/

static void G_AcidPoolThink( gentity_t *pool ) {
	int			touch[MAX_GENTITIES];
	vec3_t		mins, maxs;
	gentity_t	*other;
	int			i, num;

	if ( level.time >= pool->abilityTime ) {
		G_FreeEntity( pool );
		return;
	}
	VectorAdd( pool->r.currentOrigin, pool->r.mins, mins );
	VectorAdd( pool->r.currentOrigin, pool->r.maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( other->client && other->health > 0 ) {
			G_Poison( other, pool->parent );
		}
	}
	pool->nextthink = level.time + ABILITY_FRAME * 2;
}

#define ACID_IMPULSE	10		// a blob's push

static void G_AcidImpact( gentity_t *blob, trace_t *trace ) {
	gentity_t	*other = &g_entities[trace->entityNum];
	vec3_t		point, velocity;
	float		size = g_acidPoolSize.value;

	if ( other->takedamage ) {
		BG_EvaluateTrajectoryDelta( &blob->s.pos, level.time, velocity );
		G_NextKnockback( ACID_IMPULSE );
		G_Damage( other, blob, blob->parent, velocity, trace->endpos, g_acidDamage.integer, 0, MOD_ACID );
		if ( other->client ) {
			G_Poison( other, blob->parent );
		}
	}

	// leave a pool where it landed; a hit player's pool forms on the floor
	// below the hit (champion-audit.md)
	VectorMA( trace->endpos, 1, trace->plane.normal, point );
	if ( other->client ) {
		trace_t	down;
		vec3_t	end;

		VectorCopy( trace->endpos, end );
		end[2] -= 4096;
		trap_Trace( &down, trace->endpos, NULL, NULL, end, other->s.number, MASK_SOLID );
		if ( down.fraction < 1 ) {
			VectorMA( down.endpos, 1, down.plane.normal, point );
		} else {
			G_FreeEntity( blob );		// no floor below: no pool
			return;
		}
	}
	G_AbilityStop( blob, point );
	blob->abilityImpact = NULL;
	blob->clipmask = 0;
	VectorSet( blob->r.mins, -size, -size, -size );
	VectorSet( blob->r.maxs, size, size, size );
	blob->abilityTime = level.time + g_acidPoolTime.integer;
	blob->think = G_AcidPoolThink;
	blob->nextthink = level.time + ABILITY_FRAME;
	trap_LinkEntity( blob );
}

static void G_SpitAcid( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*blob;

	G_AbilityEyes( ent, start, forward, right );
	blob = G_AbilityMissile( ent, "acid", start, forward, g_acidSpeed.value, TR_GRAVITY_ACID, WP_PLASMAGUN );	// gravity 600
	blob->s.generic1 = AV_ACID;
	blob->abilityImpact = G_AcidImpact;
	blob->think = G_FreeEntity;
	blob->nextthink = level.time + 10000;
}

/*
==============================================================================

UNHOLY TOTEM

Thrown; arms after a moment. It heals each ally once nearby and bursts on
the first enemy that comes close. With the maximum number out, all of
them are stronger.

==============================================================================
*/

static int G_CountTotems( gentity_t *owner, gentity_t **oldest ) {
	gentity_t	*ent;
	int			i, count = 0;

	*oldest = NULL;
	for ( i = MAX_CLIENTS, ent = &g_entities[MAX_CLIENTS] ; i < level.num_entities ; i++, ent++ ) {
		if ( ent->inuse && ent->parent == owner && ent->classname && !strcmp( ent->classname, "totem" ) ) {
			if ( !*oldest || ent->abilityTime < ( *oldest )->abilityTime ) {
				*oldest = ent;
			}
			count++;
		}
	}
	return count;
}

static void G_TotemDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	G_FreeEntity( self );
}

/*
================
G_TotemBurst

An enemy came close: every enemy within the blast radius takes the hit
================
*/
static void G_TotemBurst( gentity_t *totem, int damage ) {
	gentity_t	*other;
	int			i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !other->inuse || !other->client || other->health <= 0 || !G_AbilityEnemy( totem->parent, other ) ) {
			continue;
		}
		if ( Distance( totem->r.currentOrigin, other->r.currentOrigin ) <= g_totemBlastRadius.value ) {
			trace_t	los;

			// each victim needs a clear line from the totem (champion-audit.md)
			trap_Trace( &los, totem->r.currentOrigin, NULL, NULL, other->r.currentOrigin, totem->s.number, MASK_SOLID );
			if ( los.fraction == 1.0f ) {
				G_Damage( other, totem, totem->parent, NULL, NULL, damage, 0, MOD_TOTEM );
			}
		}
	}
	G_FreeEntity( totem );
}

static void G_TotemThink( gentity_t *totem ) {
	int			touch[MAX_GENTITIES];
	vec3_t		mins, maxs;
	gentity_t	*other, *oldest;
	int			i, num, heal, max, enforced;
	float		dist;

	totem->nextthink = level.time + ABILITY_FRAME * 2;
	if ( level.time < totem->abilityTime + g_totemArmTime.integer || !totem->parent || !totem->parent->client ) {
		return;
	}
	enforced = G_CountTotems( totem->parent, &oldest ) >= g_totemMax.integer;

	VectorSet( mins, -g_totemHealRadius.value, -g_totemHealRadius.value, -g_totemHealRadius.value );
	VectorSet( maxs, g_totemHealRadius.value, g_totemHealRadius.value, g_totemHealRadius.value );
	VectorAdd( totem->r.currentOrigin, mins, mins );
	VectorAdd( totem->r.currentOrigin, maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( !other->client || other->health <= 0 ) {
			continue;
		}
		dist = Distance( totem->r.currentOrigin, other->r.currentOrigin );
		if ( G_AbilityEnemy( totem->parent, other ) ) {
			trace_t	los;

			// the burst needs a clear line from the totem (champion-audit.md)
			trap_Trace( &los, totem->r.currentOrigin, NULL, NULL, other->r.currentOrigin, totem->s.number, MASK_SOLID );
			if ( dist <= g_totemHurtRadius.value && los.fraction == 1.0f ) {
				G_TotemBurst( totem, enforced ? g_totemEnforcedDamage.integer : g_totemDamage.integer );
				return;
			}
		} else if ( !G_AbilityMarked( totem, other->s.number ) ) {
			// one heal per ally per totem
			max = other->client->ps.stats[STAT_MAX_HEALTH];
			if ( other->health >= max ) {
				continue;
			}
			G_AbilityMark( totem, other->s.number );
			heal = enforced ? g_totemEnforcedHeal.integer : g_totemHeal.integer;
			other->health += heal;
			if ( other->health > max ) {
				other->health = max;
			}
			other->client->ps.stats[STAT_HEALTH] = other->health;
			G_ClearDamageOverTime( other );
		}
	}
}

static void G_TotemImpact( gentity_t *totem, trace_t *trace ) {
	vec3_t	point;

	VectorMA( trace->endpos, 2, trace->plane.normal, point );
	G_AbilityStop( totem, point );
	totem->abilityImpact = NULL;
	totem->s.eType = ET_GENERAL;
	totem->s.generic1 = AV_NONE;
	totem->s.modelindex = G_ModelIndex( "models/abilities/totem/totem.md3" );
	totem->r.contents = CONTENTS_CORPSE;
	VectorSet( totem->r.mins, -8, -8, -8 );
	VectorSet( totem->r.maxs, 8, 8, 16 );
	totem->takedamage = qtrue;
	totem->health = g_totemHealth.integer;
	totem->die = G_TotemDie;
	totem->abilityTime = level.time;
	totem->think = G_TotemThink;
	totem->nextthink = level.time + ABILITY_FRAME;
	trap_LinkEntity( totem );
}

static void G_ThrowTotem( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*totem, *oldest;

	if ( G_CountTotems( ent, &oldest ) >= g_totemMax.integer && oldest ) {
		G_FreeEntity( oldest );
	}
	G_AbilityEyes( ent, start, forward, right );
	totem = G_AbilityMissile( ent, "totem", start, forward, g_totemSpeed.value, TR_GRAVITY, WP_GRENADE_LAUNCHER );
	totem->clipmask = MASK_SOLID;
	totem->s.generic1 = AV_THROWN;
	totem->s.modelindex = G_ModelIndex( "models/abilities/totem/totem.md3" );
	totem->abilityImpact = G_TotemImpact;
	totem->abilityTime = level.time;
	totem->think = G_FreeEntity;
	totem->nextthink = level.time + 10000;
}

/*
==============================================================================

SENTRY TURRET

Stands where it was placed, turns to the nearest visible enemy and fires
bolts at it until destroyed.

==============================================================================
*/

/*
================
G_OwnTurrets

Each Eisen's turrets, standing or still in the air, found by owner (a
client's own pointer would be lost on respawn)
================
*/
static int G_OwnTurrets( gentity_t *owner, gentity_t **oldest ) {
	gentity_t	*ent;
	int			i, count = 0;

	*oldest = NULL;
	for ( i = MAX_CLIENTS, ent = &g_entities[MAX_CLIENTS] ; i < level.num_entities ; i++, ent++ ) {
		if ( ent->inuse && ent->parent == owner && ent->classname
			&& ( !strcmp( ent->classname, "sentry_turret" ) || !strcmp( ent->classname, "turret_thrown" ) ) ) {
			if ( !*oldest || ent->s.time < ( *oldest )->s.time ) {
				*oldest = ent;
			}
			count++;
		}
	}
	return count;
}

static void G_TurretDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	gentity_t	*tent;

	tent = G_TempEntity( self->r.currentOrigin, EV_GENERAL_SOUND );
	tent->s.eventParm = G_SoundIndex( "sound/abilities/turret_destroyed.wav" );
	tent = G_TempEntity( self->r.currentOrigin, EV_MISSILE_MISS );
	tent->s.weapon = WP_PLASMAGUN;		// a small flash, not a rocket's blast
	tent->s.eventParm = DirToByte( vec3_origin );
	if ( self->parent && self->parent->client && self->parent->client->turret == self ) {
		self->parent->client->turret = NULL;
	}
	G_FreeEntity( self );
}

#define TURRET_FOV		140
#define TURRET_IMPULSE	10
#define TURRET_ARMOR	50		// absorbs two thirds of each hit until spent
#define TURRET_EYE_UP	14		// thrown from 0.35 m above the eye

static void G_TurretThink( gentity_t *turret ) {
	gentity_t	*other, *best = NULL;
	trace_t		tr;
	vec3_t		muzzle, target, dir, angles;
	float		dist, bestDist = g_turretRange.value;
	int			i;

	turret->nextthink = level.time + ABILITY_FRAME;
	// it outlives Eisen's death; it goes when he leaves the game
	if ( !turret->parent || !turret->parent->client || !turret->parent->inuse
		|| turret->parent->client->pers.connected != CON_CONNECTED ) {
		G_TurretDie( turret, NULL, NULL, 0, 0 );
		return;
	}
	// armed: a chirp, then it hums while it stands
	if ( !turret->s.loopSound && level.time >= turret->s.time + g_turretDeployTime.integer ) {
		G_AddEvent( turret, EV_GENERAL_SOUND, G_SoundIndex( "sound/abilities/turret_online.wav" ) );
		turret->s.loopSound = G_SoundIndex( "sound/abilities/turret_idle.wav" );
	}
	if ( level.time < turret->s.time + g_turretDeployTime.integer ) {
		return;
	}

	VectorCopy( turret->r.currentOrigin, muzzle );
	muzzle[2] += 12;
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !other->inuse || !G_AbilityEnemy( turret->parent, other ) ) {
			continue;
		}
		if ( other->client->ps.powerups[PW_INVIS] ) {
			continue;
		}
		VectorCopy( other->r.currentOrigin, target );
		target[2] += 16;
		dist = Distance( muzzle, target );
		if ( dist >= bestDist ) {
			continue;
		}
		// it watches a 140 degree arc around the way it was set down
		VectorSubtract( target, muzzle, dir );
		vectoangles( dir, angles );
		if ( fabs( AngleSubtract( angles[YAW], turret->movedir[YAW] ) ) > TURRET_FOV / 2 ) {
			continue;
		}
		trap_Trace( &tr, muzzle, NULL, NULL, target, turret->s.number, MASK_SHOT );
		if ( tr.entityNum != other->s.number ) {
			continue;
		}
		best = other;
		bestDist = dist;
	}
	// a new target: the acquire beeps
	if ( best && best != turret->enemy ) {
		G_AddEvent( turret, EV_GENERAL_SOUND, G_SoundIndex( "sound/abilities/turret_target.wav" ) );
	}
	turret->enemy = best;
	if ( !best ) {
		return;
	}

	VectorCopy( best->r.currentOrigin, target );
	target[2] += 16;
	VectorSubtract( target, muzzle, dir );
	VectorNormalize( dir );
	vectoangles( dir, angles );
	{
		// turn toward the target at the turn rate
		float	step = g_turretTurnRate.value * ABILITY_FRAME / 1000.0f;
		float	delta = AngleSubtract( angles[YAW], turret->s.angles[YAW] );
		float	aimed;

		if ( delta > step ) {
			delta = step;
		} else if ( delta < -step ) {
			delta = -step;
		}
		aimed = AngleSubtract( angles[YAW], turret->s.angles[YAW] + delta );
		angles[YAW] = AngleMod( turret->s.angles[YAW] + delta );
		G_AbilitySetAngles( turret, angles );
		if ( fabs( aimed ) > 10 ) {
			return;
		}
		AngleVectors( angles, dir, NULL, NULL );
	}

	if ( level.time >= turret->abilityTime ) {
		gentity_t	*bolt;

		turret->abilityTime = level.time + g_turretInterval.integer;
		VectorMA( muzzle, 20, dir, muzzle );	// start outside the turret's own box
		bolt = fire_plasma( turret->parent, muzzle, dir );
		bolt->damage = g_turretDamage.integer;
		bolt->splashDamage = 0;
		bolt->wpTuned = qtrue;		// impulse 10: x0.9 on others, x1.5 on Eisen (champion-audit.md)
		bolt->wpKnock = TURRET_IMPULSE * 0.9f;
		bolt->wpKnockSelf = TURRET_IMPULSE * 1.5f;
		bolt->methodOfDeath = MOD_TURRET;
		bolt->splashMethodOfDeath = MOD_TURRET;
		VectorScale( dir, g_turretBoltSpeed.value, bolt->s.pos.trDelta );
		SnapVector( bolt->s.pos.trDelta );
		G_AddEvent( turret, EV_GENERAL_SOUND, G_SoundIndex( "sound/abilities/turret_shot.wav" ) );
	}
}

/*
================
G_TurretImpact

The thrown turret stands up where it lands on a floor; on a wall or a
steep slope it drops on down
================
*/
static void G_TurretImpact( gentity_t *turret, trace_t *trace ) {
	vec3_t		point, mins, maxs;
	trace_t		tr;

	VectorMA( trace->endpos, 2, trace->plane.normal, point );
	if ( trace->plane.normal[2] < 0.7f ) {
		G_AbilityStop( turret, point );
		turret->s.pos.trType = TR_GRAVITY;
		turret->s.pos.trTime = level.time;
		return;
	}
	// the 24 u box on the floor, a step edge or a low ceiling allowing
	VectorSet( mins, -12, -12, 0 );
	VectorSet( maxs, 12, 12, 24 );
	VectorCopy( trace->endpos, point );
	point[2] += 1;
	trap_Trace( &tr, point, mins, maxs, point, turret->s.number, MASK_PLAYERSOLID );
	if ( tr.startsolid ) {
		// on a step's lip: settle onto the floor below its middle
		vec3_t	down;

		VectorCopy( trace->endpos, point );
		point[2] += 18;
		VectorCopy( point, down );
		down[2] -= 64;
		trap_Trace( &tr, point, mins, maxs, down, turret->s.number, MASK_PLAYERSOLID );
		if ( !tr.startsolid && !tr.allsolid && tr.fraction < 1.0f ) {
			VectorCopy( tr.endpos, point );
		} else {
			VectorCopy( trace->endpos, point );
		}
	}

	turret->abilityImpact = NULL;
	turret->classname = "sentry_turret";
	turret->s.eType = ET_GENERAL;
	turret->s.generic1 = AV_NONE;
	turret->s.otherEntityNum = turret->parent->s.number;		// drawn in its owner's colours
	turret->r.contents = CONTENTS_CORPSE;
	VectorCopy( mins, turret->r.mins );
	VectorCopy( maxs, turret->r.maxs );
	turret->takedamage = qtrue;
	turret->health = g_turretHealth.integer;
	turret->count = TURRET_ARMOR;		// its armour (G_Damage)
	turret->die = G_TurretDie;
	turret->think = G_TurretThink;
	turret->nextthink = level.time + ABILITY_FRAME;
	turret->s.time = level.time;		// deploying until s.time + g_turretDeployTime
	turret->abilityTime = 0;
	turret->enemy = NULL;
	G_AbilityStop( turret, point );
	{
		vec3_t	angles;

		VectorSet( angles, 0, turret->movedir[YAW], 0 );
		G_AbilitySetAngles( turret, angles );
	}
	trap_LinkEntity( turret );
	G_AddEvent( turret, EV_GENERAL_SOUND, G_SoundIndex( "sound/abilities/turret_deploy.wav" ) );
	turret->parent->client->turret = turret;
}

/*
================
G_TurretLost

Thrown and never landed: the charge comes back
================
*/
static void G_TurretLost( gentity_t *turret ) {
	gentity_t	*owner = turret->parent;

	if ( owner && owner->inuse && owner->client ) {
		BG_SetAbility( &owner->client->ps, BG_AbilityState( &owner->client->ps ),
			BG_AbilityCharges( &owner->client->ps ) + 1 );
	}
	G_FreeEntity( turret );
}

/*
================
G_ThrowTurret

Thrown from the eye like the totem; deploys where it lands (konstantin-list-
audit 2). One per Eisen: the next replaces it.
================
*/
static void G_ThrowTurret( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*turret, *oldest;

	while ( G_OwnTurrets( ent, &oldest ) >= 1 && oldest ) {
		G_TurretDie( oldest, NULL, NULL, 0, 0 );
	}
	G_AbilityEyes( ent, start, forward, right );
	start[2] += TURRET_EYE_UP;
	turret = G_AbilityMissile( ent, "turret_thrown", start, forward, g_totemSpeed.value, TR_GRAVITY, WP_GRENADE_LAUNCHER );
	turret->clipmask = MASK_SOLID;
	turret->s.generic1 = AV_THROWN;
	turret->s.modelindex = G_ModelIndex( "models/abilities/turret/turret.md3" );
	turret->s.angles[YAW] = ent->client->ps.viewangles[YAW];
	VectorCopy( ent->client->ps.viewangles, turret->movedir );	// the middle of its view
	turret->s.time = level.time;
	turret->abilityImpact = G_TurretImpact;
	turret->think = G_TurretLost;
	turret->nextthink = level.time + 10000;
}

/*
================
G_RemoveAbilityObjects

The turrets and totems a player has out (all of them on a team change;
on a champion change all but keepActive's; owner NULL: everyone's)
================
*/
void G_RemoveAbilityObjects( gentity_t *owner, int keepActive ) {
	static const char *names[] = { "sentry_turret", "turret_thrown", "totem", NULL };
	static const int actives[] = { ACTIVE_TURRET, ACTIVE_TURRET, ACTIVE_TOTEM };
	gentity_t	*ent;
	int			i, n;

	for ( i = MAX_CLIENTS, ent = &g_entities[MAX_CLIENTS] ; i < level.num_entities ; i++, ent++ ) {
		if ( !ent->inuse || !ent->classname || ( owner && ent->parent != owner ) ) {
			continue;
		}
		for ( n = 0 ; names[n] ; n++ ) {
			if ( !strcmp( ent->classname, names[n] ) && actives[n] != keepActive ) {
				if ( owner && owner->client && owner->client->turret == ent ) {
					owner->client->turret = NULL;
				}
				G_FreeEntity( ent );
				break;
			}
		}
	}
}

/*
==============================================================================

BARRIER AND MINING LASER

The shield halves incoming weapon damage. Pressing again turns the time
left into a laser that hits what the player looks at.

==============================================================================
*/

static void G_StartBarrier( gentity_t *ent ) {
	ent->client->barrierMode = 1;
	ent->client->barrierDamage = 0;
	ent->client->ps.eFlags |= EF_BARRIER;
}

static void G_BarrierToLaser( gentity_t *ent ) {
	if ( ent->client->barrierMode != 1 || BG_AbilityState( &ent->client->ps ) != ABILITY_ACTIVE ) {
		return;		// (a press in the shield's last frame, already ended)
	}
	// abilities.md Clutch Barrier fixes: the shield drops and the laser
	// fires at once, always for its full time (the ability runs on past its
	// 8 s if need be) with the weapons locked until it ends
	ent->client->barrierMode = 2;
	ent->client->ps.eFlags &= ~EF_BARRIER;
	ent->client->laserTime = level.time + g_laserDelay.integer;
	ent->client->laserEnd = ent->client->laserTime + g_laserTime.integer;
	if ( ent->client->ps.stats[STAT_ABILITY_TIME] < g_laserTime.integer + g_laserDelay.integer + 100 ) {
		ent->client->ps.stats[STAT_ABILITY_TIME] = g_laserTime.integer + g_laserDelay.integer + 100;
	}
	if ( ent->client->ps.weaponTime < g_laserTime.integer + g_laserDelay.integer ) {
		ent->client->ps.weaponTime = g_laserTime.integer + g_laserDelay.integer;
	}
}

#define LASER_RADIUS	10		// the beam's sweep, abilities.md

static void G_FireLaser( gentity_t *ent ) {
	vec3_t		start, forward, right, end;
	vec3_t		mins = { -LASER_RADIUS, -LASER_RADIUS, -LASER_RADIUS }, maxs = { LASER_RADIUS, LASER_RADIUS, LASER_RADIUS };
	trace_t		tr;
	gentity_t	*tent, *other;

	G_AbilityEyes( ent, start, forward, right );
	VectorMA( start, g_laserRange.value, forward, end );
	G_RewindPlayers( ent );
	G_ShotTrace( &tr, start, mins, maxs, end, ent->s.number, MASK_SHOT );		// a 10 u wide beam
	G_RestorePlayers();

	tent = G_TempEntity( tr.endpos, EV_RAILTRAIL );
	VectorCopy( start, tent->s.origin2 );
	tent->s.clientNum = ent->s.clientNum;

	other = &g_entities[tr.entityNum];
	if ( other->takedamage ) {
		G_Damage( other, ent, ent, forward, tr.endpos, g_laserDamage.integer, DAMAGE_NO_KNOCKBACK, MOD_LASER );
	}
}

/*
================
G_BarrierHit

The shield breaks after taking enough damage
================
*/
void G_BarrierHit( gentity_t *targ, int damage ) {
	gclient_t	*client = targ->client;

	if ( !client || client->barrierMode != 1 || !G_AbilityActive( targ, ACTIVE_BARRIER ) ) {
		return;
	}
	client->barrierDamage += damage;
	if ( client->barrierDamage >= g_barrierHealth.integer ) {
		BG_SetAbility( &client->ps, ABILITY_IDLE, BG_AbilityCharges( &client->ps ) );
		client->ps.stats[STAT_ABILITY_TIME] = 0;
		G_AbilityEnded( targ );
	}
}

/*
================
G_BarrierScale

Damage multiplier while the shield is up; only weapon and ability damage
================
*/
float G_BarrierScale( gentity_t *targ, int mod ) {
	if ( !targ->client || targ->client->barrierMode != 1 || !G_AbilityActive( targ, ACTIVE_BARRIER ) ) {
		return 1.0f;
	}
	switch ( mod ) {
	case MOD_WATER: case MOD_SLIME: case MOD_LAVA: case MOD_CRUSH: case MOD_TELEFRAG:
	case MOD_FALLING: case MOD_SUICIDE: case MOD_TARGET_LASER: case MOD_TRIGGER_HURT:
		return 1.0f;
	default:
		return g_barrierScale.value;
	}
}

/*
==============================================================================

PLASMA TRAIL

While active, glowing points are left behind about 80 units apart; an
enemy within 32 units of the trail's line, 20 units over the floor, burns
for g_trailDamage every 200 ms. Pressing again detonates them all.

==============================================================================
*/

#define TRAIL_BURN_MSEC		200

static void G_TrailBurnOne( gentity_t *seg, gentity_t *other );

static void G_TrailThink( gentity_t *seg ) {
	int			touch[MAX_GENTITIES];
	vec3_t		mins, maxs;
	gentity_t	*other;
	int			i, num;

	if ( level.time >= seg->abilityTime ) {
		G_FreeEntity( seg );
		return;
	}
	// champion-audit.md: the burn reaches 32 u around the trail's line from
	// this point back to the one dropped before it, not only around points
	{
		gentity_t	*prev = seg->target_ent;
		vec3_t		a, b;

		VectorCopy( seg->r.currentOrigin, a );
		if ( prev && prev->inuse && prev->parent == seg->parent && prev->classname
			&& !strcmp( prev->classname, "plasma_trail" ) && prev->think == G_TrailThink ) {
			VectorCopy( prev->r.currentOrigin, b );
		} else {
			VectorCopy( a, b );
		}
		for ( i = 0 ; i < 3 ; i++ ) {
			mins[i] = ( a[i] < b[i] ? a[i] : b[i] ) + seg->r.mins[i];
			maxs[i] = ( a[i] > b[i] ? a[i] : b[i] ) + seg->r.maxs[i];
		}
		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
		for ( i = 0 ; i < num ; i++ ) {
			vec3_t	ab, ap, c, cmins, cmaxs;
			float	t, len2;
			int		k;

			other = &g_entities[touch[i]];
			if ( !G_AbilityEnemy( seg->parent, other ) || !other->client || level.time < other->client->trailBurnTime ) {
				continue;
			}
			// the closest point of the line to the player, and its box
			VectorSubtract( b, a, ab );
			VectorSubtract( other->r.currentOrigin, a, ap );
			len2 = DotProduct( ab, ab );
			t = len2 > 0 ? DotProduct( ap, ab ) / len2 : 0;
			t = t < 0 ? 0 : t > 1 ? 1 : t;
			VectorMA( a, t, ab, c );
			VectorAdd( c, seg->r.mins, cmins );
			VectorAdd( c, seg->r.maxs, cmaxs );
			for ( k = 0 ; k < 3 ; k++ ) {
				if ( cmins[k] > other->r.absmax[k] || cmaxs[k] < other->r.absmin[k] ) {
					break;
				}
			}
			if ( k < 3 ) {
				continue;
			}
			G_TrailBurnOne( seg, other );
		}
	}
	seg->nextthink = level.time + ABILITY_FRAME;
}

static void G_TrailBurnOne( gentity_t *seg, gentity_t *other ) {
	// once per 200 ms however many points overlap
	other->client->trailBurnTime = level.time + TRAIL_BURN_MSEC;
	G_Slow( other, 0.75f, TRAIL_BURN_MSEC + 50 );		// slowed while in it
	G_Damage( other, seg, seg->parent, NULL, NULL, g_trailDamage.integer, DAMAGE_NO_KNOCKBACK, MOD_PLASMATRAIL );
}

static void G_DropTrail( gentity_t *ent ) {
	vec3_t		point, dir = { 0, 0, 0 };
	gentity_t	*seg;

	VectorCopy( ent->client->ps.origin, point );
	point[2] += MINS_Z + 8;
	seg = G_AbilityMissile( ent, "plasma_trail", point, dir, 0, TR_STATIONARY, WP_PLASMAGUN );
	seg->s.generic1 = AV_TRAIL;
	seg->s.otherEntityNum = ent->s.number;
	seg->clipmask = 0;
	// 32 units around a line 20 over the floor (the point sits 8 over it)
	VectorSet( seg->r.mins, -32, -32, 12 - 32 );
	VectorSet( seg->r.maxs, 32, 32, 12 + 32 );
	seg->abilityTime = level.time + g_trailLife.integer;
	seg->target_ent = ent->client->lastTrailSeg;		// the line back to the last point
	ent->client->lastTrailSeg = seg;
	seg->think = G_TrailThink;
	seg->nextthink = level.time + ABILITY_FRAME;
	trap_LinkEntity( seg );
}

// one segment bursting, its turn in the chain
static void G_TrailBurst( gentity_t *seg ) {
	vec3_t	dir = { 0, 0, 1 };

	if ( seg->parent ) {
		G_SplashParams( 10, 0, 0 );		// down to 10 at the edge, no push
		// the detonation spares Slash (champion-audit.md)
		G_RadiusDamage( seg->r.currentOrigin, seg->parent, g_trailBlast.integer, g_trailRadius.integer, seg->parent, MOD_PLASMATRAIL );
	}
	seg->s.eType = ET_GENERAL;
	G_AddEvent( seg, EV_MISSILE_MISS, DirToByte( dir ) );
	seg->freeAfterEvent = qtrue;
	seg->think = NULL;
	trap_LinkEntity( seg );
}

// the segments burst one by one, oldest first, 30 ms apart
#define TRAIL_CHAIN_MSEC	30

static void G_DetonateTrail( gentity_t *ent ) {
	gentity_t	*seg, *segs[MAX_GENTITIES / 8];
	int			i, j, n = 0;

	for ( i = MAX_CLIENTS, seg = &g_entities[MAX_CLIENTS] ; i < level.num_entities && n < ARRAY_LEN( segs ) ; i++, seg++ ) {
		if ( !seg->inuse || seg->parent != ent || !seg->classname || strcmp( seg->classname, "plasma_trail" ) ) {
			continue;
		}
		// oldest first: the one that expires first
		for ( j = n; j > 0 && segs[j - 1]->abilityTime > seg->abilityTime; j-- ) {
			segs[j] = segs[j - 1];
		}
		segs[j] = seg;
		n++;
	}
	for ( i = 0; i < n; i++ ) {
		segs[i]->think = G_TrailBurst;
		segs[i]->nextthink = level.time + i * TRAIL_CHAIN_MSEC;
		segs[i]->abilityTime = level.time + 60000;		// no longer burning: only the burst
	}
	G_AbilityDone( ent );
}

/*
==============================================================================

DRONE STRIKE

The player flies the drone while the body stays behind, unseen but
vulnerable. The drone has its own health; losing it, or the time running
out, brings the player back. Pressing again launches the drone as a
rocket. Killing the body kills the player.

==============================================================================
*/

static void G_DroneBodyDie( gentity_t *body, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	gentity_t	*owner = body->parent;
	int			health = body->health;		// (G_DroneEnd frees the body)

	body->takedamage = qfalse;
	if ( !owner || !owner->client || owner->client->droneBody != body ) {
		G_FreeEntity( body );
		return;
	}
	G_DroneEnd( owner, qfalse );
	// he dies as the body did: the hit was already counted against the body,
	// so no second, huge damage for the stats
	owner->health = owner->client->ps.stats[STAT_HEALTH] = health < 0 ? health : 0;
	owner->client->ps.stats[STAT_ARMOR] = 0;
	if ( attacker ) {
		owner->client->lasthurt_client = attacker->s.number;
		owner->client->lasthurt_mod = mod;
	}
	owner->enemy = attacker;
	owner->flags |= FL_NO_KNOCKBACK;
	owner->die( owner, inflictor, attacker, damage, mod );
}

// a hit flashes the body for the client (abilities.md: Strogg's body)
static void G_DroneBodyPain( gentity_t *body, gentity_t *attacker, int damage ) {
	body->s.time = level.time;
}

/*
================
G_DroneBodyThink

The body left behind falls under gravity and lands crouched; movers push
and carry it (G_MoverPush knows it by name), and pushed off an edge it
falls again
================
*/
#define DRONE_TAKEOFF		88		// the drone starts this far above the body
#define DRONE_UNDETECTED	1000	// msec not revealed after the drone
#define DRONE_VIEW			115		// degrees the drone sees enemies in
#define DRONE_BODY_SINK		60		// u/s, an idle player sinking in water
#define DRONE_BODY_SINK_ACCEL	400		// u/s/s toward that sink
#define DRONE_BODY_AIR		12000	// msec under water before drowning, as a player

/*
================
G_DroneBodyPush

Knockback, jump pads and teleporters move the body: a new fall from where
it is, with this velocity added
================
*/
void G_DroneBodyPush( gentity_t *body, const vec3_t kvel ) {
	vec3_t	vel;

	if ( body->s.pos.trType == TR_STATIONARY ) {
		VectorClear( vel );
	} else {
		BG_EvaluateTrajectoryDelta( &body->s.pos, level.time, vel );
	}
	VectorAdd( vel, kvel, vel );
	VectorCopy( body->r.currentOrigin, body->s.pos.trBase );
	VectorCopy( vel, body->s.pos.trDelta );
	body->s.pos.trType = TR_GRAVITY;
	body->s.pos.trTime = level.time;
	body->s.groundEntityNum = ENTITYNUM_NONE;
}


/*
================
G_DroneBodyChampion

Whose body it is, for its eye and hit heights (Strogg if the owner is gone)
================
*/
static int G_DroneBodyChampion( gentity_t *body ) {
	if ( body->parent && body->parent->client ) {
		return body->parent->client->ps.stats[STAT_CHAMPION];
	}
	return BG_ChampionForName( "strogg" );
}

/*
================
G_DroneBodyWorld

What the world does to the body each frame, as to an idle player: jump
pads, teleporters and hurt triggers it touches; lava, slime and drowning
================
*/
static void G_DroneBodyWorld( gentity_t *body ) {
	int			touch[MAX_GENTITIES], num, i, contents;
	vec3_t		mins, maxs, head;
	gentity_t	*hit;
	trace_t		tr;

	VectorAdd( body->r.currentOrigin, body->r.mins, mins );
	VectorAdd( body->r.currentOrigin, body->r.maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	memset( &tr, 0, sizeof( tr ) );
	for ( i = 0 ; i < num ; i++ ) {
		hit = &g_entities[touch[i]];
		if ( !hit->classname || !( hit->r.contents & CONTENTS_TRIGGER ) || !trap_EntityContact( mins, maxs, hit ) ) {
			continue;
		}
		if ( !strcmp( hit->classname, "trigger_push" ) ) {
			// launched as a player would be (origin2 holds the pad's velocity)
			VectorCopy( body->r.currentOrigin, body->s.pos.trBase );
			VectorCopy( hit->s.origin2, body->s.pos.trDelta );
			body->s.pos.trType = TR_GRAVITY;
			body->s.pos.trTime = level.time;
			body->s.groundEntityNum = ENTITYNUM_NONE;
		} else if ( !strcmp( hit->classname, "trigger_teleport" ) && !( hit->spawnflags & 1 ) ) {
			gentity_t	*dest = G_PickTarget( hit->target );

			if ( dest ) {
				vec3_t	origin, forward;

				// out of the exit at 400 u/s, as a player, facing its way
				VectorCopy( dest->s.origin, origin );
				origin[2] += 1;
				G_SetOrigin( body, origin );
				AngleVectors( dest->s.angles, forward, NULL, NULL );
				body->s.pos.trType = TR_GRAVITY;
				body->s.pos.trTime = level.time;
				VectorScale( forward, 400, body->s.pos.trDelta );
				body->s.groundEntityNum = ENTITYNUM_NONE;
				body->s.apos.trBase[YAW] = dest->s.angles[YAW];
				body->s.eFlags ^= EF_TELEPORT_BIT;
				if ( body->parent && body->parent->client ) {
					body->parent->client->droneBodyAngles[YAW] = dest->s.angles[YAW];
				}
				return;
			}
		} else if ( !strcmp( hit->classname, "trigger_hurt" ) && hit->touch ) {
			hit->touch( hit, body, &tr );
			if ( !body->inuse || body->health <= 0 ) {
				return;
			}
		}
	}

	// how deep it is in liquid, as a player: feet, waist, head (1-3)
	{
		vec3_t	p;
		int		depth = 0;

		VectorCopy( body->r.currentOrigin, p );
		p[2] += body->r.mins[2] + 1;
		contents = trap_PointContents( p, -1 );
		if ( contents & MASK_WATER ) {
			depth = 1;
			p[2] = body->r.currentOrigin[2] + ( body->r.mins[2] + BG_ViewHeight( G_DroneBodyChampion( body ), qtrue ) ) / 2;
			if ( trap_PointContents( p, -1 ) & MASK_WATER ) {
				depth = 2;
				p[2] = body->r.currentOrigin[2] + BG_ViewHeight( G_DroneBodyChampion( body ), qtrue );
				if ( trap_PointContents( p, -1 ) & MASK_WATER ) {
					depth = 3;
				}
			}
		}
		// a void that removes items: as a fall out of the map
		if ( trap_PointContents( body->r.currentOrigin, -1 ) & CONTENTS_NODROP ) {
			G_Damage( body, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT );
			return;
		}
		// lava and slime once a second, by depth (30 and 10 a level)
		if ( depth && ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) && level.time >= body->pain_debounce_time ) {
			body->pain_debounce_time = level.time + 1000;
			G_Damage( body, NULL, NULL, NULL, NULL, ( contents & CONTENTS_LAVA ) ? 30 * depth : 10 * depth, 0,
				( contents & CONTENTS_LAVA ) ? MOD_LAVA : MOD_SLIME );
			if ( !body->inuse || body->health <= 0 ) {
				return;
			}
		}
		// in water past the waist it sinks slowly, as an idle player (the
		// think caps the sink at 60 u/s); out of it, gravity again
		if ( depth >= 2 && body->s.pos.trType == TR_GRAVITY ) {
			body->s.pos.trType = TR_LINEAR;
		} else if ( depth < 2 && body->s.pos.trType == TR_LINEAR ) {
			body->s.pos.trType = TR_GRAVITY;
		}
	}
	VectorCopy( body->r.currentOrigin, head );
	head[2] += BG_ViewHeight( G_DroneBodyChampion( body ), qtrue );
	if ( trap_PointContents( head, -1 ) & MASK_WATER ) {
		if ( level.time > body->wait && level.time >= body->pain_debounce_time ) {
			body->count = body->count < 15 ? body->count + 2 : 15;
			body->pain_debounce_time = level.time + 1000;
			G_Damage( body, NULL, NULL, NULL, NULL, body->count, DAMAGE_NO_ARMOR, MOD_WATER );
		}
	} else {
		body->wait = level.time + DRONE_BODY_AIR;
		body->count = 0;
	}
}

static void G_DroneBodyThink( gentity_t *body ) {
	vec3_t	origin, down;
	trace_t	tr;

	body->nextthink = level.time + 1;		// every frame
	G_DroneBodyWorld( body );
	if ( !body->inuse || body->health <= 0 || body->think != G_DroneBodyThink ) {
		return;
	}
	if ( body->s.pos.trType == TR_STATIONARY ) {
		// still standing on something?
		VectorCopy( body->r.currentOrigin, down );
		down[2] -= 1;
		trap_Trace( &tr, body->r.currentOrigin, body->r.mins, body->r.maxs, down, body->s.number, body->clipmask );
		if ( tr.fraction < 1 && !tr.startsolid ) {
			return;
		}
		body->s.groundEntityNum = ENTITYNUM_NONE;
		body->s.pos.trType = TR_GRAVITY;
		body->s.pos.trTime = level.time;
		VectorCopy( body->r.currentOrigin, body->s.pos.trBase );
		VectorClear( body->s.pos.trDelta );
	}
	{
		vec3_t	vel;
		float	dt = ( level.time - body->s.pos.trTime ) * 0.001f;

		// in water past the waist: no gravity pull past a slow sink, and
		// the water slows the rest (pushes and knockback still count)
		if ( body->s.pos.trType == TR_LINEAR ) {
			BG_EvaluateTrajectoryDelta( &body->s.pos, level.time, vel );
			vel[2] -= DRONE_BODY_SINK_ACCEL * dt;
			if ( vel[2] < -DRONE_BODY_SINK ) {
				vel[2] = -DRONE_BODY_SINK;
			}
			vel[0] -= vel[0] * 2 * dt;
			vel[1] -= vel[1] * 2 * dt;
			VectorMA( body->r.currentOrigin, dt, vel, origin );
		} else {
			BG_EvaluateTrajectory( &body->s.pos, level.time, origin );
			BG_EvaluateTrajectoryDelta( &body->s.pos, level.time, vel );
		}
		trap_Trace( &tr, body->r.currentOrigin, body->r.mins, body->r.maxs, origin, body->s.number, body->clipmask );
		if ( tr.startsolid ) {
			tr.fraction = 0;
		}
		if ( tr.allsolid ) {
			VectorClear( vel );		// stuck in something: no speed builds up
		}
		VectorCopy( tr.endpos, body->r.currentOrigin );
		if ( tr.fraction < 1 && tr.plane.normal[2] > 0.7f ) {
			// landed
			G_SetOrigin( body, tr.endpos );
			body->s.groundEntityNum = tr.entityNum;
			trap_LinkEntity( body );
			return;
		}
		if ( tr.fraction < 1 ) {
			// a wall, a steep slope or a ceiling: slide along it, as player
			// movement clips its velocity
			float	backoff = DotProduct( vel, tr.plane.normal );

			backoff = backoff < 0 ? backoff * 1.001f : backoff / 1.001f;
			VectorMA( vel, -backoff, tr.plane.normal, vel );
		}
		// rebased every frame, so movers see where it really is
		VectorCopy( body->r.currentOrigin, body->s.pos.trBase );
		VectorCopy( vel, body->s.pos.trDelta );
		body->s.pos.trTime = level.time;
	}
	trap_LinkEntity( body );
}

static void G_StartDrone( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	gentity_t	*body;
	vec3_t		origin;

	body = G_Spawn();
	body->classname = "drone_body";
	body->s.eType = ET_GENERAL;
	// drawn by the client as Strogg crouched, faintly (EF_DRONE marks it,
	// clientNum says whose), facing the way he launched
	body->s.eFlags = EF_DRONE;
	body->s.clientNum = ent->s.number;
	body->s.apos.trBase[YAW] = client->ps.viewangles[YAW];
	body->parent = ent;
	body->r.ownerNum = ent->s.number;
	body->r.contents = CONTENTS_CORPSE;
	VectorCopy( ent->r.mins, body->r.mins );
	VectorCopy( ent->r.maxs, body->r.maxs );
	body->r.maxs[2] = CROUCH_HEIGHT;		// the crouched box (shots: G_PlaceShotVolumes)
	body->takedamage = qtrue;
	body->health = ent->health;
	body->die = G_DroneBodyDie;
	body->pain = G_DroneBodyPain;
	body->clipmask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	// the body stays where Strogg was, moving as he was (abilities.md: it
	// keeps falling, rides movers, takes jump pads and teleporters)
	VectorCopy( client->oldOrigin, origin );
	G_SetOrigin( body, origin );
	body->s.groundEntityNum = ENTITYNUM_NONE;
	body->s.pos.trType = TR_GRAVITY;
	body->s.pos.trTime = level.time;
	VectorCopy( client->oldVelocity, body->s.pos.trDelta );
	body->think = G_DroneBodyThink;
	body->nextthink = level.time + 1;
	body->wait = level.time + DRONE_BODY_AIR;		// air left (drowning)
	trap_LinkEntity( body );

	// the drone takes off about 88 u above him (less under a ceiling)
	{
		trace_t	tr;
		vec3_t	up;

		VectorCopy( origin, up );
		up[2] += DRONE_TAKEOFF;
		trap_Trace( &tr, origin, ent->r.mins, ent->r.maxs, up, ent->s.number, MASK_PLAYERSOLID );
		VectorCopy( tr.endpos, client->ps.origin );
		VectorClear( client->ps.velocity );
		client->ps.eFlags ^= EF_TELEPORT_BIT;
	}

	client->droneBody = body;
	client->droneStartTime = level.time;
	client->droneMGAmmo = client->ps.ammo[WP_MACHINEGUN];
	// the drone's gun, never the held weapon (that fired rockets for free)
	client->droneWeapon = client->ps.weapon;
	client->ps.weapon = WP_MACHINEGUN;
	client->ps.weaponstate = WEAPON_READY;
	// a slow weapon's refire (a rail's 1.5 s) doesn't carry over to it
	if ( client->ps.weaponTime > BG_AbilityParam( ACTIVE_DRONE, AP_LOCKOUT ) ) {
		client->ps.weaponTime = BG_AbilityParam( ACTIVE_DRONE, AP_LOCKOUT );
	}
	client->droneArmor = client->ps.stats[STAT_ARMOR];
	VectorCopy( client->ps.viewangles, client->droneBodyAngles );

	// the drone has its own health
	ent->health = client->ps.stats[STAT_HEALTH] = g_droneHealth.integer;
	client->ps.stats[STAT_ARMOR] = 0;
}

/*
================
G_DroneEnd

Back to the body; launch fires the drone off as a rocket first
================
*/
void G_DroneEnd( gentity_t *ent, qboolean launch ) {
	gclient_t	*client = ent->client;
	gentity_t	*body = client->droneBody;
	gentity_t	*tent;

	if ( !body ) {
		return;
	}
	client->droneCharging = qfalse;
	client->ps.pm_flags &= ~PMF_DRONE_CHARGE;		// (else the body would grapple-move)
	if ( launch ) {
		// champion-audit.md: the charging drone blows up where it is (75
		// within 120 u, never hurting Strogg or his body, credited to him)
		tent = G_TempEntity( client->ps.origin, EV_MISSILE_MISS );
		tent->s.weapon = WP_ROCKET_LAUNCHER;
		tent->s.eventParm = DirToByte( vec3_origin );
		body->takedamage = qfalse;		// the blast spares his own body
		G_RadiusDamage( client->ps.origin, ent, g_droneRocketDamage.integer, g_droneRocketRadius.integer, ent, MOD_DRONE );
	}
	// no machinegun bullet left from the drone's gun
	client->ps.ammo[WP_MACHINEGUN] = client->droneMGAmmo;
	// and the weapon he held, raised again
	if ( client->droneWeapon > WP_NONE && client->droneWeapon < WP_NUM_WEAPONS
		&& ( client->ps.stats[STAT_WEAPONS] & ( 1 << client->droneWeapon ) ) ) {
		client->ps.weapon = client->droneWeapon;
	} else {
		client->ps.weapon = WP_GAUNTLET;
	}
	client->ps.weaponstate = WEAPON_RAISING;
	// detection effects pass him by for a second after he is back
	client->undetectedUntil = level.time + DRONE_UNDETECTED;

	client->droneBody = NULL;
	client->droneEndTime = level.time;		// input waits a moment (g_active)
	// however the flight ends, the weapons stay locked a moment longer
	if ( client->ps.weaponTime < BG_AbilityParam( ACTIVE_DRONE, AP_LOCKOUT ) ) {
		client->ps.weaponTime = BG_AbilityParam( ACTIVE_DRONE, AP_LOCKOUT );
	}
	VectorCopy( body->r.currentOrigin, client->ps.origin );
	// he goes on as the body was moving, crouched if there's no room to stand
	if ( body->s.pos.trType == TR_STATIONARY ) {
		VectorClear( client->ps.velocity );
	} else {
		BG_EvaluateTrajectoryDelta( &body->s.pos, level.time, client->ps.velocity );
	}
	{
		trace_t	tr;

		vec3_t	mins, maxs;

		// the champion's standing box (ent still has the drone's)
		VectorCopy( body->r.mins, mins );
		VectorCopy( body->r.maxs, maxs );
		maxs[2] = DEFAULT_HEIGHT;
		trap_Trace( &tr, client->ps.origin, mins, maxs, client->ps.origin, ent->s.number, MASK_PLAYERSOLID );
		if ( tr.startsolid || tr.allsolid ) {
			client->ps.pm_flags |= PMF_DUCKED;
		}
	}
	client->ps.eFlags ^= EF_TELEPORT_BIT;
	client->ps.eFlags &= ~EF_DRONE;
	if ( body->health > 0 ) {
		ent->health = client->ps.stats[STAT_HEALTH] = body->health;
	}
	client->ps.stats[STAT_ARMOR] = client->droneArmor;
	SetClientViewAngle( ent, client->droneBodyAngles );
	G_FreeEntity( body );

	BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );
	VectorCopy( client->ps.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );
	G_AbilityDone( ent );
}

/*
================
G_DroneAbsorbDamage

Damage to a flying drone that would bring it down ends the flight instead
of killing the player. Returns qtrue if the damage was handled.
================
*/
qboolean G_DroneAbsorbDamage( gentity_t *ent, int take ) {
	if ( !ent->client || !ent->client->droneBody ) {
		return qfalse;
	}
	if ( ent->health - take > 0 ) {
		return qfalse;
	}
	// shot down: a small burst and its end sound where it was, no damage
	{
		gentity_t	*tent = G_TempEntity( ent->client->ps.origin, EV_MISSILE_MISS );

		tent->s.weapon = WP_PLASMAGUN;
		tent->s.eventParm = DirToByte( vec3_origin );
		tent = G_TempEntity( ent->client->ps.origin, EV_GENERAL_SOUND );
		tent->s.eventParm = G_SoundIndex( "sound/abilities/drone_end.wav" );
	}
	G_DroneEnd( ent, qfalse );
	return qtrue;
}

/*
==============================================================================

STROYENT CELLS

A kill by a champion with this passive drops a cell; only such champions
can pick it up, for health that may go over the maximum.

==============================================================================
*/

static void G_CellThink( gentity_t *cell ) {
	gentity_t	*other;
	int			i, max;

	if ( level.time >= cell->abilityTime ) {
		G_FreeEntity( cell );
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !other->inuse || !other->client || other->health <= 0 ) {
			continue;
		}
		if ( !( BG_Champion( other->client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_STROYENT ) ) {
			continue;
		}
		if ( Distance( other->r.currentOrigin, cell->r.currentOrigin ) > 40 ) {
			continue;
		}
		max = other->client->ps.stats[STAT_MAX_HEALTH] + G_ClassInfo( other->client )->maxHealthBonus;
		other->health += g_stroyentHeal.integer;
		if ( other->health > max ) {
			other->health = max;
		}
		other->client->ps.stats[STAT_HEALTH] = other->health;
		// the small health's pickup: its sound and a pickup feed line
		{
			gitem_t	*item = BG_FindItem( "5 Health" );

			G_AddEvent( other, EV_ITEM_PICKUP, item ? item - bg_itemlist : 0 );
		}
		G_FreeEntity( cell );
		return;
	}
	cell->nextthink = level.time + ABILITY_FRAME * 2;
}

void G_DropStroyentCell( gentity_t *victim, gentity_t *attacker ) {
	gentity_t	*cell;

	if ( !attacker || !attacker->client || attacker == victim ) {
		return;
	}
	if ( !( BG_Champion( attacker->client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_STROYENT ) ) {
		return;
	}
	cell = G_Spawn();
	cell->classname = "stroyent_cell";
	cell->s.eType = ET_GENERAL;
	cell->s.modelindex = G_ModelIndex( "models/powerups/health/small_cross.md3" );
	cell->abilityTime = level.time + g_stroyentLife.integer;
	cell->think = G_CellThink;
	cell->nextthink = level.time + ABILITY_FRAME;
	// on the floor under the victim (champion-audit.md)
	{
		trace_t	tr;
		vec3_t	down;

		VectorCopy( victim->r.currentOrigin, down );
		down[2] -= 4096;
		trap_Trace( &tr, victim->r.currentOrigin, NULL, NULL, down, victim->s.number, MASK_SOLID );
		G_SetOrigin( cell, tr.fraction < 1 ? tr.endpos : victim->r.currentOrigin );
	}
	trap_LinkEntity( cell );
}

/*
==============================================================================

STATE CHANGES

==============================================================================
*/

/*
================
G_AbilityStarted / G_AbilityEnded

Called by the upkeep when a timed ability starts or stops running
================
*/
void G_AbilityStarted( gentity_t *ent ) {
	ent->client->trailTime = 0;
	ent->client->lastTrailSeg = NULL;		// a new lay links only its own points
	if ( BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active == ACTIVE_MASTEROFDEATH ) {
		G_MasterOfDeathStart( ent );
	}
}

void G_AbilityEnded( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	client->barrierMode = 0;
	client->ps.eFlags &= ~EF_BARRIER;
	if ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->active == ACTIVE_MASTEROFDEATH ) {
		G_MasterOfDeathEnd( ent );
	}
	if ( client->droneBody ) {
		// a charging drone's 45 s running out end in its blast; a death, a
		// team change or a Shell Shock end it quietly
		G_DroneEnd( ent, client->droneCharging && ent->health > 0
			&& client->ps.powerups[PW_SHELLSHOCK] <= level.time && client->sess.sessionTeam != TEAM_SPECTATOR );
	}
}

/*
==============================================================================

ENTRY POINTS

==============================================================================
*/

/*
================
G_AbilityFire

EV_USE_ABILITY for the abilities that put something in the world
================
*/
void G_AbilityFire( gentity_t *ent ) {
	switch ( BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active ) {
	case ACTIVE_ORB:			G_FireOrb( ent ); break;
	case ACTIVE_GRENADES:		G_ThrowGrenade( ent ); break;
	case ACTIVE_FLAMESTRIKE:	G_FireFlameStrike( ent ); break;
	case ACTIVE_TOTEM:			G_ThrowTotem( ent ); break;
	case ACTIVE_TURRET:			G_ThrowTurret( ent ); break;
	case ACTIVE_ACIDSPIT:
		ent->client->abilityShots = 0;
		ent->client->abilityShotTime = level.time;
		break;
	case ACTIVE_BARRIER:		G_StartBarrier( ent ); break;
	case ACTIVE_DRONE:			G_StartDrone( ent ); break;
	case ACTIVE_SHELLSHOCK:		G_ShellShock( ent ); break;
	case ACTIVE_SPINEVOLLEY:	G_SpineVolley( ent ); break;
	case ACTIVE_SUPPLYDROP:		G_SupplyDrop( ent ); break;
	case ACTIVE_MARKPREY:		G_MarkPrey( ent ); break;
	case ACTIVE_DOWNPOUR:		G_Downpour( ent ); break;
	case ACTIVE_HOUND:			G_ReleaseHound( ent ); break;
	case ACTIVE_RIFTANCHOR:		G_DropAnchor( ent ); break;
	default:
		break;
	}
}

/*
================
G_AbilityRepress

EV_ABILITY_REPRESS: the ability was pressed again while running
================
*/
void G_AbilityRepress( gentity_t *ent ) {
	switch ( BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active ) {
	case ACTIVE_ORB:			G_OrbTeleport( ent ); break;
	case ACTIVE_FLAMESTRIKE:	G_FlameConverge( ent ); break;
	case ACTIVE_BARRIER:		G_BarrierToLaser( ent ); break;
	case ACTIVE_PLASMATRAIL:	G_DetonateTrail( ent ); break;
	case ACTIVE_RIFTANCHOR:		G_RecallAnchor( ent ); break;
	case ACTIVE_DRONE:
		// (pmove holds the 0.5 s gate and starts the charge) the first
		// second press: the charge and its sound; a further one: the blast
		if ( !ent->client->droneBody ) {
			break;
		}
		if ( !ent->client->droneCharging ) {
			gentity_t	*tent = G_TempEntity( ent->client->ps.origin, EV_GENERAL_SOUND );

			ent->client->droneCharging = qtrue;
			// (its own entity: the player's two event slots also carry the
			// predicted second press)
			tent->s.eventParm = G_SoundIndex( "sound/abilities/drone_charge.wav" );
		} else {
			G_DroneEnd( ent, qtrue );
		}
		break;
	default:
		break;
	}
}

/*
================
G_AbilityClientThink

Per-frame work for a client: acid blobs in a row, burning and poison
================
*/
void G_AbilityClientThink( gentity_t *ent, int msec ) {
	gclient_t	*client = ent->client;

	// Galena: how many of her totems stand, for the HUD's marks
	if ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->active == ACTIVE_TOTEM ) {
		gentity_t	*other;
		int			i, count = 0;

		for ( i = MAX_CLIENTS, other = &g_entities[MAX_CLIENTS] ; i < level.num_entities ; i++, other++ ) {
			if ( other->inuse && other->parent == ent && other->s.eType == ET_GENERAL && other->classname
				&& !strcmp( other->classname, "totem" ) ) {
				count++;
			}
		}
		client->ps.stats[STAT_TOTEMS] = count;
	}

	if ( G_AbilityActive( ent, ACTIVE_ACIDSPIT ) ) {
		while ( client->abilityShots < g_acidBlobs.integer && level.time >= client->abilityShotTime ) {
			G_SpitAcid( ent );
			client->abilityShots++;
			client->abilityShotTime += g_acidInterval.integer;
		}
	}

	if ( G_AbilityActive( ent, ACTIVE_BARRIER ) && client->barrierMode == 2 ) {
		while ( level.time >= client->laserTime && client->laserTime < client->laserEnd ) {
			G_FireLaser( ent );
			client->laserTime += g_laserInterval.integer;
		}
		if ( client->laserTime >= client->laserEnd ) {
			BG_SetAbility( &client->ps, ABILITY_IDLE, BG_AbilityCharges( &client->ps ) );
			client->ps.stats[STAT_ABILITY_TIME] = 0;
			G_AbilityEnded( ent );
		}
	}
	// the charging drone blows up on touching a wall or an enemy
	if ( client->droneCharging && client->droneBody && G_AbilityActive( ent, ACTIVE_DRONE ) ) {
		vec3_t	dir, end;
		trace_t	hit;

		// ahead along the view (it flies where the view points): this
		// command's flight and 6 u more, so a drone pressed against a wall (its
		// speed clipped away) or grazing one still finds it; the blast where
		// the trace stopped. Walls (the world and movers) and enemies only.
		AngleVectors( client->ps.viewangles, dir, NULL, NULL );
		{
			gentity_t	*other;

			VectorMA( client->ps.origin, VectorLength( client->ps.velocity ) * msec * 0.001f + 6, dir, end );
			trap_Trace( &hit, client->ps.origin, ent->r.mins, ent->r.maxs, end, ent->s.number, MASK_PLAYERSOLID );
			other = &g_entities[hit.entityNum];
			if ( hit.fraction < 1 && ( hit.entityNum == ENTITYNUM_WORLD || other->s.eType == ET_MOVER
				|| ( hit.entityNum < MAX_CLIENTS && G_AbilityEnemy( ent, other ) ) ) ) {
				VectorCopy( hit.endpos, client->ps.origin );
				G_DroneEnd( ent, qtrue );
				return;
			}
		}
	}
	// Strogg's drone shows the enemies it sees
	if ( G_AbilityActive( ent, ACTIVE_DRONE ) ) {
		gentity_t	*other;
		trace_t		tr;
		int			i;

		for ( i = 0, other = g_entities ; i < level.maxclients ; i++, other++ ) {
			if ( !other->inuse || !other->client || other == ent || other->health <= 0 || !G_AbilityEnemy( ent, other )
				|| Distance( other->r.currentOrigin, client->ps.origin ) > 2000 ) {
				continue;
			}
			// inside the drone's 115 degree view
			{
				vec3_t	fwd, to;

				AngleVectors( client->ps.viewangles, fwd, NULL, NULL );
				VectorSubtract( other->r.currentOrigin, client->ps.origin, to );
				VectorNormalize( to );
				if ( DotProduct( fwd, to ) < cos( DEG2RAD( DRONE_VIEW * 0.5f ) ) ) {
					continue;
				}
			}
			trap_Trace( &tr, client->ps.origin, NULL, NULL, other->r.currentOrigin, ent->s.number, MASK_SOLID );
			if ( tr.fraction == 1.0f && other->client->undetectedUntil <= level.time ) {
				other->client->revealEndTime = level.time + 300;
				other->client->revealTeam = g_gametype.integer >= GT_TEAM ? client->sess.sessionTeam : 1000 + ent->s.number;
			}
		}
	}

	if ( G_AbilityActive( ent, ACTIVE_PLASMATRAIL ) ) {
		// a charge every g_trailSpacing units travelled, the first at once
		if ( !client->trailTime || Distance( client->ps.origin, client->trailLast ) >= g_trailSpacing.value ) {
			G_DropTrail( ent );
			VectorCopy( client->ps.origin, client->trailLast );
			client->trailTime = level.time;
		}
	}

	if ( level.timeoutEnd ) {
		return;
	}
	G_ChampionClientThink( ent );
	G_XaeroThink( ent );

	G_DamageOverTimeTick( ent, &client->burnEndTime, &client->burnNextTime, client->burnAttacker,
		g_burnDamage.integer, g_burnTime.integer, MOD_BURN );
	G_DamageOverTimeTick( ent, &client->poisonEndTime, &client->poisonNextTime, client->poisonAttacker,
		g_poisonDamage.integer, g_poisonTime.integer, MOD_ACID );
}

/*
================
G_AbilityClientDied

Remove what belongs to a dying or leaving client
================
*/
void G_AbilityClientDied( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	int			i;

	G_ClearDamageOverTime( ent );
	G_ChampionClientDied( ent );
	G_AbilityEnded( ent );
	if ( client->abilityEnt ) {
		G_FreeEntity( client->abilityEnt );
		client->abilityEnt = NULL;
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		client->fireballs[i] = NULL;
	}
}
