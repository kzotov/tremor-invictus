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
// cg_ents.c -- present snapshot entities, happens every single frame

#include "cg_local.h"


/*
======================
CG_PositionEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;
	
	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( lerped.axis, ((refEntity_t *)parent)->axis, entity->axis );
	entity->backlerp = parent->backlerp;
}


/*
======================
CG_PositionRotatedEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;
	vec3_t			tempAxis[3];

//AxisClear( entity->axis );
	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( entity->axis, lerped.axis, tempAxis );
	MatrixMultiply( tempAxis, ((refEntity_t *)parent)->axis, entity->axis );
}



/*
==========================================================================

FUNCTIONS CALLED EACH FRAME

==========================================================================
*/

/*
======================
CG_SetEntitySoundPosition

Also called by event processing code
======================
*/
void CG_SetEntitySoundPosition( centity_t *cent ) {
	if ( cent->currentState.solid == SOLID_BMODEL ) {
		vec3_t	origin;
		float	*v;

		v = cgs.inlineModelMidpoints[ cent->currentState.modelindex ];
		VectorAdd( cent->lerpOrigin, v, origin );
		trap_S_UpdateEntityPosition( cent->currentState.number, origin );
	} else {
		trap_S_UpdateEntityPosition( cent->currentState.number, cent->lerpOrigin );
	}
}

/*
==================
CG_EntityEffects

Add continuous entity effects, like local entity emission and lighting
==================
*/
static void CG_EntityEffects( centity_t *cent ) {

	// update sound origins
	CG_SetEntitySoundPosition( cent );

	// add loop sound
	if ( cent->currentState.loopSound ) {
		if (cent->currentState.eType != ET_SPEAKER) {
			trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, 
				cgs.gameSounds[ cent->currentState.loopSound ] );
		} else {
			trap_S_AddRealLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, 
				cgs.gameSounds[ cent->currentState.loopSound ] );
		}
	}


	// constant light glow
	if(cent->currentState.constantLight)
	{
		int		cl;
		float		i, r, g, b;

		cl = cent->currentState.constantLight;
		r = (float) (cl & 0xFF) / 255.0;
		g = (float) ((cl >> 8) & 0xFF) / 255.0;
		b = (float) ((cl >> 16) & 0xFF) / 255.0;
		i = (float) ((cl >> 24) & 0xFF) * 4.0;
		trap_R_AddLightToScene(cent->lerpOrigin, i, r, g, b);
	}

}


/*
==================
CG_General
==================
*/
static void CG_General( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// Strogg's body while he flies the drone
	if ( s1->eFlags & EF_DRONE ) {
		CG_DroneBody( cent );
		return;
	}

	// if set to invisible, skip
	if (!s1->modelindex) {
		return;
	}

	memset (&ent, 0, sizeof(ent));

	// set frame

	ent.frame = s1->frame;
	ent.oldframe = ent.frame;
	ent.backlerp = 0;

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.hModel = cgs.gameModels[s1->modelindex];

	// player model
	if (s1->number == cg.snap->ps.clientNum) {
		ent.renderfx |= RF_THIRD_PERSON;	// only draw from mirrors
	}

	// convert angles to axis
	AnglesToAxis( cent->lerpAngles, ent.axis );

	// Eisen's turret is painted like its owner (otherEntityNum)
	if ( ent.hModel && ent.hModel == cgs.media.turretModel ) {
		CG_OwnerColors( s1->otherEntityNum );
		CG_ColorBeforeDraw( &ent );
		trap_R_AddRefEntityToScene( &ent );
		CG_ColorAfterDraw( &ent );
		CG_ColorReset();
		return;
	}

	// add to refresh list
	trap_R_AddRefEntityToScene (&ent);
}

/*
==================
CG_Speaker

Speaker entities can automatically play sounds
==================
*/
static void CG_Speaker( centity_t *cent ) {
	if ( ! cent->currentState.clientNum ) {	// FIXME: use something other than clientNum...
		return;		// not auto triggering
	}

	if ( cg.time < cent->miscTime ) {
		return;
	}

	trap_S_StartSound (NULL, cent->currentState.number, CHAN_ITEM, cgs.gameSounds[cent->currentState.eventParm] );

	//	ent->s.frame = ent->wait * 10;
	//	ent->s.clientNum = ent->random * 10;
	cent->miscTime = cg.time + cent->currentState.frame * 100 + cent->currentState.clientNum * 100 * crandom();
}

/*
==================
CG_Item
==================
*/
#define ITEM_BLINK	5000		// (d)

