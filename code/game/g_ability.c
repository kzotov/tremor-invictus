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
#define	ABILITY_PRESTEP		50		// move a bit on the very first frame, as missiles do

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
static void G_AbilityEyes( gentity_t *ent, vec3_t start, vec3_t forward, vec3_t right ) {
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
static void G_AbilityDone( gentity_t *ent ) {
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
static qboolean G_AbilityEnemy( gentity_t *owner, gentity_t *other ) {
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
static qboolean G_AbilityMarked( gentity_t *ent, int clientNum ) {
	return ( ent->abilityMask[clientNum >> 5] & ( 1 << ( clientNum & 31 ) ) ) != 0;
}

static void G_AbilityMark( gentity_t *ent, int clientNum ) {
	ent->abilityMask[clientNum >> 5] |= 1 << ( clientNum & 31 );
}

/*
================
G_AbilityMissile

A thrown or fired ability entity using the missile code for movement
================
*/
static gentity_t *G_AbilityMissile( gentity_t *owner, const char *classname, vec3_t start, vec3_t dir,
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
static void G_AbilityStop( gentity_t *ent, vec3_t point ) {
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

	if ( *endTime <= level.time || total <= 0 ) {
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

static void G_OrbExplode( gentity_t *orb ) {
	gentity_t	*owner = orb->parent;

	orb->splashDamage = g_orbSplashDamage.integer;
	orb->splashRadius = g_orbSplashRadius.integer;
	orb->splashMethodOfDeath = MOD_ORB;
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
	orb->clipmask = MASK_SOLID;			// flies through players
	orb->abilityImpact = G_OrbImpact;
	orb->think = G_OrbThink;
	orb->nextthink = level.time + ABILITY_FRAME;
	orb->abilityTime = level.time + g_orbLife.integer;
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

	tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
	tent->s.clientNum = ent->s.clientNum;
	tent = G_TempEntity( spot, EV_PLAYER_TELEPORT_IN );
	tent->s.clientNum = ent->s.clientNum;

	trap_UnlinkEntity( ent );
	VectorCopy( spot, ent->client->ps.origin );
	VectorClear( ent->client->ps.velocity );
	ent->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// telefrag whoever is standing there
	VectorAdd( spot, ent->r.mins, mins );
	VectorAdd( spot, ent->r.maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( G_AbilityEnemy( ent, other ) ) {
			G_Damage( other, ent, ent, NULL, NULL, g_orbTelefragDamage.integer, DAMAGE_NO_PROTECTION, MOD_TELEFRAG );
		}
	}

	BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
	trap_LinkEntity( ent );

	ent->client->abilityEnt = NULL;
	G_FreeEntity( orb );
	G_AbilityDone( ent );
}

/*
==============================================================================

GRENADE SWARM

==============================================================================
*/

static void G_ThrowGrenade( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*gren;

	G_AbilityEyes( ent, start, forward, right );
	gren = fire_grenade( ent, start, forward );
	VectorScale( forward, g_swarmSpeed.value, gren->s.pos.trDelta );
	SnapVector( gren->s.pos.trDelta );
	gren->nextthink = level.time + g_swarmFuse.integer;
	gren->damage = g_swarmDamage.integer;
	gren->splashDamage = g_swarmDamage.integer;
	gren->splashRadius = g_swarmRadius.integer;
}

/*
==============================================================================

FLAME STRIKE

Three fireballs in a fan. Each burns what it hits. Pressing again before
they land pulls them together into one bigger explosion.

==============================================================================
*/

static void G_FireballBurst( gentity_t *ball, vec3_t origin, int damage, int radius ) {
	int			touch[MAX_GENTITIES];
	vec3_t		mins, maxs;
	gentity_t	*other;
	int			i, num;

	G_RadiusDamage( origin, ball->parent, damage, radius, NULL, MOD_FIREBALL );

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
		G_Damage( other, ball, ball->parent, NULL, trace->endpos, g_fireballDamage.integer, 0, MOD_FIREBALL );
		if ( other->client ) {
			G_Ignite( other, ball->parent );
		}
	}
	G_FireballBurst( ball, trace->endpos, g_fireballSplash.integer, g_fireballRadius.integer );
	G_FireballFinish( ball, trace->endpos );
}

static void G_FireballExpire( gentity_t *ball ) {
	vec3_t	origin;

	BG_EvaluateTrajectory( &ball->s.pos, level.time, origin );
	G_FireballBurst( ball, origin, g_fireballSplash.integer, g_fireballRadius.integer );
	G_FireballFinish( ball, origin );
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
		ball->abilityImpact = G_FireballImpact;
		ball->think = G_FireballExpire;
		ball->nextthink = level.time + 10000;
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
	G_FireballBurst( ball, point, g_flameConvergeDamage.integer, g_flameConvergeRadius.integer );
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

static void G_AcidImpact( gentity_t *blob, trace_t *trace ) {
	gentity_t	*other = &g_entities[trace->entityNum];
	vec3_t		point, velocity;
	float		size = g_acidPoolSize.value;

	if ( other->takedamage ) {
		BG_EvaluateTrajectoryDelta( &blob->s.pos, level.time, velocity );
		G_Damage( other, blob, blob->parent, velocity, trace->endpos, g_acidDamage.integer, 0, MOD_ACID );
		if ( other->client ) {
			G_Poison( other, blob->parent );
		}
	}

	// leave a pool where it landed
	VectorMA( trace->endpos, 1, trace->plane.normal, point );
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
	blob = G_AbilityMissile( ent, "acid", start, forward, g_acidSpeed.value, TR_GRAVITY, WP_PLASMAGUN );
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
			if ( dist <= g_totemHurtRadius.value ) {
				G_Damage( other, totem, totem->parent, NULL, NULL,
					enforced ? g_totemEnforcedDamage.integer : g_totemDamage.integer, 0, MOD_TOTEM );
				G_FreeEntity( totem );
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
	totem->s.modelindex = G_ModelIndex( "models/powerups/holdable/medkit.md3" );
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

static void G_TurretDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	gentity_t	*tent;

	tent = G_TempEntity( self->r.currentOrigin, EV_MISSILE_MISS );
	tent->s.weapon = WP_ROCKET_LAUNCHER;
	if ( self->parent && self->parent->client && self->parent->client->turret == self ) {
		self->parent->client->turret = NULL;
	}
	G_FreeEntity( self );
}

static void G_TurretThink( gentity_t *turret ) {
	gentity_t	*other, *best = NULL;
	trace_t		tr;
	vec3_t		muzzle, target, dir, angles;
	float		dist, bestDist = g_turretRange.value;
	int			i;

	turret->nextthink = level.time + ABILITY_FRAME;
	if ( !turret->parent || !turret->parent->client || !turret->parent->inuse ) {
		G_TurretDie( turret, NULL, NULL, 0, 0 );
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
		trap_Trace( &tr, muzzle, NULL, NULL, target, turret->s.number, MASK_SHOT );
		if ( tr.entityNum != other->s.number ) {
			continue;
		}
		best = other;
		bestDist = dist;
	}
	if ( !best ) {
		return;
	}

	VectorCopy( best->r.currentOrigin, target );
	target[2] += 16;
	VectorSubtract( target, muzzle, dir );
	VectorNormalize( dir );
	vectoangles( dir, angles );
	G_AbilitySetAngles( turret, angles );

	if ( level.time >= turret->abilityTime ) {
		gentity_t	*bolt;

		turret->abilityTime = level.time + g_turretInterval.integer;
		VectorMA( muzzle, 20, dir, muzzle );	// start outside the turret's own box
		bolt = fire_plasma( turret->parent, muzzle, dir );
		bolt->damage = g_turretDamage.integer;
		bolt->splashDamage = 0;
		bolt->methodOfDeath = MOD_TURRET;
		bolt->splashMethodOfDeath = MOD_TURRET;
		VectorScale( dir, g_turretBoltSpeed.value, bolt->s.pos.trDelta );
		SnapVector( bolt->s.pos.trDelta );
	}
}

/*
================
G_PlaceTurret

Puts a turret on the ground in front of the player. It may not stand on
an item; then the charge comes back.
================
*/
static void G_PlaceTurret( gentity_t *ent ) {
	vec3_t		start, forward, right, end, mins, maxs;
	trace_t		tr;
	gentity_t	*turret;
	int			touch[MAX_GENTITIES];
	int			i, num;

	VectorSet( mins, -12, -12, 0 );
	VectorSet( maxs, 12, 12, 24 );

	G_AbilityEyes( ent, start, forward, right );
	forward[2] = 0;
	VectorNormalize( forward );
	VectorCopy( ent->client->ps.origin, start );
	VectorMA( start, 48, forward, end );
	trap_Trace( &tr, start, mins, maxs, end, ent->s.number, MASK_PLAYERSOLID );
	VectorCopy( tr.endpos, start );
	VectorCopy( start, end );
	end[2] -= 256;
	trap_Trace( &tr, start, mins, maxs, end, ent->s.number, MASK_PLAYERSOLID );
	if ( tr.allsolid || tr.startsolid || tr.fraction == 1.0f ) {
		goto refund;
	}

	VectorAdd( tr.endpos, mins, mins );
	VectorAdd( tr.endpos, maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		if ( g_entities[touch[i]].s.eType == ET_ITEM ) {
			goto refund;
		}
	}

	if ( ent->client->turret ) {
		G_TurretDie( ent->client->turret, NULL, NULL, 0, 0 );
	}
	turret = G_Spawn();
	turret->classname = "sentry_turret";
	turret->s.eType = ET_GENERAL;
	turret->s.modelindex = G_ModelIndex( "models/weapons2/machinegun/machinegun.md3" );
	turret->parent = ent;
	turret->r.ownerNum = ent->s.number;
	turret->r.contents = CONTENTS_CORPSE;
	VectorSet( turret->r.mins, -12, -12, 0 );
	VectorSet( turret->r.maxs, 12, 12, 24 );
	turret->takedamage = qtrue;
	turret->health = g_turretHealth.integer;
	turret->die = G_TurretDie;
	turret->think = G_TurretThink;
	turret->nextthink = level.time + ABILITY_FRAME;
	turret->abilityTime = level.time + 500;
	G_SetOrigin( turret, tr.endpos );
	G_AbilitySetAngles( turret, ent->client->ps.viewangles );
	trap_LinkEntity( turret );
	ent->client->turret = turret;
	return;

refund:
	BG_SetAbility( &ent->client->ps, BG_AbilityState( &ent->client->ps ), BG_AbilityCharges( &ent->client->ps ) + 1 );
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
	case ACTIVE_TURRET:			G_PlaceTurret( ent ); break;
	case ACTIVE_ACIDSPIT:
		ent->client->abilityShots = 0;
		ent->client->abilityShotTime = level.time;
		break;
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
void G_AbilityClientThink( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( G_AbilityActive( ent, ACTIVE_ACIDSPIT ) ) {
		while ( client->abilityShots < g_acidBlobs.integer && level.time >= client->abilityShotTime ) {
			G_SpitAcid( ent );
			client->abilityShots++;
			client->abilityShotTime += g_acidInterval.integer;
		}
	}

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
	if ( client->abilityEnt ) {
		G_FreeEntity( client->abilityEnt );
		client->abilityEnt = NULL;
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		client->fireballs[i] = NULL;
	}
}
