/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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
// g_weapon.c 
// perform the server side effects of a weapon firing

#include "g_local.h"

static	float	s_quadFactor;

void CalcMuzzlePointOrigin( gentity_t *ent, vec3_t origin, vec3_t localForward, vec3_t localRight, vec3_t localUp, vec3_t muzzlePoint );

/*
================
G_WeaponKnock

Knockback a weapon gives this target: its own number in the air, standing
or on its owner; G_NextKnockback scales it
================
*/
static void G_WeaponKnock( int weapon, gentity_t *target, gentity_t *attacker ) {
	if ( !BG_WeaponTuned( weapon ) ) {
		return;
	}
	if ( target == attacker && BG_WeaponParam( weapon, WPP_KNOCKSELF ) > 0 ) {
		G_NextKnockback( BG_WeaponParam( weapon, WPP_KNOCKSELF ) );
	} else if ( target->client && target->client->ps.groundEntityNum == ENTITYNUM_NONE ) {
		G_NextKnockback( BG_WeaponParam( weapon, WPP_KNOCKAIR ) );
	} else {
		G_NextKnockback( BG_WeaponParam( weapon, WPP_KNOCK ) );
	}
}

/*
================
G_WeaponTuneMissile

Copies a tuned weapon's projectile numbers onto a missile
================
*/
void G_WeaponTuneMissile( gentity_t *m, int weapon ) {
	vec3_t	dir;

	if ( !BG_WeaponTuned( weapon ) ) {
		return;
	}
	m->wpTuned = qtrue;
	m->damage = BG_WeaponParam( weapon, WPP_DAMAGE );
	m->splashDamage = BG_WeaponParam( weapon, WPP_SPLASH );
	m->splashMin = BG_WeaponParam( weapon, WPP_SPLASHMIN );
	m->splashRadius = BG_WeaponParam( weapon, WPP_RADIUS );
	// full strength near the blast: Rocket 8 u, Tri-bolt 12 u; nails are flat
	switch ( weapon ) {
	case WP_ROCKET_LAUNCHER:	m->splashFull = 8; break;
	case WP_TRIBOLT:			m->splashFull = 12; break;
	case WP_NAILGUN:
	case WP_SUPER_NAILGUN:		m->splashFull = m->splashRadius; break;
	default:					break;
	}
	m->wpKnock = BG_WeaponParam( weapon, WPP_KNOCK );
	m->wpKnockSelf = BG_WeaponParam( weapon, WPP_KNOCKSELF );
	if ( BG_WeaponParam( weapon, WPP_SPEED ) > 0 ) {
		VectorNormalize2( m->s.pos.trDelta, dir );
		VectorScale( dir, BG_WeaponParam( weapon, WPP_SPEED ), m->s.pos.trDelta );
		SnapVector( m->s.pos.trDelta );
	}
}
static	vec3_t	forward, right, up;
static	vec3_t	muzzle;

#define NUM_NAILSHOTS 15

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout ) {
	vec3_t v, newv;
	float dot;

	VectorSubtract( impact, start, v );
	dot = DotProduct( v, dir );
	VectorMA( v, -2*dot, dir, newv );

	VectorNormalize(newv);
	VectorMA(impact, 8192, newv, endout);
}


/*
======================================================================

GAUNTLET

======================================================================
*/

void Weapon_Gauntlet( gentity_t *ent ) {

}

/*
===============
G_BerserkTarget

Berserk punches reach everything inside a cylinder in front of the player:
the nearest enemy along it, if nothing solid is in the way
===============
*/
static gentity_t *G_BerserkTarget( gentity_t *ent, vec3_t org, vec3_t dir, trace_t *tr ) {
	gentity_t	*best = NULL, *other;
	vec3_t		delta, point;
	float		along, side, bestAlong = 0;
	int			i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( other == ent || !other->inuse || !other->client || other->health <= 0 || !other->takedamage
			|| other->client->sess.sessionTeam == TEAM_SPECTATOR || OnSameTeam( ent, other ) ) {
			continue;
		}
		VectorSubtract( other->r.currentOrigin, org, delta );
		along = DotProduct( delta, dir );
		if ( along < 0 || along > g_berserkReach.value ) {
			continue;
		}
		VectorMA( delta, -along, dir, point );
		side = VectorLength( point ) - other->r.maxs[0];		// to the body's edge
		if ( side > g_berserkRadius.value ) {
			continue;
		}
		if ( best && along >= bestAlong ) {
			continue;
		}
		trap_Trace( tr, org, NULL, NULL, other->r.currentOrigin, ent->s.number, MASK_SOLID );
		if ( tr->fraction < 1.0f ) {
			continue;
		}
		best = other;
		bestAlong = along;
	}
	if ( best ) {
		VectorCopy( best->r.currentOrigin, tr->endpos );
		VectorNegate( dir, tr->plane.normal );
		tr->entityNum = best->s.number;
		tr->surfaceFlags = 0;
	}
	return best;
}

