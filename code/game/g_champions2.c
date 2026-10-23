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
// g_champions2.c -- champions that never shipped: Wrack's Shell Shock,
// Klesk's spines, Sarge's supply drop, Hunter's mark, Xaero's eternal
// champion, Raine's storm, Beastmaster's hound and Unumrey's rift anchor,
// with their passives

#include "g_local.h"

#define CHAMP_FRAME			50
#define MOD_WINDUP_MSEC		500		// Master of Death: no fire for this long first

// Wrack: Shell Shock
vmCvar_t	g_shockRadius;
vmCvar_t	g_shockZoneTime;
vmCvar_t	g_shockTime;
vmCvar_t	g_shockReverseCap;

/*
=================
G_RegisterChampionCvars

Shell Shock's numbers, as settings
=================
*/
void G_RegisterChampionCvars( void ) {
	// the reference values: a 6.5 m zone for 8 s, shocked 8 s, up to 60 s of cooldown back
	trap_Cvar_Register( &g_shockRadius, "g_shockRadius", "260", 0 );
	trap_Cvar_Register( &g_shockZoneTime, "g_shockZoneTime", "8000", 0 );
	trap_Cvar_Register( &g_shockTime, "g_shockTime", "8000", 0 );
	trap_Cvar_Register( &g_shockReverseCap, "g_shockReverseCap", "60000", 0 );
}

void G_UpdateChampionCvars( void ) {
	trap_Cvar_Update( &g_shockRadius );
	trap_Cvar_Update( &g_shockZoneTime );
	trap_Cvar_Update( &g_shockTime );
	trap_Cvar_Update( &g_shockReverseCap );
}

static int G_Passives( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return 0;
	}
	return BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->passives;
}

/*
==============================================================================

WRACK: SHELL SHOCK

==============================================================================
*/

static void G_CancelAbility( gentity_t *ent );

/*
=================
G_BreakHook

The rope snaps: the hook lets go as if released
=================
*/
void G_BreakHook( gentity_t *ent ) {
	ent->client->hookDamageTaken = 0;
	G_CancelAbility( ent );
}

/*
=================
G_CancelAbility

Shell shock stops whatever active ability the player is using
=================
*/
static void G_CancelAbility( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	int			state = BG_AbilityState( &client->ps );
	int			active = BG_Champion( client->ps.stats[STAT_CHAMPION] )->active;

	if ( state == ABILITY_IDLE ) {
		return;
	}
	BG_SetAbility( &client->ps, ABILITY_IDLE, BG_AbilityCharges( &client->ps ) );
	client->ps.stats[STAT_ABILITY_TIME] = 0;
	client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
	if ( active == ACTIVE_HOOK ) {
		client->ps.stats[STAT_MOVE_STATE] = 0;		// (the swing's rope)
		if ( client->hook ) {
			Weapon_HookFree( client->hook );
		}
	}
	if ( active == ACTIVE_RIFTANCHOR && client->anchorEnt ) {
		G_FreeEntity( client->anchorEnt );
		client->anchorEnt = NULL;
	}
	if ( active == ACTIVE_BERSERK ) {
		client->ps.stats[STAT_MOVE_STATE] &= ~1023;	// a lunge in progress ends too
	}
	// a cancelled Bull Rush still gets its end lockout
	if ( state == ABILITY_RUSH && client->ps.weaponTime < BG_AbilityParam( active, AP_LOCKOUT ) ) {
		client->ps.weaponTime = BG_AbilityParam( active, AP_LOCKOUT );
	}
	if ( active == ACTIVE_BARRIER && client->barrierMode == 2 ) {
		client->ps.weaponTime = 0;		// the laser's weapon lock goes with it
	}
}

