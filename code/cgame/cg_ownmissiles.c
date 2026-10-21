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
// cg_ownmissiles.c -- our own projectiles are drawn the moment we fire,
// from the predicted muzzle and aim, and hand over to the server's copies
// when those arrive

#include "cg_local.h"

#define MAX_OWN_MISSILES		32
#define OWN_MISSILE_PRESTEP		25		// the server starts projectiles this far along
#define OWN_MISSILE_TIMEOUT		1000	// never wait longer than this for the server

typedef struct {
	qboolean	active;
	qboolean	hidden;				// met a wall or player; the server shows the rest
	int			fireTime;			// cg.time when fired
	int			commandTime;		// our predicted command time when fired
	vec3_t		lastOrigin;
	centity_t	cent;				// drawn like any projectile
} ownMissile_t;

static ownMissile_t	ownMissiles[MAX_OWN_MISSILES];

/*
=================
CG_OwnMissileLaunch

What the server launches for a weapon: the projectile's own weapon number,
flight and speed. False for weapons we do not predict.
=================
*/
static qboolean CG_OwnMissileLaunch( int weapon, int *missileWeapon, trType_t *trType, float *speed ) {
	float	tuned = BG_WeaponTuned( weapon ) ? BG_WeaponParam( weapon, WPP_SPEED ) : 0;

	switch ( weapon ) {
	case WP_ROCKET_LAUNCHER:
		*missileWeapon = WP_ROCKET_LAUNCHER;
		*trType = TR_LINEAR;
		*speed = tuned > 0 ? tuned : 900;
		return qtrue;
	case WP_TRIBOLT:
		*missileWeapon = WP_GRENADE_LAUNCHER;
		*trType = TR_LINEAR;
		*speed = tuned > 0 ? tuned : 700;
		return qtrue;
	case WP_SUPER_NAILGUN:
#ifndef MISSIONPACK
	case WP_NAILGUN:
#endif
		*missileWeapon = WP_PLASMAGUN;
		*trType = TR_LINEAR;
		*speed = tuned > 0 ? tuned : 2000;
		return qtrue;
	case WP_BFG:
		*missileWeapon = WP_BFG;
		*trType = TR_LINEAR;
		*speed = 2000;
		return qtrue;
	default:
		return qfalse;
	}
}

/*
=================
CG_PredictOwnMissile

Called when we fire; starts drawing the projectile at once
=================
*/
void CG_PredictOwnMissile( int weapon, const vec3_t muzzle, const vec3_t forward ) {
	ownMissile_t	*om, *oldest = NULL;
	entityState_t	*s;
	int				i, missileWeapon;
	trType_t		trType;
	float			speed;
	vec3_t			dir;

	if ( !CG_OwnMissileLaunch( weapon, &missileWeapon, &trType, &speed ) ) {
		return;
	}

	om = NULL;
	for ( i = 0 ; i < MAX_OWN_MISSILES ; i++ ) {
		if ( !ownMissiles[i].active ) {
			om = &ownMissiles[i];
			break;
		}
		if ( !oldest || ownMissiles[i].fireTime < oldest->fireTime ) {
			oldest = &ownMissiles[i];
		}
	}
	if ( !om ) {
		om = oldest;
	}

	memset( om, 0, sizeof( *om ) );
	om->active = qtrue;
	om->fireTime = cg.time;
	om->commandTime = cg.predictedPlayerState.commandTime;

	s = &om->cent.currentState;
	s->number = ENTITYNUM_NONE;
	s->eType = ET_MISSILE;
	s->weapon = missileWeapon;
	s->otherEntityNum2 = cg.predictedPlayerState.clientNum;
	s->pos.trType = trType;
	s->pos.trTime = cg.time - OWN_MISSILE_PRESTEP;
	VectorCopy( muzzle, s->pos.trBase );
	VectorNormalize2( forward, dir );
	VectorScale( dir, speed, s->pos.trDelta );
	SnapVector( s->pos.trDelta );

	VectorCopy( muzzle, om->lastOrigin );
	VectorCopy( muzzle, om->cent.lerpOrigin );
	om->cent.trailTime = cg.time;
	om->cent.currentValid = qtrue;
}