/*
===============
CheckGauntletAttack
===============
*/
qboolean CheckGauntletAttack( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			damage;

	// set aiming directions
	AngleVectors (ent->client->ps.viewangles, forward, right, up);

	CalcMuzzlePoint ( ent, forward, right, up, muzzle );

	// reach 30, 34 for the big four; the hit is a sphere of radius 16
	// (weapons.md): a 16 u box swept, then the target's box must lie within
	// 16 u of the sphere's centre where it stopped
	{
		const char	*name = BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->name;
		float		reach = BG_WeaponParam( WP_GAUNTLET, WPP_RANGE ) > 0 ? BG_WeaponParam( WP_GAUNTLET, WPP_RANGE ) : 30;
		vec3_t		hitMins = { -16, -16, -16 }, hitMaxs = { 16, 16, 16 };

		if ( !Q_stricmp( name, "sorlag" ) || !Q_stricmp( name, "clutch" ) || !Q_stricmp( name, "keel" )
			|| !Q_stricmp( name, "scalebearer" ) ) {
			reach += 4;
		}
		VectorMA( muzzle, reach, forward, end );
		if ( !G_AbilityActive( ent, ACTIVE_BERSERK ) || !G_BerserkTarget( ent, muzzle, forward, &tr ) ) {
			trap_Trace( &tr, muzzle, hitMins, hitMaxs, end, ent->s.number, MASK_SHOT );
			if ( tr.entityNum < ENTITYNUM_MAX_NORMAL && g_entities[tr.entityNum].takedamage ) {
				gentity_t	*t = &g_entities[tr.entityNum];
				vec3_t		v;
				int			i;

				for ( i = 0 ; i < 3 ; i++ ) {
					v[i] = tr.endpos[i] < t->r.absmin[i] ? t->r.absmin[i] - tr.endpos[i]
						: tr.endpos[i] > t->r.absmax[i] ? tr.endpos[i] - t->r.absmax[i] : 0;
				}
				if ( VectorLength( v ) > 16 ) {
					return qfalse;		// the box's corner touched, the sphere didn't
				}
			}
		}
	}
	if ( tr.surfaceFlags & SURF_NOIMPACT ) {
		return qfalse;
	}

	if ( ent->client->noclip ) {
		return qfalse;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	// send blood impact
	if ( traceEnt->takedamage && traceEnt->client ) {
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
		tent->s.otherEntityNum = traceEnt->s.number;
		tent->s.eventParm = DirToByte( tr.plane.normal );
		tent->s.weapon = ent->s.weapon;
	}

	if ( !traceEnt->takedamage) {
		return qfalse;
	}

	if (ent->client->ps.powerups[PW_QUAD] ) {
		G_AddEvent( ent, EV_POWERUP_QUAD, 0 );
		s_quadFactor = g_quadfactor.value;
	} else {
		s_quadFactor = 1;
	}
#ifdef MISSIONPACK
	if( ent->client->persistantPowerup && ent->client->persistantPowerup->item && ent->client->persistantPowerup->item->giTag == PW_DOUBLER ) {
		s_quadFactor *= 2;
	}
#endif

	damage = BG_WeaponParam( WP_GAUNTLET, WPP_DAMAGE ) * s_quadFactor;
	if ( G_AbilityActive( ent, ACTIVE_BERSERK ) ) {
		// one punch per victim per second
		if ( traceEnt->client ) {
			int		*last = &ent->client->berserkHitTime[traceEnt->s.number];

			if ( *last && level.time - *last < g_berserkHitCooldown.integer ) {
				return qfalse;
			}
			*last = level.time;
		}
		damage = g_berserkDamage.value * s_quadFactor;
	}
	G_WeaponKnock( WP_GAUNTLET, traceEnt, ent );
	G_Damage( traceEnt, ent, ent, forward, tr.endpos,
		damage, 0, MOD_GAUNTLET );
	if ( traceEnt->client && ( BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_CHARRING ) ) {
		G_Ignite( traceEnt, ent );
	}

	return qtrue;
}


/*
===============
G_LogAccuracyHit

A hit counted for accuracy, overall and for the weapon
===============
*/
void G_LogAccuracyHit( gclient_t *client, int weapon ) {
	if ( !client ) {
		return;
	}
	client->accuracy_hits++;
	if ( weapon > WP_NONE && weapon < WP_NUM_WEAPONS ) {
		client->pers.accHits[weapon]++;
	}
}

/*
======================================================================

MACHINEGUN

======================================================================
*/

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating 
into a wall.
======================
*/
void SnapVectorTowards( vec3_t v, vec3_t to ) {
	int		i;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( to[i] <= v[i] ) {
			v[i] = floor(v[i]);
		} else {
			v[i] = ceil(v[i]);
		}
	}
}

#ifdef MISSIONPACK
#define CHAINGUN_SPREAD		600
#define CHAINGUN_DAMAGE		7
#endif
#define MACHINEGUN_SPREAD	200
#define	MACHINEGUN_DAMAGE	7
#define	MACHINEGUN_TEAM_DAMAGE	5		// wimpier MG in teamplay

void Bullet_Fire (gentity_t *ent, float spread, int damage, int mod ) {
	trace_t		tr;
	vec3_t		end;
#ifdef MISSIONPACK
	vec3_t		impactpoint, bouncedir;
#endif
	float		r;
	float		u;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			i, passent;
	float		range;

	damage *= s_quadFactor;

	// spread is the sine of the largest angle off the aim line
	range = BG_WeaponTuned( ent->s.weapon ) ? BG_WeaponParam( ent->s.weapon, WPP_RANGE ) : 8192*16;
	r = random() * M_PI * 2.0f;
	u = sin(r) * crandom() * spread * range;
	r = cos(r) * crandom() * spread * range;
	VectorMA (muzzle, range, forward, end);
	VectorMA (end, r, right, end);
	VectorMA (end, u, up, end);

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {

		G_ShotTrace (&tr, muzzle, NULL, NULL, end, passent, MASK_SHOT);
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		// snap the endpos to integers, but nudged towards the line
		SnapVectorTowards( tr.endpos, muzzle );

		// send bullet impact
		if ( traceEnt->takedamage && traceEnt->client ) {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
			tent->s.eventParm = traceEnt->s.number;
			if( LogAccuracyHit( traceEnt, ent ) ) {
				G_LogAccuracyHit( ent->client, ent->s.weapon );
			}
		} else {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
			tent->s.eventParm = DirToByte( tr.plane.normal );
		}
		tent->s.otherEntityNum = ent->s.number;

		if ( traceEnt->takedamage) {
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					VectorCopy( impactpoint, muzzle );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, muzzle );
					passent = traceEnt->s.number;
				}
				continue;
			}
			else {
#endif
				G_WeaponKnock( ent->s.weapon, traceEnt, ent );
				G_Damage( traceEnt, ent, ent, forward, tr.endpos,
					damage, 0, mod);
#ifdef MISSIONPACK
			}