static void CG_Item( centity_t *cent ) {
	refEntity_t		ent;
	entityState_t	*es;
	gitem_t			*item;
	int				msec;
	float			frac;
	float			scale;
	weaponInfo_t	*wi;

	es = &cent->currentState;
	if ( es->modelindex >= bg_numItems ) {
		CG_Error( "Bad item index %i on entity", es->modelindex );
	}

	// if set to invisible, skip
	if ( !es->modelindex || ( es->eFlags & EF_NODRAW ) ) {
		return;
	}
	CG_RegisterItemVisuals( es->modelindex );		// a power-up spot may switch items

	// hud.md corrections: a dropped item about to vanish blinks, hidden
	// 45 ms at a time while its visible time shrinks from 250 ms to 50 ms
	// over the last ITEM_BLINK ms
	if ( es->modelindex2 && es->time && es->time - cg.time < ITEM_BLINK ) {
		int		left = es->time - cg.time, shown;

		if ( left < 0 ) {
			left = 0;
		}
		shown = 50 + 200 * left / ITEM_BLINK;
		if ( cg.time % ( shown + 45 ) < 45 ) {
			return;
		}
	}

	item = &bg_itemlist[ es->modelindex ];
	if ( cg_simpleItems.integer && item->giType != IT_TEAM ) {
		memset( &ent, 0, sizeof( ent ) );
		ent.reType = RT_SPRITE;
		VectorCopy( cent->lerpOrigin, ent.origin );
		ent.radius = 14;
		ent.customShader = cg_items[es->modelindex].icon;
		ent.shaderRGBA[0] = 255;
		ent.shaderRGBA[1] = 255;
		ent.shaderRGBA[2] = 255;
		ent.shaderRGBA[3] = 255;
		trap_R_AddRefEntityToScene(&ent);
		return;
	}

	// items bob up and down continuously
	scale = 0.005 + cent->currentState.number * 0.00001;
	cent->lerpOrigin[2] += 4 + cos( ( cg.time + 1000 ) *  scale ) * 4;

	memset (&ent, 0, sizeof(ent));

	// autorotate at one of two speeds
	if ( item->giType == IT_HEALTH ) {
		VectorCopy( cg.autoAnglesFast, cent->lerpAngles );
		AxisCopy( cg.autoAxisFast, ent.axis );
	} else {
		VectorCopy( cg.autoAngles, cent->lerpAngles );
		AxisCopy( cg.autoAxis, ent.axis );
	}

	wi = NULL;
	// the weapons have their origin where they attatch to player
	// models, so we need to offset them or they will rotate
	// eccentricly
	if ( item->giType == IT_WEAPON ) {
		wi = &cg_weapons[item->giTag];
		cent->lerpOrigin[0] -= 
			wi->weaponMidpoint[0] * ent.axis[0][0] +
			wi->weaponMidpoint[1] * ent.axis[1][0] +
			wi->weaponMidpoint[2] * ent.axis[2][0];
		cent->lerpOrigin[1] -= 
			wi->weaponMidpoint[0] * ent.axis[0][1] +
			wi->weaponMidpoint[1] * ent.axis[1][1] +
			wi->weaponMidpoint[2] * ent.axis[2][1];
		cent->lerpOrigin[2] -= 
			wi->weaponMidpoint[0] * ent.axis[0][2] +
			wi->weaponMidpoint[1] * ent.axis[1][2] +
			wi->weaponMidpoint[2] * ent.axis[2][2];

		cent->lerpOrigin[2] += 8;	// an extra height boost
	}
	
	if( item->giType == IT_WEAPON && item->giTag == WP_RAILGUN ) {
		clientInfo_t *ci = &cgs.clientinfo[cg.snap->ps.clientNum];
		Byte4Copy( ci->c1RGBA, ent.shaderRGBA );
	}

	ent.hModel = cg_items[es->modelindex].models[0];

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.nonNormalizedAxes = qfalse;

	// if just respawned, slowly scale up
	msec = cg.time - cent->miscTime;
	if ( msec >= 0 && msec < ITEM_SCALEUP_TIME ) {
		frac = (float)msec / ITEM_SCALEUP_TIME;
		VectorScale( ent.axis[0], frac, ent.axis[0] );
		VectorScale( ent.axis[1], frac, ent.axis[1] );
		VectorScale( ent.axis[2], frac, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	} else {
		frac = 1.0;
	}

	// items without glow textures need to keep a minimum light value
	// so they are always visible
	if ( ( item->giType == IT_WEAPON ) ||
		 ( item->giType == IT_ARMOR ) ) {
		ent.renderfx |= RF_MINLIGHT;
	}

	// increase the size of the weapons when they are presented as items
	if ( item->giType == IT_WEAPON ) {
		VectorScale( ent.axis[0], 1.5, ent.axis[0] );
		VectorScale( ent.axis[1], 1.5, ent.axis[1] );
		VectorScale( ent.axis[2], 1.5, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
#ifdef MISSIONPACK
		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, cgs.media.weaponHoverSound );
#endif
	}

#ifdef MISSIONPACK
	if ( item->giType == IT_HOLDABLE && item->giTag == HI_KAMIKAZE ) {
		VectorScale( ent.axis[0], 2, ent.axis[0] );
		VectorScale( ent.axis[1], 2, ent.axis[1] );
		VectorScale( ent.axis[2], 2, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	}
#endif

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);

	if ( item->giType == IT_WEAPON && wi && wi->barrelModel ) {
		refEntity_t	barrel;
		vec3_t		angles;

		memset( &barrel, 0, sizeof( barrel ) );

		barrel.hModel = wi->barrelModel;

		VectorCopy( ent.lightingOrigin, barrel.lightingOrigin );
		barrel.shadowPlane = ent.shadowPlane;
		barrel.renderfx = ent.renderfx;

		angles[YAW] = 0;
		angles[PITCH] = 0;
		angles[ROLL] = 0;
		AnglesToAxis( angles, barrel.axis );

		CG_PositionRotatedEntityOnTag( &barrel, &ent, wi->weaponModel, "tag_barrel" );

		barrel.nonNormalizedAxes = ent.nonNormalizedAxes;

		trap_R_AddRefEntityToScene( &barrel );
	}

	// accompanying rings / spheres for powerups
	if ( !cg_simpleItems.integer ) 
	{
		vec3_t spinAngles;

		VectorClear( spinAngles );

		if ( item->giType == IT_HEALTH || item->giType == IT_POWERUP )
		{
			if ( ( ent.hModel = cg_items[es->modelindex].models[1] ) != 0 )
			{
				if ( item->giType == IT_POWERUP )
				{
					ent.origin[2] += 12;
					spinAngles[1] = ( cg.time & 1023 ) * 360 / -1024.0f;
				}
				AnglesToAxis( spinAngles, ent.axis );
				
				// scale up if respawning
				if ( frac != 1.0 ) {
					VectorScale( ent.axis[0], frac, ent.axis[0] );
					VectorScale( ent.axis[1], frac, ent.axis[1] );
					VectorScale( ent.axis[2], frac, ent.axis[2] );
					ent.nonNormalizedAxes = qtrue;
				}
				trap_R_AddRefEntityToScene( &ent );
			}
		}
	}
}

//============================================================================

/*
===============
CG_Missile
===============
*/
/*
==================
CG_VfxLine
==================
*/
static void CG_VfxLine( const vec3_t start, const vec3_t end, byte r, byte g, byte b, byte a ) {
	refEntity_t	re;

	memset( &re, 0, sizeof( re ) );
	re.reType = RT_RAIL_CORE;
	re.customShader = cgs.media.vfxLineShader;
	VectorCopy( start, re.origin );
	VectorCopy( end, re.oldorigin );
	re.shaderRGBA[0] = r;
	re.shaderRGBA[1] = g;
	re.shaderRGBA[2] = b;
	re.shaderRGBA[3] = a;
	trap_R_AddRefEntityToScene( &re );
}

/*
==================
CG_VfxCircle

A flat ring of lines, turned by `phase` degrees
==================
*/
static void CG_VfxCircle( const vec3_t center, float radius, int segments, float phase, byte r, byte g, byte b ) {
	vec3_t	p0, p1;
	float	a0, a1;
	int		i;

	for ( i = 0 ; i < segments ; i++ ) {
		a0 = DEG2RAD( phase + i * 360.0f / segments );
		a1 = DEG2RAD( phase + ( i + 1 ) * 360.0f / segments );
		VectorSet( p0, center[0] + cos( a0 ) * radius, center[1] + sin( a0 ) * radius, center[2] );
		VectorSet( p1, center[0] + cos( a1 ) * radius, center[1] + sin( a1 ) * radius, center[2] );
		CG_VfxLine( p0, p1, r, g, b, 255 );
	}
}

/*
==================
CG_VfxSprite

A camera-facing ball of light in the given colour
==================
*/
static void CG_VfxSprite( const vec3_t origin, float radius, qhandle_t shader, byte r, byte g, byte b, float rotation ) {
	refEntity_t	re;

	memset( &re, 0, sizeof( re ) );
	re.reType = RT_SPRITE;
	re.customShader = shader;
	re.radius = radius;
	re.rotation = rotation;
	VectorCopy( origin, re.origin );
	VectorCopy( origin, re.oldorigin );
	re.shaderRGBA[0] = r;
	re.shaderRGBA[1] = g;
	re.shaderRGBA[2] = b;
	re.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene( &re );
}

/*
==================
CG_VfxGroundRing

A ring of light lying flat, `radius` out from `center`, turned by `phase`
degrees
==================
*/
void CG_VfxGroundRing( const vec3_t center, float radius, float phase, byte r, byte g, byte b ) {
	polyVert_t	v[4];
	float		c = cos( DEG2RAD( phase ) ) * radius, s = sin( DEG2RAD( phase ) ) * radius;
	int			i;

	VectorSet( v[0].xyz, center[0] - c + s, center[1] - s - c, center[2] );
	VectorSet( v[1].xyz, center[0] + c + s, center[1] + s - c, center[2] );
	VectorSet( v[2].xyz, center[0] + c - s, center[1] + s + c, center[2] );
	VectorSet( v[3].xyz, center[0] - c - s, center[1] - s + c, center[2] );
	v[0].st[0] = 0; v[0].st[1] = 0;
	v[1].st[0] = 1; v[1].st[1] = 0;
	v[2].st[0] = 1; v[2].st[1] = 1;
	v[3].st[0] = 0; v[3].st[1] = 1;
	for ( i = 0 ; i < 4 ; i++ ) {
		v[i].modulate[0] = r;
		v[i].modulate[1] = g;
		v[i].modulate[2] = b;
		v[i].modulate[3] = 255;
	}
	trap_R_AddPolyToScene( cgs.media.ringShader, 4, v );
}

/*
==================
CG_AbilityVisual

How the champions' ability entities look: generated sprites and rings
(misc/art/make-abilities.py), lines, and models every install has
==================
*/
// light from an ability effect, unless turned off (cg_abilityLights)
static void CG_AbilityLight( const vec3_t origin, float intensity, float r, float g, float b ) {
	if ( cg_abilityLights.integer ) {
		trap_R_AddLightToScene( origin, intensity, r, g, b );
	}
}

/*
===============
Plasma Trail ribbon

Every trail point this frame, then quads 12 units wide lying on the floor
between each owner's neighbouring points
===============
*/
#define MAX_TRAIL_POINTS	256
#define TRAIL_JOIN_DIST		120		// points further apart are separate runs

typedef struct {
	vec3_t	origin;
	int		owner, number;
	qboolean	blue;
} trailPoint_t;

static trailPoint_t	trailPoints[MAX_TRAIL_POINTS];
static int			trailPointCount;

static void CG_AddTrailRibbons( void ) {
	trailPoint_t	*p, *q, tmp;
	polyVert_t		v[4];
	vec3_t			dir, side, up = { 0, 0, 1 };
	int				i, j, k;
	float			u0, u1;

	// owner, then the order they were dropped (entity numbers are reused, so
	// sort by distance along the run instead: nearest neighbour chains)
	for ( i = 1; i < trailPointCount; i++ ) {
		for ( j = i; j > 0 && trailPoints[j - 1].owner > trailPoints[j].owner; j-- ) {
			tmp = trailPoints[j]; trailPoints[j] = trailPoints[j - 1]; trailPoints[j - 1] = tmp;
		}
	}
	for ( i = 0; i < trailPointCount; i++ ) {
		p = &trailPoints[i];
		// join to the nearest later point of the same owner
		q = NULL;
		for ( j = i + 1; j < trailPointCount && trailPoints[j].owner == p->owner; j++ ) {
			if ( Distance( trailPoints[j].origin, p->origin ) <= TRAIL_JOIN_DIST
				&& ( !q || Distance( trailPoints[j].origin, p->origin ) < Distance( q->origin, p->origin ) ) ) {
				q = &trailPoints[j];
			}
		}
		if ( !q ) {
			continue;
		}
		VectorSubtract( q->origin, p->origin, dir );
		CrossProduct( dir, up, side );
		if ( VectorNormalize( side ) == 0 ) {
			continue;
		}
		VectorScale( side, 6, side );
		u0 = ( p->number % 8 ) * 0.125f;
		u1 = u0 + Distance( q->origin, p->origin ) / 128.0f;
		VectorAdd( p->origin, side, v[0].xyz );
		VectorSubtract( p->origin, side, v[1].xyz );
		VectorSubtract( q->origin, side, v[2].xyz );
		VectorAdd( q->origin, side, v[3].xyz );
		v[0].st[0] = u0; v[0].st[1] = 0;
		v[1].st[0] = u0; v[1].st[1] = 1;
		v[2].st[0] = u1; v[2].st[1] = 1;
		v[3].st[0] = u1; v[3].st[1] = 0;
		{
			byte		rgba[4];
			vmCvar_t	*custom = p->owner == cg.snap->ps.clientNum ? &cg_trailColor
				: ( cgs.gametype < GT_TEAM || cgs.clientinfo[p->owner & ( MAX_CLIENTS - 1 )].team != cg.snap->ps.persistant[PERS_TEAM] )
				? &cg_trailEnemyColor : NULL;

			// a chosen colour, else the team's
			if ( custom && custom->string[0] ) {
				CG_ParseColor( custom, "0xFF40C0FF", rgba );
			} else {
				rgba[0] = p->blue ? 80 : 255;
				rgba[1] = p->blue ? 160 : 200;
				rgba[2] = 255;
			}
			for ( k = 0; k < 4; k++ ) {
				v[k].xyz[2] -= 14;		// down on the floor, the points float at the shins
				v[k].modulate[0] = rgba[0];
				v[k].modulate[1] = rgba[1];
				v[k].modulate[2] = rgba[2];
				v[k].modulate[3] = 255;
			}
		}
		trap_R_AddPolyToScene( cgs.media.abilityTrailShader, 4, v );
	}
	trailPointCount = 0;
}

static void CG_AbilityVisual( centity_t *cent ) {
	entityState_t	*s1 = &cent->currentState;
	refEntity_t		ent;
	vec3_t			a, b, dir;
	int				i, t;

	switch ( s1->generic1 ) {
	case AV_SHOCKZONE:
		// the zone's edge and a ripple moving out
		VectorCopy( cent->lerpOrigin, a );
		a[2] -= 20;
		CG_VfxGroundRing( a, 260, cg.time * 0.01f, 140, 120, 200 );
		t = cg.time % 1500;
		CG_VfxGroundRing( a, 260 * t / 1500.0f, 0, 90 * ( 1500 - t ) / 1500, 80 * ( 1500 - t ) / 1500, 140 * ( 1500 - t ) / 1500 );
		break;

	case AV_SPINE:
		BG_EvaluateTrajectoryDelta( &s1->pos, cg.time, dir );
		VectorNormalize( dir );
		VectorMA( cent->lerpOrigin, -40, dir, b );
		CG_VfxLine( cent->lerpOrigin, b, 255, 230, 60, 255 );
		break;

	case AV_SUPPLYDROP:
		memset( &ent, 0, sizeof( ent ) );
		VectorCopy( cent->lerpOrigin, ent.origin );
		VectorCopy( cent->lerpOrigin, ent.oldorigin );
		ent.hModel = cgs.media.vfxCrateModel;
		AxisClear( ent.axis );
		for ( i = 0 ; i < 3 ; i++ ) {
			VectorScale( ent.axis[i], 2.0f, ent.axis[i] );
		}
		ent.nonNormalizedAxes = qtrue;
		trap_R_AddRefEntityToScene( &ent );
		if ( ( cg.time / 400 ) & 1 ) {
			CG_AbilityLight( cent->lerpOrigin, 120, 0.2f, 1.0f, 0.2f );
		}
		break;

	case AV_STORM:
		// a dark disc high up and blue streaks falling inside 160 units
		VectorCopy( cent->lerpOrigin, a );
		a[2] += 256;
		CG_VfxCircle( a, 160, 32, 0, 60, 60, 80 );
		CG_VfxCircle( a, 100, 24, 0, 60, 60, 80 );
		for ( i = 0 ; i < 24 ; i++ ) {
			float	ang = i * 2.39996f;
			float	rad = 150 * ( ( i * 37 ) % 100 ) / 100.0f;

			t = ( cg.time + i * 137 ) % 600;
			VectorSet( a, cent->lerpOrigin[0] + cos( ang ) * rad, cent->lerpOrigin[1] + sin( ang ) * rad,
				cent->lerpOrigin[2] + 256 - t * 256 / 600 );
			VectorCopy( a, b );
			b[2] -= 40;
			CG_VfxLine( a, b, 80, 140, 255, 255 );
		}
		break;

	case AV_HOUND:
		memset( &ent, 0, sizeof( ent ) );
		VectorCopy( cent->lerpOrigin, ent.origin );
		ent.origin[2] += 8;
		VectorCopy( ent.origin, ent.oldorigin );
		ent.hModel = cgs.media.vfxHoundModel;
		BG_EvaluateTrajectoryDelta( &s1->pos, cg.time, dir );
		dir[2] = 0;
		if ( VectorNormalize( dir ) == 0 ) {
			VectorCopy( s1->angles, a );
			AnglesToAxis( a, ent.axis );
		} else {
			VectorCopy( dir, ent.axis[0] );
			VectorSet( ent.axis[2], 0, 0, 1 );
			CrossProduct( ent.axis[2], ent.axis[0], ent.axis[1] );
		}
		for ( i = 0 ; i < 3 ; i++ ) {
			VectorScale( ent.axis[i], 1.4f, ent.axis[i] );
		}
		ent.nonNormalizedAxes = qtrue;
		trap_R_AddRefEntityToScene( &ent );
		break;

	case AV_ANCHOR:
		// two purple rings turning opposite ways on the floor
		VectorCopy( cent->lerpOrigin, a );
		a[2] -= 22;
		CG_VfxGroundRing( a, 28, cg.time * 0.12f, 170, 60, 220 );
		CG_VfxGroundRing( a, 20, -cg.time * 0.2f, 200, 120, 255 );
		break;

	case AV_ORB:
		if ( cgs.media.abilityOrbModel ) {
			// a swirling sphere that grows brighter as its blast ramps up over 1.5 s
			float	ramp = ( cg.time - s1->pos.trTime ) / 1500.0f;
			float	k;

			if ( ramp > 1 ) {
				ramp = 1;
			}
			k = 0.45f + 0.55f * ramp + 0.12f * sin( cg.time * 0.012f );
			memset( &ent, 0, sizeof( ent ) );
			VectorCopy( cent->lerpOrigin, ent.origin );
			VectorCopy( cent->lerpOrigin, ent.oldorigin );
			ent.hModel = cgs.media.abilityOrbModel;
			VectorSet( a, cg.time * 0.09f, cg.time * 0.05f, 0 );
			AnglesToAxis( a, ent.axis );
			ent.shaderRGBA[0] = 255 * ( k > 1 ? 1 : k );
			ent.shaderRGBA[1] = 200 * ( k > 1 ? 1 : k );
			ent.shaderRGBA[2] = 255 * ( k > 1 ? 1 : k );
			ent.shaderRGBA[3] = 255;
			ent.renderfx = RF_NOSHADOW;
			trap_R_AddRefEntityToScene( &ent );
			CG_VfxSprite( cent->lerpOrigin, 22 + 8 * ramp, cgs.media.glowBallShader, 90 * k, 30 * k, 160 * k, 0 );
			CG_AbilityLight( cent->lerpOrigin, 120 + 80 * ramp, 0.55f, 0.15f, 0.9f );
			// a dim violet wake while it flies
			if ( s1->pos.trType != TR_STATIONARY && cg.time - cent->trailTime > 40 ) {
				static const vec3_t	wake = { 0.3f, 0.08f, 0.45f };

				cent->trailTime = cg.time;
				CG_VfxWake( cent->lerpOrigin, 12, 600, wake );
			}
		} else {
			CG_VfxSprite( cent->lerpOrigin, 22, cgs.media.orbShader, 255, 255, 255, 0 );
			CG_AbilityLight( cent->lerpOrigin, 160, 0.83f, 0.1f, 0.12f );
		}
		break;

	case AV_GROUNDFIRE:
		// low flames on the floor, flickering
		{
			vec3_t	p;

			VectorCopy( cent->lerpOrigin, p );
			p[2] += 10;
			CG_VfxSprite( p, 16 + 4 * sin( cg.time * 0.02f + s1->number ), cgs.media.glowBallShader, 255, 110, 20, cg.time * 0.1f );
			p[2] += 10;
			CG_VfxSprite( p, 10, cgs.media.glowBallShader, 255, 200, 80, 0 );
			CG_AbilityLight( p, 90, 1.0f, 0.45f, 0.1f );
		}
		break;

	case AV_FIREBALL:
		CG_VfxSprite( cent->lerpOrigin, 14, cgs.media.glowBallShader, 255, 140, 40, 0 );
		CG_VfxSprite( cent->lerpOrigin, 28, cgs.media.glowBallShader, 160, 50, 10, cg.time * 0.2f );
		CG_AbilityLight( cent->lerpOrigin, 140, 1.0f, 0.5f, 0.15f );
		break;

	case AV_THROWN:
		memset( &ent, 0, sizeof( ent ) );
		VectorCopy( cent->lerpOrigin, ent.origin );
		VectorCopy( cent->lerpOrigin, ent.oldorigin );
		ent.hModel = cgs.gameModels[s1->modelindex];
		VectorSet( a, cg.time * 0.4f, s1->angles[YAW], 0 );
		AnglesToAxis( a, ent.axis );
		ent.renderfx = RF_NOSHADOW;
		trap_R_AddRefEntityToScene( &ent );
		break;

	case AV_ACID:
		if ( cgs.media.acidBallShader ) {
			CG_VfxSprite( cent->lerpOrigin, 12, cgs.media.acidBallShader, 255, 255, 255, cg.time * 0.3f );
		} else {
			CG_VfxSprite( cent->lerpOrigin, 10, cgs.media.glowBallShader, 120, 255, 60, 0 );
		}
		CG_VfxSprite( cent->lerpOrigin, 18, cgs.media.glowBallShader, 40, 120, 20, 0 );
		break;

	case AV_TRAIL:
		// the owner's team colour, or the logo red
		t = s1->otherEntityNum >= 0 && s1->otherEntityNum < MAX_CLIENTS ? cgs.clientinfo[s1->otherEntityNum].team : TEAM_FREE;
		VectorCopy( cent->lerpOrigin, a );
		if ( cgs.media.abilityTrailShader && trailPointCount < MAX_TRAIL_POINTS ) {
			// a ribbon joins the points once every entity is in (CG_AddTrailRibbons)
			trailPoint_t	*tp = &trailPoints[trailPointCount++];

			VectorCopy( a, tp->origin );
			tp->owner = s1->otherEntityNum;
			tp->number = s1->number;
			tp->blue = t == TEAM_BLUE && cgs.gametype >= GT_TEAM;
			CG_VfxSprite( a, 7, cgs.media.glowBallShader, tp->blue ? 40 : 200, tp->blue ? 110 : 60, 220, 0 );
		} else if ( t == TEAM_BLUE && cgs.gametype >= GT_TEAM ) {
			CG_VfxSprite( a, 18, cgs.media.glowBallShader, 40, 90, 220, 0 );
		} else {
			CG_VfxSprite( a, 18, cgs.media.glowBallShader, 211, 25, 31, 0 );
		}
		break;
	}
}

static void CG_Missile( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	const weaponInfo_t		*weapon;
//	int	col;

	s1 = &cent->currentState;
	if ( s1->generic1 > AV_NONE && s1->generic1 < AV_NUM ) {
		CG_AbilityVisual( cent );
		return;
	}
	if ( s1->weapon >= WP_NUM_WEAPONS ) {
		s1->weapon = 0;
	}
	weapon = &cg_weapons[s1->weapon];

	// calculate the axis
	VectorCopy( s1->angles, cent->lerpAngles);

	// add trails
	if ( weapon->missileTrailFunc ) 
	{
		weapon->missileTrailFunc( cent, weapon );
	}
/*
	if ( cent->currentState.modelindex == TEAM_RED ) {
		col = 1;
	}
	else if ( cent->currentState.modelindex == TEAM_BLUE ) {
		col = 2;
	}
	else {
		col = 0;
	}

	// add dynamic light
	if ( weapon->missileDlight ) {
		trap_R_AddLightToScene(cent->lerpOrigin, weapon->missileDlight, 
			weapon->missileDlightColor[col][0], weapon->missileDlightColor[col][1], weapon->missileDlightColor[col][2] );
	}
*/
	// add dynamic light
	if ( weapon->missileDlight ) {
		trap_R_AddLightToScene(cent->lerpOrigin, weapon->missileDlight, 
			weapon->missileDlightColor[0], weapon->missileDlightColor[1], weapon->missileDlightColor[2] );
	}

	// add missile sound
	if ( weapon->missileSound ) {
		vec3_t	velocity;

		BG_EvaluateTrajectoryDelta( &cent->currentState.pos, cg.time, velocity );

		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, velocity, weapon->missileSound );
	}

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	if ( cent->currentState.weapon == WP_PLASMAGUN ) {
		ent.reType = RT_SPRITE;
		ent.radius = 16;
		ent.rotation = 0;
		ent.customShader = cgs.media.plasmaBallShader;
		trap_R_AddRefEntityToScene( &ent );
		return;
	}

	// flicker between two skins
	ent.skinNum = cg.clientFrame & 1;
	ent.hModel = cgs.media.abilityClawModel ? cgs.media.abilityClawModel : weapon->missileModel;
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

#ifdef MISSIONPACK
	if ( cent->currentState.weapon == WP_PROX_LAUNCHER ) {
		if (s1->generic1 == TEAM_BLUE) {
			ent.hModel = cgs.media.blueProxMine;
		}
	}
#endif

	// convert direction of travel into axis
	if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
		ent.axis[0][2] = 1;
	}

	// spin as it moves
	if ( s1->pos.trType != TR_STATIONARY ) {
		RotateAroundDirection( ent.axis, cg.time / 4 );
	} else {
#ifdef MISSIONPACK
		if ( s1->weapon == WP_PROX_LAUNCHER ) {
			AnglesToAxis( cent->lerpAngles, ent.axis );
		}
		else
#endif
		{
			RotateAroundDirection( ent.axis, s1->time );
		}
	}

	// add to refresh list, possibly with quad glow
	CG_ColorReset();
	CG_AddRefEntityWithPowerups( &ent, s1, TEAM_FREE );
	if ( s1->weapon == WP_GRENADE_LAUNCHER ) {
		CG_ColorGrenade( &ent );
	}
}