static void G_ShockZoneThink( gentity_t *zone ) {
	gentity_t	*other;
	int			i;

	if ( level.time >= zone->abilityTime || !zone->parent || !zone->parent->inuse ) {
		G_FreeEntity( zone );
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !G_AbilityEnemy( zone->parent, other )
			|| Distance( other->r.currentOrigin, zone->r.currentOrigin ) > g_shockRadius.value ) {
			continue;
		}
		// in sight of the zone's centre (champion-audit.md)
		{
			trace_t	los;

			trap_Trace( &los, zone->r.currentOrigin, NULL, NULL, other->r.currentOrigin, zone->s.number, MASK_SOLID );
			if ( los.fraction < 1.0f ) {
				continue;
			}
		}
		// a new shock gets a fresh allowance of cooldown to run back; a
		// ready ability is pushed back by it too
		if ( other->client->ps.powerups[PW_SHELLSHOCK] <= level.time ) {
			int	active = BG_Champion( other->client->ps.stats[STAT_CHAMPION] )->active;
			int	charges = BG_AbilityCharges( &other->client->ps );

			other->client->shockBudget = g_shockReverseCap.integer;
			if ( active > ACTIVE_NONE && charges > 0 && charges >= BG_AbilityParam( active, AP_CHARGES )
				&& BG_AbilityState( &other->client->ps ) == ABILITY_IDLE ) {
				int	recharge = BG_AbilityParam( active, AP_RECHARGE );

				BG_SetAbility( &other->client->ps, ABILITY_IDLE, charges - 1 );
				other->client->abilityRecharge = recharge - other->client->shockBudget > 0 ? recharge - other->client->shockBudget : 0;
				other->client->shockBudget = 0;
			}
		}
		other->client->ps.powerups[PW_SHELLSHOCK] = level.time + g_shockTime.integer;
		G_CancelAbility( other );
	}
	zone->nextthink = level.time + CHAMP_FRAME;
}

/*
=================
G_ShellShock

A zone where Wrack stands: enemies inside are shell-shocked
=================
*/
void G_ShellShock( gentity_t *ent ) {
	gentity_t	*zone;
	vec3_t		dir = { 0, 0, 1 };

	zone = G_AbilityMissile( ent, "shellshock", ent->client->ps.origin, dir, 0, TR_STATIONARY, WP_BFG );
	zone->s.generic1 = AV_SHOCKZONE;
	G_AbilityStop( zone, ent->client->ps.origin );
	zone->clipmask = 0;
	zone->abilityTime = level.time + g_shockZoneTime.integer;
	zone->think = G_ShockZoneThink;
	zone->nextthink = level.time + CHAMP_FRAME;
	trap_LinkEntity( zone );
}

/*
=================
G_ShellShockRecharge

While shell-shocked the cooldown runs backwards, up to the shock's
allowance; true when it took over the recharge this frame
=================
*/
qboolean G_ShellShockRecharge( gclient_t *client, int msec ) {
	if ( client->ps.powerups[PW_SHELLSHOCK] <= level.time ) {
		return qfalse;
	}
	if ( client->shockBudget > 0 ) {
		if ( msec > client->shockBudget ) {
			msec = client->shockBudget;
		}
		client->shockBudget -= msec;
		client->abilityRecharge -= msec;
		if ( client->abilityRecharge < 0 ) {
			client->abilityRecharge = 0;
		}
	}
	return qtrue;
}

/*
==============================================================================

KLESK: SPINE VOLLEY

==============================================================================
*/

static void G_SpineImpact( gentity_t *spine, trace_t *trace ) {
	gentity_t	*other = &g_entities[trace->entityNum];
	vec3_t		velocity;

	if ( other->takedamage ) {
		BG_EvaluateTrajectoryDelta( &spine->s.pos, level.time, velocity );
		G_Damage( other, spine, spine->parent, velocity, trace->endpos, 20, 0, MOD_SPINES );
		if ( other->client ) {
			G_Slow( other, 0.7f, 1500 );
		}
	}
	G_FreeEntity( spine );
}

void G_SpineVolley( gentity_t *ent ) {
	vec3_t		start, forward, right, dir;
	gentity_t	*spine;
	int			i;
	float		angle;

	G_AbilityEyes( ent, start, forward, right );
	for ( i = -2 ; i <= 2 ; i++ ) {
		angle = DEG2RAD( i * 3.0f );
		VectorScale( forward, cos( angle ), dir );
		VectorMA( dir, sin( angle ), right, dir );
		spine = G_AbilityMissile( ent, "spine", start, dir, 1500, TR_LINEAR, WP_PLASMAGUN );
		spine->s.generic1 = AV_SPINE;
		spine->abilityImpact = G_SpineImpact;
		spine->think = G_FreeEntity;
		spine->nextthink = level.time + 4000;
	}
}

/*
==============================================================================

SARGE: SUPPLY DROP

==============================================================================
*/

static qboolean G_CrateFor( gentity_t *crate, gentity_t *other ) {
	if ( !other->client || other->health <= 0 || other->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return qfalse;
	}
	if ( g_gametype.integer >= GT_TEAM ) {
		return OnSameTeam( crate->parent, other );
	}
	return other == crate->parent;
}