#endif
		}
		break;
	}
}


/*
======================================================================

BFG

======================================================================
*/

void BFG_Fire ( gentity_t *ent ) {
	gentity_t	*m;

	m = fire_bfg (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}


/*
======================================================================

SHOTGUN

======================================================================
*/

// DEFAULT_SHOTGUN_SPREAD and DEFAULT_SHOTGUN_COUNT	are in bg_public.h, because
// client predicts same spreads
#define	DEFAULT_SHOTGUN_DAMAGE	10

static qboolean	shotgunBonusGiven;		// this shot's close-range bonus has landed

qboolean ShotgunPellet( vec3_t start, vec3_t end, gentity_t *ent, int weapon ) {
	trace_t		tr;
	int			damage, i, passent;
	gentity_t	*traceEnt;
#ifdef MISSIONPACK
	vec3_t		impactpoint, bouncedir;
#endif
	vec3_t		tr_start, tr_end;
	qboolean	hitClient = qfalse;

	passent = ent->s.number;
	VectorCopy( start, tr_start );
	VectorCopy( end, tr_end );
	for (i = 0; i < 10; i++) {
		G_ShotTrace (&tr, tr_start, NULL, NULL, tr_end, passent, MASK_SHOT);
		traceEnt = &g_entities[ tr.entityNum ];

		// send bullet impact
		if (  tr.surfaceFlags & SURF_NOIMPACT ) {
			return qfalse;
		}

		if ( traceEnt->takedamage) {
			float	dist = Distance( start, tr.endpos );
			float	scale = 1.0f;
			int		pellets = BG_WeaponParam( weapon, WPP_PELLETS );
			float	bonus = 0;

			// beyond g_shotgunFalloffStep each pellet loses a point per further
			// step (or part of one); the close-range bonus, fading out from its
			// range to g_*CloseFade, goes once a shot to the first pellet to hit
			if ( g_shotgunFalloffStep.value > 0 ) {
				scale = 0;		// the step decides, not a factor
			}
			if ( weapon == WP_SUPER_SHOTGUN ) {
				if ( dist <= g_ssgCloseRange.value ) {
					bonus = g_ssgCloseBonus.value;
				} else if ( dist < g_ssgCloseFade.value ) {
					bonus = g_ssgCloseBonus.value * ( g_ssgCloseFade.value - dist ) / ( g_ssgCloseFade.value - g_ssgCloseRange.value );
				}
			} else if ( dist <= g_shotgunCloseRange.value ) {
				bonus = g_shotgunCloseBonus.value;
			} else if ( dist < g_shotgunCloseFade.value ) {
				bonus = g_shotgunCloseBonus.value * ( g_shotgunCloseFade.value - dist ) / ( g_shotgunCloseFade.value - g_shotgunCloseRange.value );
			}
			if ( scale == 0 ) {
				damage = BG_WeaponParam( weapon, WPP_DAMAGE );
				if ( dist > g_shotgunFalloffStep.value ) {
					float	over = dist - g_shotgunFalloffStep.value;
					int		steps = (int)( over / g_shotgunFalloffStep.value );

					if ( steps * g_shotgunFalloffStep.value < over ) {
						steps++;		// a part step counts
					}
					damage -= steps;
				}
				if ( damage < 1 ) {
					damage = 1;
				}
			} else {
				damage = BG_WeaponParam( weapon, WPP_DAMAGE );
			}
			if ( bonus > 0 && !shotgunBonusGiven ) {
				shotgunBonusGiven = qtrue;
				damage += bonus;
			}
			damage *= s_quadFactor;
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( tr_start, impactpoint, bouncedir, tr_end );
					VectorCopy( impactpoint, tr_start );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, tr_start );
					passent = traceEnt->s.number;
				}
				continue;
			}
#endif
			if( LogAccuracyHit( traceEnt, ent ) ) {
				hitClient = qtrue;
			}
			G_WeaponKnock( weapon, traceEnt, ent );
			G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_SHOTGUN);
			return hitClient;
		}
		return qfalse;
	}
	return qfalse;
}