/*
===============
CG_DrawMissile

Draws a projectile that is not a snapshot entity: our own, predicted
===============
*/
void CG_DrawMissile( centity_t *cent ) {
	CG_Missile( cent );
}

/*
===============
CG_Grapple

This is called when the grapple is sitting up against the wall
===============
*/
static void CG_Grapple( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	const weaponInfo_t		*weapon;

	s1 = &cent->currentState;
	if ( s1->weapon >= WP_NUM_WEAPONS ) {
		s1->weapon = 0;
	}
	weapon = &cg_weapons[s1->weapon];

	// calculate the axis
	VectorCopy( s1->angles, cent->lerpAngles);

#if 0 // FIXME add grapple pull sound here..?
	// add missile sound
	if ( weapon->missileSound ) {
		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->missileSound );
	}
#endif

	// Will draw cable if needed
	CG_GrappleTrail ( cent, weapon );

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	// flicker between two skins
	ent.skinNum = cg.clientFrame & 1;
	ent.hModel = weapon->missileModel;
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

	// convert direction of travel into axis
	if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
		ent.axis[0][2] = 1;
	}

	trap_R_AddRefEntityToScene( &ent );
}

/*
===============
CG_Mover
===============
*/
static void CG_Mover( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);
	AnglesToAxis( cent->lerpAngles, ent.axis );

	ent.renderfx = RF_NOSHADOW;

	// flicker between two skins (FIXME?)
	ent.skinNum = ( cg.time >> 6 ) & 1;

	// get the model, either as a bmodel or a modelindex
	if ( s1->solid == SOLID_BMODEL ) {
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];
	} else {
		ent.hModel = cgs.gameModels[s1->modelindex];
	}

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);

	// add the secondary model
	if ( s1->modelindex2 ) {
		ent.skinNum = 0;
		ent.hModel = cgs.gameModels[s1->modelindex2];
		trap_R_AddRefEntityToScene(&ent);
	}

}