static void G_CrateThink( gentity_t *crate ) {
	gentity_t			*other;
	gclient_t			*cl;
	int					i, w, max;

	if ( level.time >= crate->abilityTime ) {
		G_FreeEntity( crate );
		return;
	}
	crate->nextthink = level.time + CHAMP_FRAME;
	if ( crate->s.pos.trType != TR_STATIONARY || level.time < crate->wait ) {
		return;		// still flying, or not open yet
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !G_CrateFor( crate, other ) || G_AbilityMarked( crate, i )
			|| Distance( other->r.currentOrigin, crate->r.currentOrigin ) > 48 ) {
			continue;
		}
		G_AbilityMark( crate, i );
		cl = other->client;
		cl->ps.stats[STAT_ARMOR] += 50;
		if ( cl->ps.stats[STAT_ARMOR] > G_MaxArmor( cl ) ) {
			cl->ps.stats[STAT_ARMOR] = G_MaxArmor( cl );
		}
		for ( w = WP_MACHINEGUN ; w < WP_NUM_WEAPONS ; w++ ) {
			if ( !( cl->ps.stats[STAT_WEAPONS] & ( 1 << w ) ) || !BG_WeaponTuned( w ) ) {
				continue;
			}
			max = BG_WeaponParam( w, WPP_AMMOMAX );
			if ( max <= 0 ) {
				continue;
			}
			cl->ps.ammo[w] += max / 2;
			if ( cl->ps.ammo[w] > max ) {
				cl->ps.ammo[w] = max;
			}
		}
		G_AddEvent( other, EV_ITEM_PICKUP, BG_FindItem( "Armor Shard" ) - bg_itemlist );
	}
}

static void G_CrateImpact( gentity_t *crate, trace_t *trace ) {
	vec3_t	point;

	VectorMA( trace->endpos, 1, trace->plane.normal, point );
	G_AbilityStop( crate, point );
	crate->abilityImpact = NULL;
	crate->clipmask = 0;
	crate->wait = level.time + 1000;			// opens after a second
	crate->abilityTime = level.time + 10000;	// and stays ten
	trap_LinkEntity( crate );
}

static void G_CrateDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	G_FreeEntity( self );
}

void G_SupplyDrop( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*crate;

	G_AbilityEyes( ent, start, forward, right );
	forward[2] += 0.3f;
	VectorNormalize( forward );
	crate = G_AbilityMissile( ent, "supplydrop", start, forward, 600, TR_GRAVITY, WP_GRENADE_LAUNCHER );
	crate->s.generic1 = AV_SUPPLYDROP;
	crate->abilityImpact = G_CrateImpact;
	crate->abilityTime = level.time + 15000;
	crate->think = G_CrateThink;
	crate->nextthink = level.time + CHAMP_FRAME;
	crate->takedamage = qtrue;
	crate->health = 100;
	crate->die = G_CrateDie;
	VectorSet( crate->r.mins, -12, -12, -12 );
	VectorSet( crate->r.maxs, 12, 12, 12 );
	crate->r.contents = CONTENTS_CORPSE;
}

/*
==============================================================================

HUNTER: MARK PREY, RAINE: DOWNPOUR

==============================================================================
*/

/*
=================
G_Slow

Run speed times scale for msec; the slow that lasts longest wins, its
strength with it (champion-audit.md)
=================
*/
void G_Slow( gentity_t *target, float scale, int msec ) {
	gclient_t	*cl = target->client;

	if ( !cl ) {
		return;
	}
	if ( cl->slowEndTime <= level.time || level.time + msec > cl->slowEndTime ) {
		cl->slowScale = scale;
		cl->slowEndTime = level.time + msec;
	}
}

static int G_MarkTeam( gentity_t *ent ) {
	if ( g_gametype.integer >= GT_TEAM ) {
		return ent->client->sess.sessionTeam;
	}
	return 1000 + ent->s.number;
}

static void G_Mark( gentity_t *target, gentity_t *by, int msec ) {
	if ( !target->client ) {
		return;
	}
	if ( target->client->markEndTime < level.time + msec ) {
		target->client->markEndTime = level.time + msec;
	}
	target->client->markTeam = G_MarkTeam( by );
}