// this should match CG_ShotgunPattern
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent, int weapon ) {
	int			i, count;
	float		range;
	float		r, u, m;
	vec3_t		end, start, side;
	vec3_t		localForward, localRight, localUp;
	qboolean	hitClient = qfalse;

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, localForward );
	PerpendicularVector( localRight, localForward );
	CrossProduct( localForward, localRight, localUp );

	// the pellet ring, the same on the clients
	shotgunBonusGiven = qfalse;
	count = BG_WeaponParam( weapon, WPP_PELLETS );
	range = BG_WeaponParam( weapon, WPP_RANGE );
	for ( i = 0 ; i < count ; i++ ) {
		BG_ShotgunPelletOffset( weapon, i, count, &seed, &r, &u, &m );
		// the pellet starts off the aim line by its ring's muzzle spacing
		VectorCopy( origin, start );
		if ( m > 0 ) {
			trace_t		tr;

			VectorScale( localRight, r, side );
			VectorMA( side, u, localUp, side );
			VectorNormalize( side );
			VectorMA( origin, m, side, start );
			// never past a wall or ceiling next to the muzzle
			trap_Trace( &tr, origin, NULL, NULL, start, ENTITYNUM_NONE, MASK_SOLID );
			VectorCopy( tr.endpos, start );
		}
		VectorMA( start, range, localForward, end);
		VectorMA (end, r * range, localRight, end);
		VectorMA (end, u * range, localUp, end);
		if( ShotgunPellet( start, end, ent, weapon ) && !hitClient ) {
			hitClient = qtrue;
			G_LogAccuracyHit( ent->client, ent->s.weapon );
		}
	}
}


void weapon_supershotgun_fire (gentity_t *ent) {
	gentity_t		*tent;

	// send shotgun blast
	tent = G_TempEntity( muzzle, EV_SHOTGUN );
	VectorScale( forward, 4096, tent->s.origin2 );
	SnapVector( tent->s.origin2 );
	tent->s.eventParm = rand() & 255;		// seed for spread pattern
	tent->s.otherEntityNum = ent->s.number;
	tent->s.weapon = ent->s.weapon;			// which shotgun's pattern

	ShotgunPattern( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent, ent->s.weapon );
}

/*
======================================================================

NAILGUNS AND TRI-BOLT

======================================================================
*/

static void Weapon_Nails_Fire( gentity_t *ent ) {
	gentity_t	*m;

	m = fire_plasma( ent, muzzle, forward );
	G_WeaponTuneMissile( m, ent->s.weapon );
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;
}

/*
================
G_TriboltImpact

A bolt that hits a player explodes at once; one that hits a wall sticks
there until its fuse runs out
================
*/
static void G_TriboltImpact( gentity_t *bolt, trace_t *trace ) {
	gentity_t	*other = &g_entities[trace->entityNum];
	vec3_t		point;

	if ( other->takedamage ) {
		G_NextKnockback( bolt->wpKnock );
		G_Damage( other, bolt, bolt->parent, NULL, trace->endpos, bolt->damage, 0, MOD_GRENADE );
		VectorCopy( trace->endpos, point );
		G_SetOrigin( bolt, point );
		G_ExplodeMissile( bolt );
		return;
	}
	VectorMA( trace->endpos, 1, trace->plane.normal, point );
	SnapVector( point );
	G_SetOrigin( bolt, point );
	bolt->abilityImpact = NULL;
	bolt->clipmask = 0;
	trap_LinkEntity( bolt );
}

static void G_FireTribolt( gentity_t *ent ) {
	gentity_t	*bolt;

	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePointOrigin( ent, ent->client->oldOrigin, forward, right, up, muzzle );

	bolt = fire_grenade( ent, muzzle, forward );
	bolt->classname = "tribolt";
	bolt->s.eFlags = 0;							// no bouncing
	bolt->s.pos.trType = TR_GRAVITY_LOW;		// bolts drop at 400 u/s^2
	G_WeaponTuneMissile( bolt, WP_TRIBOLT );
	bolt->damage *= s_quadFactor;
	bolt->splashDamage *= s_quadFactor;
	bolt->methodOfDeath = MOD_GRENADE;
	bolt->splashMethodOfDeath = MOD_GRENADE_SPLASH;
	bolt->abilityImpact = G_TriboltImpact;
	bolt->nextthink = level.time + g_triboltFuse.integer;
}

/*
================
G_WeaponThink

Per-frame weapon work: the rest of a tri-bolt burst
================
*/
void G_WeaponThink( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( client->triboltShots <= 0 ) {
		return;
	}
	if ( ent->health <= 0 ) {
		client->triboltShots = 0;
		return;
	}
	if ( level.time >= client->triboltTime ) {
		s_quadFactor = client->ps.powerups[PW_QUAD] ? g_quadfactor.value : 1;
		G_FireTribolt( ent );
		client->triboltShots--;
		client->triboltTime += g_triboltInterval.integer;
	}
}