/*
=================
CG_OwnMissileOrigin

Where a predicted projectile is now
=================
*/
static void CG_OwnMissileOrigin( ownMissile_t *om, vec3_t origin ) {
	if ( om->hidden ) {
		VectorCopy( om->lastOrigin, origin );
		return;
	}
	BG_EvaluateTrajectory( &om->cent.currentState.pos, cg.time, origin );
}

/*
=================
CG_UpdateOwnMissiles

Once per frame, before entities are added: hands predicted projectiles over
to the server's copies, and drops those the server never made
=================
*/
void CG_UpdateOwnMissiles( void ) {
	entityState_t	*es;
	centity_t		*cent;
	ownMissile_t	*om, *match;
	vec3_t			predicted, server;
	int				i, j;

	for ( i = 0 ; i < cg.snap->numEntities ; i++ ) {
		es = &cg.snap->entities[i];
		if ( es->eType != ET_MISSILE ) {
			continue;
		}
		cent = &cg_entities[es->number];
		if ( cent->ownMissile != OWNMISSILE_UNCHECKED ) {
			continue;
		}
		cent->ownMissile = OWNMISSILE_OTHER;
		if ( es->otherEntityNum2 != cg.snap->ps.clientNum || !CG_PredictingOwnShots() ) {
			continue;
		}

		// the oldest prediction still waiting for a projectile of this kind
		match = NULL;
		for ( j = 0 ; j < MAX_OWN_MISSILES ; j++ ) {
			om = &ownMissiles[j];
			if ( om->active && om->cent.currentState.weapon == es->weapon
				&& ( !match || om->fireTime < match->fireTime ) ) {
				match = om;
			}
		}
		if ( !match ) {
			continue;
		}

		// blend from where we drew it to where the server has it
		CG_OwnMissileOrigin( match, predicted );
		BG_EvaluateTrajectory( &es->pos, cg.time, server );
		VectorSubtract( predicted, server, cent->ownBlendOffset );
		cent->ownBlendTime = cg.time;
		cent->ownMissile = OWNMISSILE_MATCHED;
		match->active = qfalse;
	}

	// the server has run the command that fired and made nothing for it
	for ( j = 0 ; j < MAX_OWN_MISSILES ; j++ ) {
		om = &ownMissiles[j];
		if ( !om->active ) {
			continue;
		}
		if ( cg.snap->ps.commandTime >= om->commandTime || cg.time - om->fireTime > OWN_MISSILE_TIMEOUT
			|| cg.time < om->fireTime || !CG_PredictingOwnShots() ) {
			om->active = qfalse;
		}
	}
}

/*
=================
CG_AddOwnMissiles

Draws the predicted projectiles still waiting for the server; one that
meets a wall or a player stops being drawn, except a sticky bolt, which
stays where it hit
=================
*/
void CG_AddOwnMissiles( void ) {
	ownMissile_t	*om;
	trace_t			tr;
	vec3_t			origin;
	int				i;

	for ( i = 0 ; i < MAX_OWN_MISSILES ; i++ ) {
		om = &ownMissiles[i];
		if ( !om->active ) {
			continue;
		}
		if ( !om->hidden ) {
			BG_EvaluateTrajectory( &om->cent.currentState.pos, cg.time, origin );
			CG_Trace( &tr, om->lastOrigin, NULL, NULL, origin, cg.predictedPlayerState.clientNum, MASK_SHOT );
			VectorCopy( tr.endpos, om->lastOrigin );
			if ( tr.fraction < 1.0f || tr.startsolid ) {
				om->hidden = qtrue;
			}
		}
		if ( om->hidden && om->cent.currentState.weapon != WP_GRENADE_LAUNCHER ) {
			continue;
		}
		VectorCopy( om->lastOrigin, om->cent.lerpOrigin );
		CG_DrawMissile( &om->cent );
	}
}