void G_MarkPrey( gentity_t *ent ) {
	vec3_t		start, forward, right, end;
	trace_t		tr;
	gentity_t	*target;

	G_AbilityEyes( ent, start, forward, right );
	VectorMA( start, 2000, forward, end );
	trap_Trace( &tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT );
	target = tr.entityNum < ENTITYNUM_MAX_NORMAL ? &g_entities[tr.entityNum] : NULL;
	if ( target && G_AbilityEnemy( ent, target ) ) {
		G_Mark( target, ent, 6000 );
		return;
	}
	// a miss gives half the cooldown back
	ent->client->abilityRecharge += BG_AbilityParam( ACTIVE_MARKPREY, AP_RECHARGE ) / 2;
}

static void G_StormThink( gentity_t *storm ) {
	gentity_t	*other;
	vec3_t		d;
	int			i;

	if ( level.time >= storm->abilityTime || !storm->parent || !storm->parent->inuse ) {
		G_FreeEntity( storm );
		return;
	}
	storm->nextthink = level.time + 500;
	if ( level.time < storm->wait ) {
		return;		// gathering
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !G_AbilityEnemy( storm->parent, other ) ) {
			continue;
		}
		VectorSubtract( other->r.currentOrigin, storm->r.currentOrigin, d );
		if ( d[2] < -64 || d[2] > 400 || d[0] * d[0] + d[1] * d[1] > 160 * 160 ) {
			continue;
		}
		G_Damage( other, storm, storm->parent, NULL, other->r.currentOrigin, 10, DAMAGE_NO_KNOCKBACK, MOD_STORM );
		// shown to Raine only, with no +15% mark (champion-audit.md)
		if ( other->client ) {
			other->client->revealEndTime = level.time + 600;
			other->client->revealTeam = 1000 + storm->parent->s.number;
		}
	}
}

void G_Downpour( gentity_t *ent ) {
	vec3_t		start, forward, right, end, down;
	trace_t		tr;
	gentity_t	*storm;
	vec3_t		up = { 0, 0, 1 };

	// the floor below where the aim meets the world, up to 1200 u away
	G_AbilityEyes( ent, start, forward, right );
	VectorMA( start, 1200, forward, end );
	trap_Trace( &tr, start, NULL, NULL, end, ent->s.number, MASK_SOLID );
	VectorMA( tr.endpos, -8, forward, end );
	VectorCopy( end, down );
	down[2] -= 2000;
	trap_Trace( &tr, end, NULL, NULL, down, ent->s.number, MASK_SOLID );
	VectorCopy( tr.endpos, end );
	end[2] += 8;

	storm = G_AbilityMissile( ent, "downpour", end, up, 0, TR_STATIONARY, WP_LIGHTNING );
	storm->s.generic1 = AV_STORM;
	G_AbilityStop( storm, end );
	storm->clipmask = 0;
	storm->wait = level.time + 500;
	storm->abilityTime = level.time + 4500;
	storm->think = G_StormThink;
	storm->nextthink = level.time + 500;
	trap_LinkEntity( storm );
}

/*
==============================================================================

BEASTMASTER: RELEASE THE HOUND

==============================================================================
*/

static gentity_t *G_HoundTarget( gentity_t *hound ) {
	gentity_t	*other, *best = NULL;
	trace_t		tr;
	float		d, bestDist = 1500;
	int			i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !G_AbilityEnemy( hound->parent, other ) ) {
			continue;
		}
		d = Distance( other->r.currentOrigin, hound->r.currentOrigin );
		if ( d > bestDist ) {
			continue;
		}
		trap_Trace( &tr, hound->r.currentOrigin, NULL, NULL, other->r.currentOrigin, hound->s.number, MASK_SOLID );
		if ( tr.fraction < 1.0f ) {
			continue;
		}
		best = other;
		bestDist = d;
	}
	return best;
}

static void G_HoundThink( gentity_t *hound ) {
	gentity_t	*target;
	vec3_t		dir, move, ground;
	trace_t		tr;
	float		dist;

	if ( level.time >= hound->abilityTime || !hound->parent || !hound->parent->inuse ) {
		G_FreeEntity( hound );
		return;
	}
	hound->nextthink = level.time + CHAMP_FRAME;

	// hunt the nearest enemy in sight, else stay at heel
	target = G_HoundTarget( hound );
	if ( !target ) {
		target = hound->parent;
	}
	VectorSubtract( target->r.currentOrigin, hound->r.currentOrigin, dir );
	dir[2] = 0;
	dist = VectorNormalize( dir );

	if ( target != hound->parent && dist < 40 ) {
		if ( level.time >= hound->wait ) {
			hound->wait = level.time + 1000;
			G_Damage( target, hound, hound->parent, dir, target->r.currentOrigin, 35, 0, MOD_HOUND );
		}
		return;
	}
	if ( target == hound->parent && dist < 96 ) {
		return;
	}

	// run along the ground
	VectorScale( dir, 600 * CHAMP_FRAME * 0.001f, move );
	VectorAdd( hound->r.currentOrigin, move, move );
	trap_Trace( &tr, hound->r.currentOrigin, hound->r.mins, hound->r.maxs, move, hound->s.number, MASK_PLAYERSOLID );
	VectorCopy( tr.endpos, move );
	VectorCopy( move, ground );
	ground[2] -= 64;
	trap_Trace( &tr, move, hound->r.mins, hound->r.maxs, ground, hound->s.number, MASK_PLAYERSOLID );
	G_SetOrigin( hound, tr.endpos );
	vectoangles( dir, hound->s.angles );
	trap_LinkEntity( hound );
}