/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void weapon_grenadelauncher_fire (gentity_t *ent) {
	gentity_t	*m;

	// extra vertical velocity
	forward[2] += 0.2f;
	VectorNormalize( forward );

	m = fire_grenade (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire (gentity_t *ent) {
	gentity_t	*m;

	m = fire_rocket (ent, muzzle, forward);
	G_WeaponTuneMissile( m, WP_ROCKET_LAUNCHER );
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}


/*
======================================================================

PLASMA GUN

======================================================================
*/

void Weapon_Plasmagun_Fire (gentity_t *ent) {
	gentity_t	*m;

	m = fire_plasma (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

RAILGUN

======================================================================
*/


/*
=================
weapon_railgun_fire
=================
*/
#define	MAX_RAIL_HITS	4
void weapon_railgun_fire (gentity_t *ent) {
	vec3_t		end;
#ifdef MISSIONPACK
	vec3_t impactpoint, bouncedir;
#endif
	trace_t		trace;
	gentity_t	*tent;
	gentity_t	*traceEnt;
	int			damage;
	int			i;
	int			hits;
	int			unlinked;
	int			passent;
	gentity_t	*unlinkedEntities[MAX_RAIL_HITS];

	damage = BG_WeaponParam( WP_RAILGUN, WPP_DAMAGE ) * s_quadFactor;
	// Visor's Piercing Sight makes the rail hit harder while it runs
	if ( ent->client && G_AbilityActive( ent, ACTIVE_PIERCINGSIGHT ) ) {
		damage = g_railSightDamage.integer * s_quadFactor;
	}

	VectorMA (muzzle, BG_WeaponParam( WP_RAILGUN, WPP_RANGE ), forward, end);

	// trace only against the solids, so the railgun will go through people
	unlinked = 0;
	hits = 0;
	passent = ent->s.number;
	do {
		G_ShotTrace (&trace, muzzle, NULL, NULL, end, passent, MASK_SHOT );
		if ( trace.entityNum >= ENTITYNUM_MAX_NORMAL ) {
			break;
		}
		traceEnt = &g_entities[ trace.entityNum ];
		if ( traceEnt->takedamage ) {
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if ( G_InvulnerabilityEffect( traceEnt, forward, trace.endpos, impactpoint, bouncedir ) ) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					// snap the endpos to integers to save net bandwidth, but nudged towards the line
					SnapVectorTowards( trace.endpos, muzzle );
					// send railgun beam effect
					tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );
					// set player number for custom colors on the railtrail
					tent->s.clientNum = ent->s.clientNum;
					VectorCopy( muzzle, tent->s.origin2 );
					// move origin a bit to come closer to the drawn gun muzzle
					VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
					VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );
					tent->s.eventParm = 255;	// don't make the explosion at the end
					//
					VectorCopy( impactpoint, muzzle );
					// the player can hit him/herself with the bounced rail
					passent = ENTITYNUM_NONE;
				}
			}
			else {
				if( LogAccuracyHit( traceEnt, ent ) ) {
					hits++;
				}
				G_WeaponKnock( WP_RAILGUN, traceEnt, ent );
				G_Damage (traceEnt, ent, ent, forward, trace.endpos, damage, 0, MOD_RAILGUN);
			}
#else
				if( LogAccuracyHit( traceEnt, ent ) ) {
					hits++;
				}
				G_WeaponKnock( WP_RAILGUN, traceEnt, ent );
				G_Damage (traceEnt, ent, ent, forward, trace.endpos, damage, 0, MOD_RAILGUN);
#endif
		}
		if ( trace.contents & CONTENTS_SOLID ) {
			break;		// we hit something solid enough to stop the beam
		}
		// a door, or an enemy's Barrier (which took the hit), stops it too
		if ( traceEnt->s.eType == ET_MOVER || ( traceEnt->client && traceEnt->client->barrierMode == 1
			&& G_AbilityActive( traceEnt, ACTIVE_BARRIER ) && !OnSameTeam( traceEnt, ent ) ) ) {
			break;
		}
		// unlink this entity, so the next trace will go past it
		trap_UnlinkEntity( traceEnt );
		unlinkedEntities[unlinked] = traceEnt;
		unlinked++;
	} while ( unlinked < MAX_RAIL_HITS );

	// link back in any entities we unlinked
	for ( i = 0 ; i < unlinked ; i++ ) {
		trap_LinkEntity( unlinkedEntities[i] );
	}

	// the final trace endpos will be the terminal point of the rail trail

	// snap the endpos to integers to save net bandwidth, but nudged towards the line
	SnapVectorTowards( trace.endpos, muzzle );

	// send railgun beam effect
	tent = G_TempEntity( trace.endpos, EV_RAILTRAIL );

	// set player number for custom colors on the railtrail
	tent->s.clientNum = ent->s.clientNum;

	VectorCopy( muzzle, tent->s.origin2 );
	// move origin a bit to come closer to the drawn gun muzzle
	VectorMA( tent->s.origin2, 4, right, tent->s.origin2 );
	VectorMA( tent->s.origin2, -1, up, tent->s.origin2 );

	// no explosion at end if SURF_NOIMPACT, but still make the trail
	if ( trace.surfaceFlags & SURF_NOIMPACT ) {
		tent->s.eventParm = 255;	// don't make the explosion at the end
	} else {
		tent->s.eventParm = DirToByte( trace.plane.normal );
	}
	tent->s.clientNum = ent->s.clientNum;

	G_MedalsRail( ent, hits );

	// give the shooter a reward sound if they have made two railgun hits in a row
	if ( hits == 0 ) {
		// complete miss
		ent->client->accurateCount = 0;
	} else {
		// check for "impressive" reward sound
		ent->client->accurateCount += hits;
		if ( ent->client->accurateCount >= 2 ) {
			ent->client->accurateCount -= 2;
			ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
			G_BankKick( ent );		// Xaero's Leaping Kick
			// add the sprite over the player's head
			ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
			ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
			ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
		}
		G_LogAccuracyHit( ent->client, ent->s.weapon );
	}

}


/*
======================================================================

GRAPPLING HOOK

======================================================================
*/

void Weapon_GrapplingHook_Fire (gentity_t *ent)
{
	if (!ent->client->fireHeld && !ent->client->hook)
		fire_grapple (ent, muzzle, forward);

	ent->client->fireHeld = qtrue;
}

void Weapon_HookFree (gentity_t *ent)
{
	ent->parent->client->hook = NULL;
	ent->parent->client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
	G_FreeEntity( ent );
}