/*
===============
CG_Beam

Also called as an event
===============
*/
void CG_Beam( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( s1->pos.trBase, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	AxisClear( ent.axis );
	ent.reType = RT_BEAM;

	ent.renderfx = RF_NOSHADOW;

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}


/*
===============
CG_Portal
===============
*/
static void CG_Portal( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	ByteToDir( s1->eventParm, ent.axis[0] );
	PerpendicularVector( ent.axis[1], ent.axis[0] );

	// negating this tends to get the directions like they want
	// we really should have a camera roll value
	VectorSubtract( vec3_origin, ent.axis[1], ent.axis[1] );

	CrossProduct( ent.axis[0], ent.axis[1], ent.axis[2] );
	ent.reType = RT_PORTALSURFACE;
	ent.oldframe = s1->powerups;
	ent.frame = s1->frame;		// rotation speed
	ent.skinNum = s1->clientNum/256.0 * 360;	// roll offset

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}


/*
================
CG_CreateRotationMatrix
================
*/
void CG_CreateRotationMatrix(vec3_t angles, vec3_t matrix[3]) {
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/*
================
CG_TransposeMatrix
================
*/
void CG_TransposeMatrix(vec3_t matrix[3], vec3_t transpose[3]) {
	int i, j;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			transpose[i][j] = matrix[j][i];
		}
	}
}