static void G_HoundDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	G_FreeEntity( self );
}

void G_ReleaseHound( gentity_t *ent ) {
	vec3_t		start, forward, right;
	gentity_t	*hound;
	trace_t		tr;
	vec3_t		mins = { -12, -12, -12 }, maxs = { 12, 12, 12 };

	AngleVectors( ent->client->ps.viewangles, forward, right, NULL );
	forward[2] = 0;
	VectorNormalize( forward );
	VectorMA( ent->client->ps.origin, 48, forward, start );
	trap_Trace( &tr, ent->client->ps.origin, mins, maxs, start, ent->s.number, MASK_PLAYERSOLID );

	hound = G_AbilityMissile( ent, "hound", tr.endpos, forward, 0, TR_STATIONARY, WP_ROCKET_LAUNCHER );
	hound->s.generic1 = AV_HOUND;
	G_AbilityStop( hound, tr.endpos );
	hound->clipmask = MASK_PLAYERSOLID;
	VectorCopy( mins, hound->r.mins );
	VectorCopy( maxs, hound->r.maxs );
	hound->r.contents = CONTENTS_BODY;
	hound->takedamage = qtrue;
	hound->health = 80;
	hound->die = G_HoundDie;
	hound->abilityTime = level.time + 8000;
	hound->think = G_HoundThink;
	hound->nextthink = level.time + CHAMP_FRAME;
	trap_LinkEntity( hound );
}

/*
==============================================================================

UNUMREY: RIFT ANCHOR

==============================================================================
*/

void G_DropAnchor( gentity_t *ent ) {
	vec3_t		up = { 0, 0, 1 };
	gentity_t	*anchor;

	VectorCopy( ent->client->ps.origin, ent->client->anchorOrigin );
	if ( ent->client->anchorEnt ) {
		G_FreeEntity( ent->client->anchorEnt );
	}
	anchor = G_AbilityMissile( ent, "riftanchor", ent->client->ps.origin, up, 0, TR_STATIONARY, WP_PLASMAGUN );
	anchor->s.generic1 = AV_ANCHOR;
	G_AbilityStop( anchor, ent->client->ps.origin );
	anchor->clipmask = 0;
	anchor->think = G_FreeEntity;
	anchor->nextthink = level.time + BG_AbilityParam( ACTIVE_RIFTANCHOR, AP_DURATION );
	trap_LinkEntity( anchor );
	ent->client->anchorEnt = anchor;
}

/*
=================
G_RecallAnchor

Back to the anchor at once, standing still; not with a flag or the soul
=================
*/
void G_RecallAnchor( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	gentity_t	*tent;

	if ( !client->anchorEnt || client->ps.powerups[PW_REDFLAG] || client->ps.powerups[PW_BLUEFLAG]
		|| client->ps.powerups[PW_NEUTRALFLAG] ) {
		return;
	}
	tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_OUT );
	tent->s.clientNum = ent->s.clientNum;
	tent = G_TempEntity( client->anchorOrigin, EV_PLAYER_TELEPORT_IN );
	tent->s.clientNum = ent->s.clientNum;

	VectorCopy( client->anchorOrigin, client->ps.origin );
	VectorClear( client->ps.velocity );
	client->ps.eFlags ^= EF_TELEPORT_BIT;
	G_SetOrigin( ent, client->anchorOrigin );
	G_RewindReset( ent );
	trap_LinkEntity( ent );

	G_FreeEntity( client->anchorEnt );
	client->anchorEnt = NULL;
	// the lockout follows the recall, not an unused anchor fading
	if ( client->ps.weaponTime < BG_AbilityParam( ACTIVE_RIFTANCHOR, AP_LOCKOUT ) ) {
		client->ps.weaponTime = BG_AbilityParam( ACTIVE_RIFTANCHOR, AP_LOCKOUT );
	}
	G_AbilityDone( ent );
}