void Weapon_HookThink (gentity_t *ent)
{
	if (ent->enemy) {
		vec3_t v, oldorigin;

		VectorCopy(ent->r.currentOrigin, oldorigin);
		v[0] = ent->enemy->r.currentOrigin[0] + (ent->enemy->r.mins[0] + ent->enemy->r.maxs[0]) * 0.5;
		v[1] = ent->enemy->r.currentOrigin[1] + (ent->enemy->r.mins[1] + ent->enemy->r.maxs[1]) * 0.5;
		v[2] = ent->enemy->r.currentOrigin[2] + (ent->enemy->r.mins[2] + ent->enemy->r.maxs[2]) * 0.5;
		SnapVectorTowards( v, oldorigin );	// save net bandwidth

		G_SetOrigin( ent, v );
	}

	VectorCopy( ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);
}

/*
======================================================================

LIGHTNING GUN

======================================================================
*/

void Weapon_LightningFire( gentity_t *ent ) {
	trace_t		tr;
	vec3_t		end;
#ifdef MISSIONPACK
	vec3_t impactpoint, bouncedir;
#endif
	gentity_t	*traceEnt, *tent;
	int			damage, i, passent;

	damage = BG_WeaponParam( WP_LIGHTNING, WPP_DAMAGE ) * s_quadFactor;

	passent = ent->s.number;
	for (i = 0; i < 10; i++) {
		VectorMA( muzzle, BG_WeaponParam( WP_LIGHTNING, WPP_RANGE ), forward, end );

		G_ShotTrace( &tr, muzzle, NULL, NULL, end, passent, MASK_SHOT );

#ifdef MISSIONPACK
		// if not the first trace (the lightning bounced of an invulnerability sphere)
		if (i) {
			// add bounced off lightning bolt temp entity
			// the first lightning bolt is a cgame only visual
			//
			tent = G_TempEntity( muzzle, EV_LIGHTNINGBOLT );
			VectorCopy( tr.endpos, end );
			SnapVector( end );
			VectorCopy( end, tent->s.origin2 );
		}
#endif
		if ( tr.entityNum == ENTITYNUM_NONE ) {
			return;
		}

		traceEnt = &g_entities[ tr.entityNum ];

		if ( traceEnt->takedamage) {
#ifdef MISSIONPACK
			if ( traceEnt->client && traceEnt->client->invulnerabilityTime > level.time ) {
				if (G_InvulnerabilityEffect( traceEnt, forward, tr.endpos, impactpoint, bouncedir )) {
					G_BounceProjectile( muzzle, impactpoint, bouncedir, end );
					VectorCopy( impactpoint, muzzle );
					VectorSubtract( end, impactpoint, forward );
					VectorNormalize(forward);
					// the player can hit him/herself with the bounced lightning
					passent = ENTITYNUM_NONE;
				}
				else {
					VectorCopy( tr.endpos, muzzle );
					passent = traceEnt->s.number;
				}
				continue;
			}
#endif
			if( LogAccuracyHit( traceEnt, ent ) ) {
				G_LogAccuracyHit( ent->client, ent->s.weapon );
			}
			G_WeaponKnock( WP_LIGHTNING, traceEnt, ent );
			G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_LIGHTNING);
		}

		if ( traceEnt->takedamage && traceEnt->client ) {
			tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
			tent->s.otherEntityNum = traceEnt->s.number;
			tent->s.eventParm = DirToByte( tr.plane.normal );
			tent->s.weapon = ent->s.weapon;
		} else if ( !( tr.surfaceFlags & SURF_NOIMPACT ) ) {
			tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
			tent->s.eventParm = DirToByte( tr.plane.normal );
		}

		break;
	}
}

#ifdef MISSIONPACK
/*
======================================================================

NAILGUN

======================================================================
*/

void Weapon_Nailgun_Fire (gentity_t *ent) {
	gentity_t	*m;
	int			count;

	for( count = 0; count < NUM_NAILSHOTS; count++ ) {
		m = fire_nail (ent, muzzle, forward, right, up );
		m->damage *= s_quadFactor;
		m->splashDamage *= s_quadFactor;
	}

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}


/*
======================================================================

PROXIMITY MINE LAUNCHER

======================================================================
*/