/*
================
CG_RotatePoint
================
*/
void CG_RotatePoint(vec3_t point, vec3_t matrix[3]) {
	vec3_t tvec;

	VectorCopy(point, tvec);
	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
}

/*
=========================
CG_AdjustPositionForMover

Also called by client movement prediction code
=========================
*/
void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t angles_in, vec3_t angles_out) {
	centity_t	*cent;
	vec3_t	oldOrigin, origin, deltaOrigin;
	vec3_t	oldAngles, angles, deltaAngles;
	vec3_t	matrix[3], transpose[3];
	vec3_t	org, org2, move2;

	if ( moverNum <= 0 || moverNum >= ENTITYNUM_MAX_NORMAL ) {
		VectorCopy( in, out );
		VectorCopy(angles_in, angles_out);
		return;
	}

	cent = &cg_entities[ moverNum ];
	if ( cent->currentState.eType != ET_MOVER ) {
		VectorCopy( in, out );
		VectorCopy(angles_in, angles_out);
		return;
	}

	BG_EvaluateTrajectory( &cent->currentState.pos, fromTime, oldOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, fromTime, oldAngles );

	BG_EvaluateTrajectory( &cent->currentState.pos, toTime, origin );
	BG_EvaluateTrajectory( &cent->currentState.apos, toTime, angles );

	VectorSubtract( origin, oldOrigin, deltaOrigin );
	VectorSubtract( angles, oldAngles, deltaAngles );

	// origin change when on a rotating object
	CG_CreateRotationMatrix( deltaAngles, transpose );
	CG_TransposeMatrix( transpose, matrix );
	VectorSubtract( in, oldOrigin, org );
	VectorCopy( org, org2 );
	CG_RotatePoint( org2, matrix );
	VectorSubtract( org2, org, move2 );
	VectorAdd( deltaOrigin, move2, deltaOrigin );

	VectorAdd( in, deltaOrigin, out );
	VectorAdd( angles_in, deltaAngles, angles_out );
}