/*
==============================================================================

PASSIVES AND PER-FRAME WORK

==============================================================================
*/

/*
=================
G_ChampionClientThink

Slows, marks, Klesk's regeneration and Unumrey's echo
=================
*/
void G_ChampionClientThink( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	int			passives = G_Passives( ent );
	int			add;

	// burning and poison, for the client's feedback (champion-audit.md)
	client->ps.powerups[PW_BURNING] = client->burnEndTime > level.time && ent->health > 0 ? client->burnEndTime : 0;
	client->ps.powerups[PW_POISONED] = client->poisonEndTime > level.time && ent->health > 0 ? client->poisonEndTime : 0;

	// marked (or seen by an enemy drone): everyone else is sent this player,
	// and sees them through walls
	// champion-audit.md: shown only to the player or team that marked them;
	// s.time2 carries which clients may see it (0-31)
	if ( ( client->markEndTime > level.time || client->revealEndTime > level.time ) && ent->health > 0
		&& client->undetectedUntil <= level.time ) {
		int		i, seers = 0;

		for ( i = 0 ; i < level.maxclients && i < 32 ; i++ ) {
			gclient_t	*cl = &level.clients[i];
			int			team = g_gametype.integer >= GT_TEAM ? cl->sess.sessionTeam : 1000 + i;

			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( ( client->markEndTime > level.time && client->markTeam == team )
				|| ( client->revealEndTime > level.time && client->revealTeam == team ) ) {
				seers |= 1 << i;
			}
		}
		client->ps.eFlags |= EF_MARKED;
		ent->s.time2 = seers;
	} else {
		client->ps.eFlags &= ~EF_MARKED;
		ent->s.time2 = 0;
	}

	// the rift anchor fades with the ability
	if ( client->anchorEnt && BG_AbilityState( &client->ps ) != ABILITY_ACTIVE ) {
		G_FreeEntity( client->anchorEnt );
		client->anchorEnt = NULL;
	}

	if ( ent->health <= 0 ) {
		return;
	}

	// Hive Regeneration: 3 s after the last hit, 5 health a second up to base health
	if ( ( passives & PASSIVE_HIVEREGEN ) && g_passiveRegen.integer && level.time - client->lastDamageTime >= 3000 && ent->health < 100 ) {
		client->hiveResidual += 5 * CHAMP_FRAME;
		add = client->hiveResidual / 1000;
		client->hiveResidual -= add * 1000;
		ent->health += add;
		if ( ent->health > 100 ) {
			ent->health = 100;
		}
		client->ps.stats[STAT_HEALTH] = ent->health;
	}

	// Armour Regen: 3 s after the last hit, 5 armour a second up to 50
	if ( ( passives & PASSIVE_ARMORREGEN ) && g_passiveRegen.integer && level.time - client->lastDamageTime >= 3000
		&& client->ps.stats[STAT_ARMOR] < 50 ) {
		client->armorRegenResidual += 5 * CHAMP_FRAME;
		add = client->armorRegenResidual / 1000;
		client->armorRegenResidual -= add * 1000;
		client->ps.stats[STAT_ARMOR] += add;
		if ( client->ps.stats[STAT_ARMOR] > 50 ) {
			client->ps.stats[STAT_ARMOR] = 50;
		}
	}

	// Echo: a quarter of the last hit heals back over 3 s
	if ( ( passives & PASSIVE_ECHO ) && client->echoLeft > 0 && level.time < client->echoEndTime ) {
		client->echoResidual += client->echoLeft * CHAMP_FRAME;
		add = client->echoResidual / ( client->echoEndTime - level.time + CHAMP_FRAME );
		if ( add > 0 ) {
			client->echoResidual = 0;
			if ( add > client->echoLeft ) {
				add = client->echoLeft;
			}
			client->echoLeft -= add;
			ent->health += add;
			if ( ent->health > client->ps.stats[STAT_MAX_HEALTH] ) {
				ent->health = client->ps.stats[STAT_MAX_HEALTH];
			}
			client->ps.stats[STAT_HEALTH] = ent->health;
		}
	}
}

/*
=================
G_ChampionClientDied
=================
*/
void G_ChampionClientDied( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( client->anchorEnt ) {
		G_FreeEntity( client->anchorEnt );
		client->anchorEnt = NULL;
	}
	client->markEndTime = 0;
	client->slowEndTime = 0;
	client->echoLeft = 0;
	client->ps.powerups[PW_SHELLSHOCK] = 0;
	client->ps.eFlags &= ~EF_MARKED;
}