void weapon_proxlauncher_fire (gentity_t *ent) {
	gentity_t	*m;

	// extra vertical velocity
	forward[2] += 0.2f;
	VectorNormalize( forward );

	m = fire_prox (ent, muzzle, forward);
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

#endif

//======================================================================


/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {
	if( !target->takedamage ) {
		return qfalse;
	}

	if ( target == attacker ) {
		return qfalse;
	}

	if( !target->client ) {
		return qfalse;
	}

	if( !attacker->client ) {
		return qfalse;
	}

	if( target->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return qfalse;
	}

	if ( OnSameTeam( target, attacker ) ) {
		return qfalse;
	}

	return qtrue;
}


/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePoint ( gentity_t *ent, vec3_t localForward, vec3_t localRight, vec3_t localUp, vec3_t muzzlePoint ) {
	VectorCopy( ent->s.pos.trBase, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA( muzzlePoint, 14, localForward, muzzlePoint );
	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}

/*
===============
CalcMuzzlePointOrigin

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePointOrigin ( gentity_t *ent, vec3_t origin, vec3_t localForward, vec3_t localRight, vec3_t localUp, vec3_t muzzlePoint ) {
	VectorCopy( ent->s.pos.trBase, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA( muzzlePoint, 14, localForward, muzzlePoint );
	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}


/*
===============
Ability_HookFire

Athena's hook, thrown by EV_USE_ABILITY from pmove
===============
*/
void Ability_HookFire( gentity_t *ent ) {
	gentity_t	*hook;
	float		speed;

	if ( ent->client->hook ) {
		Weapon_HookFree( ent->client->hook );
	}

	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePointOrigin( ent, ent->client->oldOrigin, forward, right, up, muzzle );

	hook = fire_grapple( ent, muzzle, forward );
	speed = g_hookSpeed.value > 0 ? g_hookSpeed.value : 800;
	VectorScale( forward, speed, hook->s.pos.trDelta );
	SnapVector( hook->s.pos.trDelta );
	hook->nextthink = level.time + 1000 * g_hookRange.value / speed;
	hook->damage = g_hookDamage.integer;
}



/*
===============
FireWeapon
===============
*/
void FireWeapon( gentity_t *ent ) {
	ent->client->spawnProtectEnd = 0;		// firing ends spawn protection
	if (ent->client->ps.powerups[PW_QUAD] ) {
		s_quadFactor = g_quadfactor.value;
	} else {
		s_quadFactor = 1;
	}
#ifdef MISSIONPACK
	if( ent->client->persistantPowerup && ent->client->persistantPowerup->item && ent->client->persistantPowerup->item->giTag == PW_DOUBLER ) {
		s_quadFactor *= 2;
	}
#endif

	// track shots taken for accuracy tracking.  Grapple is not a weapon and gauntet is just not tracked
	if( ent->s.weapon != WP_GRAPPLING_HOOK && ent->s.weapon != WP_GAUNTLET ) {
#ifdef MISSIONPACK
		if( ent->s.weapon == WP_NAILGUN ) {
			ent->client->accuracy_shots += NUM_NAILSHOTS;
			ent->client->pers.accShots[ent->s.weapon] += NUM_NAILSHOTS;
		} else {
			ent->client->accuracy_shots++;
			ent->client->pers.accShots[ent->s.weapon]++;
		}
#else
		ent->client->accuracy_shots++;
		ent->client->pers.accShots[ent->s.weapon]++;
#endif
	}

	// set aiming directions
	AngleVectors (ent->client->ps.viewangles, forward, right, up);

	CalcMuzzlePointOrigin ( ent, ent->client->oldOrigin, forward, right, up, muzzle );

	// instant-hit weapons see other players where the shooter saw them
	switch( ent->s.weapon ) {
	case WP_MACHINEGUN:
	case WP_SHOTGUN:
	case WP_LIGHTNING:
	case WP_RAILGUN:
	case WP_HEAVY_MACHINEGUN:
	case WP_SUPER_SHOTGUN:
#ifdef MISSIONPACK
	case WP_CHAINGUN:
#endif
		G_RewindPlayers( ent );
		break;
	default:
		break;
	}

	// fire the specific weapon
	switch( ent->s.weapon ) {
	case WP_GAUNTLET:
		Weapon_Gauntlet( ent );
		break;
	case WP_LIGHTNING:
		Weapon_LightningFire( ent );
		break;
	case WP_SHOTGUN:
	case WP_SUPER_SHOTGUN:
		weapon_supershotgun_fire( ent );
		break;
	case WP_HEAVY_MACHINEGUN:
		if ( ent->client->pers.cmd.buttons & BUTTON_ZOOM ) {
			Bullet_Fire( ent, g_hmgZoomSpread.value, g_hmgZoomDamage.value, MOD_MACHINEGUN );
		} else {
			Bullet_Fire( ent, BG_WeaponParam( WP_HEAVY_MACHINEGUN, WPP_SPREAD ), BG_WeaponParam( WP_HEAVY_MACHINEGUN, WPP_DAMAGE ), MOD_MACHINEGUN );
		}
		break;
#ifndef MISSIONPACK
	case WP_NAILGUN:
		Weapon_Nails_Fire( ent );
		break;
#endif
	case WP_MACHINEGUN:
		if ( ent->client->ps.eFlags & EF_DRONE ) {
			Bullet_Fire( ent, BG_WeaponParam( WP_MACHINEGUN, WPP_SPREAD ), g_droneDamage.integer, MOD_DRONE );
		} else if ( BG_WeaponTuned( WP_MACHINEGUN ) ) {
			Bullet_Fire( ent, BG_WeaponParam( WP_MACHINEGUN, WPP_SPREAD ), BG_WeaponParam( WP_MACHINEGUN, WPP_DAMAGE ), MOD_MACHINEGUN );
		} else if ( g_gametype.integer != GT_TEAM ) {
			Bullet_Fire( ent, MACHINEGUN_SPREAD, MACHINEGUN_DAMAGE, MOD_MACHINEGUN );
		} else {
			Bullet_Fire( ent, MACHINEGUN_SPREAD, MACHINEGUN_TEAM_DAMAGE, MOD_MACHINEGUN );
		}
		break;
	case WP_TRIBOLT:
		// a burst of bolts, the first one now
		G_FireTribolt( ent );
		ent->client->triboltShots = g_triboltShots.integer - 1;
		ent->client->triboltTime = level.time + g_triboltInterval.integer;
		break;
	case WP_ROCKET_LAUNCHER:
		Weapon_RocketLauncher_Fire( ent );
		break;
	case WP_SUPER_NAILGUN:
		Weapon_Nails_Fire( ent );
		break;
	case WP_RAILGUN:
		weapon_railgun_fire( ent );
		break;
	case WP_BFG:
		BFG_Fire( ent );
		break;
	case WP_GRAPPLING_HOOK:
		Weapon_GrapplingHook_Fire( ent );
		break;
#ifdef MISSIONPACK
	case WP_NAILGUN:
		Weapon_Nailgun_Fire( ent );
		break;
	case WP_PROX_LAUNCHER:
		weapon_proxlauncher_fire( ent );
		break;
	case WP_CHAINGUN:
		Bullet_Fire( ent, CHAINGUN_SPREAD, CHAINGUN_DAMAGE, MOD_CHAINGUN );
		break;
#endif
	default:
// FIXME		G_Error( "Bad ent->s.weapon" );
		break;
	}

	G_RestorePlayers();
}


#ifdef MISSIONPACK

/*
===============
KamikazeRadiusDamage
===============
*/
static void KamikazeRadiusDamage( vec3_t origin, gentity_t *attacker, float damage, float radius ) {
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;

	if ( radius < 1 ) {
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if (!ent->takedamage) {
			continue;
		}

		// don't hit things we have already hit
		if( ent->kamikazeTime > level.time ) {
			continue;
		}

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

//		if( CanDamage (ent, origin) ) {
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;
			G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
			ent->kamikazeTime = level.time + 3000;
//		}
	}
}

/*
===============
KamikazeShockWave
===============
*/
static void KamikazeShockWave( vec3_t origin, gentity_t *attacker, float damage, float push, float radius ) {
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;

	if ( radius < 1 )
		radius = 1;

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		// don't hit things we have already hit
		if( ent->kamikazeShockTime > level.time ) {
			continue;
		}

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

//		if( CanDamage (ent, origin) ) {
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			dir[2] += 24;
			G_Damage( ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS|DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE );
			//
			dir[2] = 0;
			VectorNormalize(dir);
			if ( ent->client ) {
				ent->client->ps.velocity[0] = dir[0] * push;
				ent->client->ps.velocity[1] = dir[1] * push;
				ent->client->ps.velocity[2] = 100;
			}
			ent->kamikazeShockTime = level.time + 3000;
//		}
	}
}

/*
===============
KamikazeDamage
===============
*/
static void KamikazeDamage( gentity_t *self ) {
	int i;
	float t;
	gentity_t *ent;
	vec3_t newangles;

	self->count += 100;

	if (self->count >= KAMI_SHOCKWAVE_STARTTIME) {
		// shockwave push back
		t = self->count - KAMI_SHOCKWAVE_STARTTIME;
		KamikazeShockWave(self->s.pos.trBase, self->activator, 25, 400,	(int) (float) t * KAMI_SHOCKWAVE_MAXRADIUS / (KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME) );
	}
	//
	if (self->count >= KAMI_EXPLODE_STARTTIME) {
		// do our damage
		t = self->count - KAMI_EXPLODE_STARTTIME;
		KamikazeRadiusDamage( self->s.pos.trBase, self->activator, 400,	(int) (float) t * KAMI_BOOMSPHERE_MAXRADIUS / (KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME) );
	}

	// either cycle or kill self
	if( self->count >= KAMI_SHOCKWAVE_ENDTIME ) {
		G_FreeEntity( self );
		return;
	}
	self->nextthink = level.time + 100;

	// add earth quake effect
	newangles[0] = crandom() * 2;
	newangles[1] = crandom() * 2;
	newangles[2] = 0;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		ent = &g_entities[i];
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;

		if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
			ent->client->ps.velocity[0] += crandom() * 120;
			ent->client->ps.velocity[1] += crandom() * 120;
			ent->client->ps.velocity[2] = 30 + random() * 25;
		}

		ent->client->ps.delta_angles[0] += ANGLE2SHORT(newangles[0] - self->movedir[0]);
		ent->client->ps.delta_angles[1] += ANGLE2SHORT(newangles[1] - self->movedir[1]);
		ent->client->ps.delta_angles[2] += ANGLE2SHORT(newangles[2] - self->movedir[2]);
	}
	VectorCopy(newangles, self->movedir);
}