/*
=============================
CG_InterpolateEntityPosition
=============================
*/
static void CG_InterpolateEntityPosition( centity_t *cent ) {
	vec3_t		current, next;
	float		f;

	// it would be an internal error to find an entity that interpolates without
	// a snapshot ahead of the current one
	if ( cg.nextSnap == NULL ) {
		CG_Error( "CG_InterpoateEntityPosition: cg.nextSnap == NULL" );
	}

	f = cg.frameInterpolation;

	// this will linearize a sine or parabolic curve, but it is important
	// to not extrapolate player positions if more recent data is available
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState.pos, cg.nextSnap->serverTime, next );

	cent->lerpOrigin[0] = current[0] + f * ( next[0] - current[0] );
	cent->lerpOrigin[1] = current[1] + f * ( next[1] - current[1] );
	cent->lerpOrigin[2] = current[2] + f * ( next[2] - current[2] );

	BG_EvaluateTrajectory( &cent->currentState.apos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState.apos, cg.nextSnap->serverTime, next );

	cent->lerpAngles[0] = LerpAngle( current[0], next[0], f );
	cent->lerpAngles[1] = LerpAngle( current[1], next[1], f );
	cent->lerpAngles[2] = LerpAngle( current[2], next[2], f );

}

/*
===============
CG_ExtrapolatePlayer

Moves a player on along its last velocity, for at most two server ticks
===============
*/
#define	EXTRAPOLATE_MSEC	32

static void CG_ExtrapolatePlayer( centity_t *cent ) {
	float	dt;

	dt = cg.time - cg.snap->serverTime;
	if ( dt > EXTRAPOLATE_MSEC ) {
		dt = EXTRAPOLATE_MSEC;
	} else if ( dt < 0 ) {
		dt = 0;
	}
	VectorMA( cent->currentState.pos.trBase, dt * 0.001f, cent->currentState.pos.trDelta, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );
}

/*
===============
CG_NudgedMissilePosition

Other players' straight-flying projectiles are drawn ahead by the nudge
(your ping when automatic, at most 80 ms) so they show where the server
has them; never through a wall. Bouncing, falling and hook projectiles
are not nudged.
===============
*/
static void CG_NudgedMissilePosition( centity_t *cent ) {
	trace_t	tr;
	vec3_t	nudged;
	int		nudge;

	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );

	// our own projectile, just taken over from its predicted copy
	if ( cent->ownMissile == OWNMISSILE_MATCHED && cent->ownBlendTime ) {
		float	f = 1.0f - (float)( cg.time - cent->ownBlendTime ) / OWNMISSILE_BLEND_MSEC;

		if ( f > 0 && f <= 1 ) {
			VectorMA( cent->lerpOrigin, f, cent->ownBlendOffset, cent->lerpOrigin );
		} else {
			cent->ownBlendTime = 0;
		}
	}

	if ( cent->currentState.pos.trType != TR_LINEAR || cent->currentState.weapon == WP_GRAPPLING_HOOK
		|| cent->currentState.otherEntityNum2 == cg.snap->ps.clientNum ) {
		return;
	}
	if ( cg_autoProjectileNudge.integer ) {
		nudge = cg.snap->ping;
		if ( nudge > 80 ) {
			nudge = 80;
		}
	} else {
		nudge = cg_projectileNudge.integer;
	}
	if ( nudge <= 0 ) {
		return;
	}

	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time + nudge, nudged );
	CG_Trace( &tr, cent->lerpOrigin, vec3_origin, vec3_origin, nudged, cent->currentState.number, MASK_SOLID );
	VectorCopy( tr.endpos, cent->lerpOrigin );
}