/*
=================
G_ChampionDamageScale

Damage changes from abilities and passives, before armour
=================
*/
int G_ChampionDamageScale( gentity_t *targ, gentity_t *attacker, int damage, int dflags, int mod ) {
	int		tp = G_Passives( targ );
	float	scale = 1.0f;

	if ( targ->client && targ->client->invulnEndTime > level.time ) {
		return 0;
	}
	// Xaero's Eternal Champion
	// marked prey takes more from the team that marked it
	if ( targ->client && attacker && attacker->client && targ->client->markEndTime > level.time
		&& G_MarkTeam( attacker ) == targ->client->markTeam ) {
		scale *= 1.15f;
	}
	// Beastmaster: less splash from others
	if ( ( tp & PASSIVE_THICKHIDE ) && targ != attacker && ( dflags & DAMAGE_RADIUS ) ) {
		scale *= 0.8f;
	}
	// Hunter: no falling damage
	if ( ( tp & PASSIVE_LIGHTFEET ) && mod == MOD_FALLING ) {
		return 0;
	}
	return damage * scale;
}

/*
=================
G_ChampionDamaged

After a hit lands: Xaero's Undying and Unumrey's Echo
=================
*/
void G_ChampionDamaged( gentity_t *targ, int take ) {
	gclient_t	*client = targ->client;
	int			tp = G_Passives( targ );

	if ( !client || take <= 0 ) {
		return;
	}
	if ( tp & PASSIVE_ECHO ) {
		// the next hit cancels the last one's echo
		client->echoLeft = take / 4;
		client->echoEndTime = level.time + 3000;
		client->echoResidual = 0;
	}
}

/*
=================
G_UndyingSave

Master of Death (Xaero): while it runs, a killing blow leaves him at 120
health for 3 s instead; telefrags and the world still kill
=================
*/
int G_UndyingSave( gentity_t *targ, gentity_t *attacker, int take, int mod ) {
	gclient_t	*client = targ->client;

	if ( !client || !G_AbilityActive( targ, ACTIVE_MASTEROFDEATH ) || client->modDeferred ) {
		return take;
	}
	if ( level.time - client->modStartTime < MOD_WINDUP_MSEC || targ->health - take > 0 ) {
		return take;
	}
	switch ( mod ) {
	case MOD_TELEFRAG: case MOD_FALLING: case MOD_LAVA: case MOD_SLIME: case MOD_WATER:
	case MOD_CRUSH: case MOD_TRIGGER_HURT: case MOD_SUICIDE:
		return take;
	default:
		break;
	}
	client->modDeferred = qtrue;
	client->modDeferEnd = level.time + 3000;
	client->modKiller = attacker && attacker->client ? attacker->s.number : ENTITYNUM_WORLD;
	client->modFragged = qfalse;
	targ->health = client->ps.stats[STAT_HEALTH] = 120;
	return 0;
}

/*
=================
Master of Death

Usable below 50 health. The first half second holds his fire (and is
heard); a death in the next 3 s is held off (G_UndyingSave) and becomes
real unless he frags someone meanwhile; if nothing kills him he ends at 1
health.
=================
*/
void G_MasterOfDeathStart( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( ent->health >= 50 ) {
		// not hurt enough: the charge comes back
		BG_SetAbility( &client->ps, ABILITY_IDLE, BG_AbilityCharges( &client->ps ) + 1 );
		client->ps.stats[STAT_ABILITY_TIME] = 0;
		return;
	}
	client->modStartTime = level.time;
	client->modDeferred = qfalse;
	G_Sound( ent, CHAN_AUTO, G_SoundIndex( "sound/items/protect3.wav" ) );
}

void G_MasterOfDeathEnd( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( client->modStartTime && !client->modDeferred && ent->health > 0 ) {
		ent->health = client->ps.stats[STAT_HEALTH] = 1;
	}
	client->modStartTime = 0;
}

void G_MasterOfDeathFrag( gentity_t *attacker ) {
	if ( attacker && attacker->client && attacker->client->modDeferred ) {
		attacker->client->modFragged = qtrue;
	}
}