/*
===============
G_StartKamikaze
===============
*/
void G_StartKamikaze( gentity_t *ent ) {
	gentity_t	*explosion;
	gentity_t	*te;
	vec3_t		snapped;

	// start up the explosion logic
	explosion = G_Spawn();

	explosion->s.eType = ET_EVENTS + EV_KAMIKAZE;
	explosion->eventTime = level.time;

	if ( ent->client ) {
		VectorCopy( ent->s.pos.trBase, snapped );
	}
	else {
		VectorCopy( ent->activator->s.pos.trBase, snapped );
	}
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( explosion, snapped );

	explosion->classname = "kamikaze";
	explosion->s.pos.trType = TR_STATIONARY;

	explosion->kamikazeTime = level.time;

	explosion->think = KamikazeDamage;
	explosion->nextthink = level.time + 100;
	explosion->count = 0;
	VectorClear(explosion->movedir);

	trap_LinkEntity( explosion );

	if (ent->client) {
		//
		explosion->activator = ent;
		//
		ent->s.eFlags &= ~EF_KAMIKAZE;
		// nuke the guy that used it
		G_Damage( ent, ent, ent, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_KAMIKAZE );
	}
	else {
		if ( !strcmp(ent->activator->classname, "bodyque") ) {
			explosion->activator = &g_entities[ent->activator->r.ownerNum];
		}
		else {
			explosion->activator = ent->activator;
		}
	}

	// play global sound at all clients
	te = G_TempEntity(snapped, EV_GLOBAL_TEAM_SOUND );
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = GTS_KAMIKAZE;
}
#endif