/*
===============
CG_CalcEntityLerpPositions

===============
*/
static void CG_CalcEntityLerpPositions( centity_t *cent ) {

	// if this player does not want to see extrapolated players
	if ( !cg_smoothClients.integer ) {
		// make sure the clients use TR_INTERPOLATE
		if ( cent->currentState.number < MAX_CLIENTS ) {
			cent->currentState.pos.trType = TR_INTERPOLATE;
			cent->nextState.pos.trType = TR_INTERPOLATE;
		}
	}

	if ( cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE ) {
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// no newer snapshot: carry other players on for at most two ticks, then hold
	if ( !cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE
		&& cent->currentState.number < MAX_CLIENTS && cent != &cg.predictedPlayerEntity ) {
		CG_ExtrapolatePlayer( cent );
		return;
	}

	// other players' straight projectiles are drawn where the server has them
	if ( cent->currentState.eType == ET_MISSILE ) {
		CG_NudgedMissilePosition( cent );
		return;
	}

	// first see if we can interpolate between two snaps for
	// linear extrapolated clients
	if ( cent->interpolate && cent->currentState.pos.trType == TR_LINEAR_STOP &&
											cent->currentState.number < MAX_CLIENTS) {
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// just use the current frame and evaluate as best we can
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );

	// adjust for riding a mover if it wasn't rolled into the predicted
	// player state
	if ( cent != &cg.predictedPlayerEntity ) {
		CG_AdjustPositionForMover( cent->lerpOrigin, cent->currentState.groundEntityNum, 
		cg.snap->serverTime, cg.time, cent->lerpOrigin, cent->lerpAngles, cent->lerpAngles);
	}
}

/*
===============
CG_TeamBase
===============
*/
static void CG_TeamBase( centity_t *cent ) {
	refEntity_t model;
#ifdef MISSIONPACK
	vec3_t angles;
	int t, h;
	float c;

	if ( cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF ) {
#else
	if ( cgs.gametype == GT_CTF) {
#endif
		// show the flag base
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );
		if ( cent->currentState.modelindex == TEAM_RED ) {
			model.hModel = cgs.media.redFlagBaseModel;
		}
		else if ( cent->currentState.modelindex == TEAM_BLUE ) {
			model.hModel = cgs.media.blueFlagBaseModel;
		}
		else {
			model.hModel = cgs.media.neutralFlagBaseModel;
		}
		trap_R_AddRefEntityToScene( &model );
	}
#ifdef MISSIONPACK
	else if ( cgs.gametype == GT_OBELISK ) {
		// show the obelisk
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		model.hModel = cgs.media.overloadBaseModel;
		trap_R_AddRefEntityToScene( &model );
		// if hit
		if ( cent->currentState.frame == 1) {
			// show hit model
			// modelindex2 is the health value of the obelisk
			c = cent->currentState.modelindex2;
			model.shaderRGBA[0] = 0xff;
			model.shaderRGBA[1] = c;
			model.shaderRGBA[2] = c;
			model.shaderRGBA[3] = 0xff;
			//
			model.hModel = cgs.media.overloadEnergyModel;
			trap_R_AddRefEntityToScene( &model );
		}
		// if respawning
		if ( cent->currentState.frame == 2) {
			if ( !cent->miscTime ) {
				cent->miscTime = cg.time;
			}
			t = cg.time - cent->miscTime;
			h = (cg_obeliskRespawnDelay.integer - 5) * 1000;
			//
			if (t > h) {
				c = (float) (t - h) / h;
				if (c > 1)
					c = 1;
			}
			else {
				c = 0;
			}
			// show the lights
			AnglesToAxis( cent->currentState.angles, model.axis );
			//
			model.shaderRGBA[0] = c * 0xff;
			model.shaderRGBA[1] = c * 0xff;
			model.shaderRGBA[2] = c * 0xff;
			model.shaderRGBA[3] = c * 0xff;

			model.hModel = cgs.media.overloadLightsModel;
			trap_R_AddRefEntityToScene( &model );
			// show the target
			if (t > h) {
				if ( !cent->muzzleFlashTime ) {
					trap_S_StartSound (cent->lerpOrigin, ENTITYNUM_NONE, CHAN_BODY,  cgs.media.obeliskRespawnSound);
					cent->muzzleFlashTime = 1;
				}
				VectorCopy(cent->currentState.angles, angles);
				angles[YAW] += (float) 16 * acos(1-c) * 180 / M_PI;
				AnglesToAxis( angles, model.axis );

				VectorScale( model.axis[0], c, model.axis[0]);
				VectorScale( model.axis[1], c, model.axis[1]);
				VectorScale( model.axis[2], c, model.axis[2]);

				model.shaderRGBA[0] = 0xff;
				model.shaderRGBA[1] = 0xff;
				model.shaderRGBA[2] = 0xff;
				model.shaderRGBA[3] = 0xff;
				//
				model.origin[2] += 56;
				model.hModel = cgs.media.overloadTargetModel;
				trap_R_AddRefEntityToScene( &model );
			}
			else {
				//FIXME: show animated smoke
			}
		}
		else {
			cent->miscTime = 0;
			cent->muzzleFlashTime = 0;
			// modelindex2 is the health value of the obelisk
			c = cent->currentState.modelindex2;
			model.shaderRGBA[0] = 0xff;
			model.shaderRGBA[1] = c;
			model.shaderRGBA[2] = c;
			model.shaderRGBA[3] = 0xff;
			// show the lights
			model.hModel = cgs.media.overloadLightsModel;
			trap_R_AddRefEntityToScene( &model );
			// show the target
			model.origin[2] += 56;
			model.hModel = cgs.media.overloadTargetModel;
			trap_R_AddRefEntityToScene( &model );
		}
	}
	else if ( cgs.gametype == GT_HARVESTER ) {
		// show harvester model
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		if ( cent->currentState.modelindex == TEAM_RED ) {
			model.hModel = cgs.media.harvesterModel;
			model.customSkin = cgs.media.harvesterRedSkin;
		}
		else if ( cent->currentState.modelindex == TEAM_BLUE ) {
			model.hModel = cgs.media.harvesterModel;
			model.customSkin = cgs.media.harvesterBlueSkin;
		}
		else {
			model.hModel = cgs.media.harvesterNeutralModel;
			model.customSkin = 0;
		}
		trap_R_AddRefEntityToScene( &model );
	}
#endif
}

/*
===============
CG_AddCEntity

===============
*/
static void CG_AddCEntity( centity_t *cent ) {
	// event-only entities will have been dealt with already
	if ( cent->currentState.eType >= ET_EVENTS ) {
		return;
	}

	// calculate the current origin
	CG_CalcEntityLerpPositions( cent );

	// add automatic effects
	CG_EntityEffects( cent );

	switch ( cent->currentState.eType ) {
	default:
		CG_Error( "Bad entity type: %i", cent->currentState.eType );
		break;
	case ET_INVISIBLE:
	case ET_PUSH_TRIGGER:
	case ET_TELEPORT_TRIGGER:
		break;
	case ET_GENERAL:
		CG_General( cent );
		break;
	case ET_PLAYER:
		CG_Player( cent );
		break;
	case ET_ITEM:
		CG_Item( cent );
		break;
	case ET_MISSILE:
		CG_Missile( cent );
		break;
	case ET_MOVER:
		CG_Mover( cent );
		break;
	case ET_BEAM:
		CG_Beam( cent );
		break;
	case ET_PORTAL:
		CG_Portal( cent );
		break;
	case ET_SPEAKER:
		CG_Speaker( cent );
		break;
	case ET_GRAPPLE:
		CG_Grapple( cent );
		break;
	case ET_TEAM:
		CG_TeamBase( cent );
		break;
	}
}

/*
===============
CG_AddWorldItemTimers

hud.md 9.4: a major item that is away shows a pie over its spot, in the
world, so walls hide it and it shrinks with distance: one slice per 5 s of
its cycle, a slice going every 5 s, the going one blinking; the item's
icon in the middle; fading in and out over 0.29 s; not past 2000 u.
Mega, heavy and light armour, Quad and Protection. Spectators always see
them, players too unless the mode is duel (cg_worldItemTimers 0: off)
===============
*/
#define PIE_RADIUS		14
#define PIE_ICON		8
#define PIE_SLICE_MSEC	5000
#define PIE_FADE_MSEC	290
#define PIE_RANGE		2000

static void CG_PieSlice( const vec3_t centre, float a0, float a1, const byte rgba[4] ) {
	polyVert_t	verts[3];
	int			i, k;
	float		a[2];

	a[0] = a0;
	a[1] = a1;
	VectorCopy( centre, verts[0].xyz );
	for ( k = 0 ; k < 2 ; k++ ) {
		VectorCopy( centre, verts[k + 1].xyz );
		VectorMA( verts[k + 1].xyz, -PIE_RADIUS * sin( a[k] ), cg.refdef.viewaxis[1], verts[k + 1].xyz );		// (clockwise: axis 1 is left)
		VectorMA( verts[k + 1].xyz, PIE_RADIUS * cos( a[k] ), cg.refdef.viewaxis[2], verts[k + 1].xyz );
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		verts[i].st[0] = verts[i].st[1] = 0.5f;
		memcpy( verts[i].modulate, rgba, 4 );
	}
	trap_R_AddPolyToScene( cgs.media.pieShader, 3, verts );
}

void CG_AddWorldItemTimers( void ) {
	char		buf[MAX_STRING_CHARS], *p, *token;
	int			item, time, cycle, left, slices, shown, i, cls, sub;
	vec3_t		spot, d;
	byte		rgba[4];
	float		fade, step, a;
	qboolean	watching = cg.demoPlayback || ( cg.snap->ps.pm_flags & PMF_FOLLOW )
		|| cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR;

	if ( !cg_worldItemTimers.integer || ( !watching && cgs.gametype == GT_TOURNAMENT ) || !cgs.media.pieShader ) {
		return;
	}
	Q_strncpyz( buf, CG_ConfigString( CS_ITEM_TIMERS ), sizeof( buf ) );
	p = buf;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		item = atoi( token );
		time = atoi( COM_Parse( &p ) );
		spot[0] = atoi( COM_Parse( &p ) );
		spot[1] = atoi( COM_Parse( &p ) );
		spot[2] = atoi( COM_Parse( &p ) ) + 20;
		cycle = atoi( COM_Parse( &p ) );
		if ( item <= 0 || item >= bg_numItems || !time || cycle < PIE_SLICE_MSEC ) {
			continue;
		}
		cls = BG_ItemTimerClass( &bg_itemlist[item] );
		// of the power-ups, Quad and Protection only (hud.md 9.4)
		if ( cls == 1 && bg_itemlist[item].giTag != PW_QUAD && bg_itemlist[item].giTag != PW_BATTLESUIT ) {
			continue;
		}
		left = time - cg.time;
		VectorSubtract( spot, cg.refdef.vieworg, d );
		if ( !cls || left <= -PIE_FADE_MSEC || VectorLength( d ) > PIE_RANGE ) {
			continue;
		}
		// fading in as it's taken, out as it comes back
		fade = 1.0f;
		if ( cycle - left < PIE_FADE_MSEC ) {
			fade = ( cycle - left ) / (float)PIE_FADE_MSEC;
		}
		if ( left < 0 ) {
			fade = 1.0f + left / (float)PIE_FADE_MSEC;
			left = 0;
		}
		if ( fade <= 0 ) {
			continue;
		}
		// the tint: red / yellow armour, blue mega, the power-up's colour
		switch ( cls ) {
		case 1:		rgba[0] = 200; rgba[1] = 90; rgba[2] = 255; break;
		case 2:		rgba[0] = 70; rgba[1] = 140; rgba[2] = 255; break;
		case 4:		rgba[0] = 255; rgba[1] = 50; rgba[2] = 40; break;
		default:	rgba[0] = 255; rgba[1] = 210; rgba[2] = 40; break;
		}
		slices = ( cycle + PIE_SLICE_MSEC - 1 ) / PIE_SLICE_MSEC;
		shown = ( left + PIE_SLICE_MSEC - 1 ) / PIE_SLICE_MSEC;
		step = 2 * M_PI / slices;
		for ( i = 0 ; i < shown && i < slices ; i++ ) {
			float	alpha = 0.75f;

			if ( i == shown - 1 && ( ( cg.time / 250 ) & 1 ) ) {
				alpha = 0.3f;		// the slice going next blinks
			}
			rgba[3] = (byte)( 255 * alpha * fade );
			// clockwise from the top, a few triangles a slice
			for ( sub = 0 ; sub < 3 ; sub++ ) {
				a = i * step + sub * step / 3;
				CG_PieSlice( spot, a, a + step / 3 - 0.02f, rgba );
			}
		}
		// the item's icon in the middle
		{
			refEntity_t	ent;

			CG_RegisterItemVisuals( item );
			memset( &ent, 0, sizeof( ent ) );
			ent.reType = RT_SPRITE;
			VectorMA( spot, -1, cg.refdef.viewaxis[0], ent.origin );		// just in front
			ent.radius = PIE_ICON;
			ent.customShader = cg_items[item].icon;
			ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 255;
			ent.shaderRGBA[3] = (byte)( 255 * fade );
			trap_R_AddRefEntityToScene( &ent );
		}
	}
}