/*
=================
Leaping Kick

Medals bank kicks (up to 3, 10 s each); pressing the weapon's key again
spends one: a leap of 600 forward and at least 150 up that passes through
enemies for 25, then 5 s of air control
=================
*/
void G_BankKick( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	int			i;

	if ( !client || !( G_Passives( ent ) & PASSIVE_LEAPINGKICK ) ) {
		return;
	}
	for ( i = 0; i < KICK_MAX; i++ ) {
		if ( client->kickExpire[i] <= level.time ) {
			client->kickExpire[i] = level.time + 10000;
			return;
		}
	}
}

void G_LeapingKick( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	vec3_t		forward;
	int			i, w = ent->s.weapon;

	if ( !( G_Passives( ent ) & PASSIVE_LEAPINGKICK ) || ent->health <= 0
		|| ( w != WP_SHOTGUN && w != WP_SUPER_SHOTGUN && w != WP_ROCKET_LAUNCHER && w != WP_RAILGUN ) ) {
		return;
	}
	for ( i = 0; i < KICK_MAX; i++ ) {
		if ( client->kickExpire[i] > level.time ) {
			break;
		}
	}
	if ( i == KICK_MAX ) {
		return;
	}
	client->kickExpire[i] = 0;
	// the leap adds to his momentum rather than replacing it (champion-audit.md)
	AngleVectors( client->ps.viewangles, forward, NULL, NULL );
	VectorMA( client->ps.velocity, 600, forward, client->ps.velocity );
	if ( client->ps.velocity[2] < 150 ) {
		client->ps.velocity[2] = 150;
	}
	client->ps.groundEntityNum = ENTITYNUM_NONE;
	client->kickEndTime = level.time + 700;
	client->kickAirEnd = level.time + 5000;
	client->kickHits[0] = client->kickHits[1] = 0;
	G_AddEvent( ent, EV_JUMP, 0 );
}

/*
=================
G_XaeroThink

Kicks run out, the kick passes through players and hurts them once, the
held death is settled, and the state goes out in STAT_KICK
=================
*/
void G_XaeroThink( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	int			i, count = 0, air;

	if ( !( G_Passives( ent ) & PASSIVE_LEAPINGKICK ) && !client->modStartTime ) {
		return;
	}
	if ( client->modDeferred && level.time >= client->modDeferEnd ) {
		client->modDeferred = qfalse;
		if ( !client->modFragged && ent->health > 0 ) {
			gentity_t *killer = client->modKiller < MAX_CLIENTS ? &g_entities[client->modKiller] : NULL;

			ent->health = client->ps.stats[STAT_HEALTH] = -999;
			player_die( ent, killer, killer, 100000, MOD_UNKNOWN );
			return;
		}
	}
	// the kick: through players, 25 to each enemy it meets
	if ( client->kickEndTime > level.time ) {
		int			touch[MAX_GENTITIES], num;
		vec3_t		mins, maxs;

		VectorAdd( client->ps.origin, ent->r.mins, mins );
		VectorAdd( client->ps.origin, ent->r.maxs, maxs );
		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
		for ( i = 0; i < num; i++ ) {
			gentity_t	*other = &g_entities[touch[i]];
			int			n = touch[i];

			if ( n >= MAX_CLIENTS || other == ent || !other->client || other->health <= 0 || OnSameTeam( ent, other )
				|| ( client->kickHits[n >> 5] & ( 1 << ( n & 31 ) ) ) ) {
				continue;
			}
			client->kickHits[n >> 5] |= 1 << ( n & 31 );
			G_Damage( other, ent, ent, client->ps.velocity, other->r.currentOrigin, 25, DAMAGE_NO_KNOCKBACK, MOD_LEAPINGKICK );
		}
	}
	if ( !( G_Passives( ent ) & PASSIVE_LEAPINGKICK ) ) {
		return;
	}
	for ( i = 0; i < KICK_MAX; i++ ) {
		if ( client->kickExpire[i] > level.time ) {
			count++;
		}
	}
	air = client->kickAirEnd > level.time ? ( client->kickAirEnd - level.time + 99 ) / 100 : 0;
	if ( air > 63 ) {
		air = 63;
	}
	client->ps.stats[STAT_KICK] = count | ( air << 2 ) | ( client->kickEndTime > level.time ? KICK_PASSING : 0 );
}

/*
=================
G_MaxArmor

The class limit, except Eisen's lower one
=================
*/
int G_MaxArmor( gclient_t *client ) {
	if ( !Q_stricmp( BG_Champion( client->ps.stats[STAT_CHAMPION] )->name, "eisen" ) ) {
		return 150;
	}
	return G_ClassInfo( client )->maxArmor;
}