/*
===============
CG_AddPacketEntities

===============
*/
void CG_AddPacketEntities( void ) {
	int					num;
	centity_t			*cent;
	playerState_t		*ps;

	// set cg.frameInterpolation
	if ( cg.nextSnap ) {
		int		delta;

		delta = (cg.nextSnap->serverTime - cg.snap->serverTime);
		if ( delta == 0 ) {
			cg.frameInterpolation = 0;
		} else {
			cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
		}
	} else {
		cg.frameInterpolation = 0;	// actually, it should never be used, because 
									// no entities should be marked as interpolating
	}

	// the auto-rotating items will all have the same axis
	cg.autoAngles[0] = 0;
	cg.autoAngles[1] = ( cg.time & 2047 ) * 360 / 2048.0;
	cg.autoAngles[2] = 0;

	cg.autoAnglesFast[0] = 0;
	cg.autoAnglesFast[1] = ( cg.time & 1023 ) * 360 / 1024.0f;
	cg.autoAnglesFast[2] = 0;

	AnglesToAxis( cg.autoAngles, cg.autoAxis );
	AnglesToAxis( cg.autoAnglesFast, cg.autoAxisFast );

	// generate and add the entity from the playerstate
	ps = &cg.predictedPlayerState;
	BG_PlayerStateToEntityState( ps, &cg.predictedPlayerEntity.currentState, qfalse );
	CG_AddCEntity( &cg.predictedPlayerEntity );

	// lerp the non-predicted value for lightning gun origins
	CG_CalcEntityLerpPositions( &cg_entities[ cg.snap->ps.clientNum ] );

	// hand our own predicted projectiles over to the server's copies
	CG_UpdateOwnMissiles();

	// add each entity sent over by the server
	for ( num = 0 ; num < cg.snap->numEntities ; num++ ) {
		cent = &cg_entities[ cg.snap->entities[ num ].number ];
		CG_AddCEntity( cent );
	}

	// and the predicted ones still waiting for them
	CG_AddOwnMissiles();

	CG_AddTrailRibbons();
	CG_AddFadingRopes();
}

