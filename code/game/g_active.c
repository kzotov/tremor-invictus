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

#include "g_local.h"


/*
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player_state_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback( gentity_t *player ) {
	gclient_t	*client;
	float	count;
	vec3_t	angles;

	client = player->client;
	if ( client->ps.pm_type == PM_DEAD ) {
		return;
	}

	// total points of damage shot at the player this frame
	count = client->damage_blood + client->damage_armor;
	if ( count == 0 ) {
		return;		// didn't take any damage
	}

	if ( count > 255 ) {
		count = 255;
	}

	// send the information to the client

	// world damage (falling, slime, etc) uses a special code
	// to make the blend blob centered instead of positional
	if ( client->damage_fromWorld ) {
		client->ps.damagePitch = 255;
		client->ps.damageYaw = 255;

		client->damage_fromWorld = qfalse;
	} else {
		vectoangles( client->damage_from, angles );
		client->ps.damagePitch = angles[PITCH]/360.0 * 256;
		client->ps.damageYaw = angles[YAW]/360.0 * 256;
	}

	// play an appropriate pain sound; the drone is a machine: a clank at
	// most each 100 ms and no pain voice (abilities.md, Peeker drone)
	if ( client->droneBody ) {
		if ( level.time > player->pain_debounce_time ) {
			player->pain_debounce_time = level.time + 100;
			G_AddEvent( player, EV_DRONE_HIT, 0 );
			client->ps.damageEvent++;
		}
	} else if ( (level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) ) {
		player->pain_debounce_time = level.time + 700;
		G_AddEvent( player, EV_PAIN, player->health );
		client->ps.damageEvent++;
	}


	client->ps.damageCount = count;

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_knockback = 0;
}



/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects( gentity_t *ent ) {
	qboolean	envirosuit;
	int			waterlevel;

	if ( ent->client->noclip ) {
		ent->client->airOutTime = level.time + 12000;	// don't need air
		return;
	}

	waterlevel = ent->waterlevel;

	envirosuit = ent->client->ps.powerups[PW_BATTLESUIT] > level.time;

	//
	// check for drowning
	//
	if ( waterlevel == 3 ) {
		// envirosuit give air
		if ( envirosuit ) {
			ent->client->airOutTime = level.time + 10000;
		}

		// if out of air, start drowning
		if ( ent->client->airOutTime < level.time) {
			// drown!
			ent->client->airOutTime += 1000;
			if ( ent->health > 0 ) {
				// take more damage the longer underwater
				ent->damage += 2;
				if (ent->damage > 15)
					ent->damage = 15;

				// don't play a normal pain sound
				ent->pain_debounce_time = level.time + 200;

				G_Damage (ent, NULL, NULL, NULL, NULL, 
					ent->damage, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	} else {
		ent->client->airOutTime = level.time + 12000;
		ent->damage = 2;
	}

	//
	// check for sizzle damage (move to pmove?)
	//
	if (waterlevel && 
		(ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		if (ent->health > 0
			&& ent->pain_debounce_time <= level.time	) {

			if ( envirosuit ) {
				G_AddEvent( ent, EV_POWERUP_BATTLESUIT, 0 );
			} else {
				if (ent->watertype & CONTENTS_LAVA) {
					G_Damage (ent, NULL, NULL, NULL, NULL, 
						30*waterlevel, 0, MOD_LAVA);
				}

				if (ent->watertype & CONTENTS_SLIME) {
					G_Damage (ent, NULL, NULL, NULL, NULL, 
						10*waterlevel, 0, MOD_SLIME);
				}
			}
		}
	}
}



/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( gentity_t *ent ) {
#ifdef MISSIONPACK
	if( ent->s.eFlags & EF_TICKING ) {
		ent->client->ps.loopSound = G_SoundIndex( "sound/weapons/proxmine/wstbtick.wav");
	}
	else
#endif
	if (ent->waterlevel && (ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		ent->client->ps.loopSound = level.snd_fry;
	} else {
		ent->client->ps.loopSound = 0;
	}
	// an ability's loop while it runs (abilities-data: Visor's sight, the
	// Barrier's hum, the hook's rope pulling)
	if ( !ent->client->ps.loopSound && ent->client->ps.stats[STAT_HEALTH] > 0 ) {
		int		active = BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active;
		int		state = BG_AbilityState( &ent->client->ps );

		if ( active == ACTIVE_PIERCINGSIGHT && state == ABILITY_ACTIVE ) {
			ent->client->ps.loopSound = G_SoundIndex( "sound/abilities/piercingsight_loop.wav" );
		} else if ( active == ACTIVE_BARRIER && state == ABILITY_ACTIVE && ( ent->client->ps.eFlags & EF_BARRIER ) ) {
			ent->client->ps.loopSound = G_SoundIndex( "sound/abilities/barrier_loop.wav" );
		} else if ( active == ACTIVE_HOOK && state == ABILITY_HOOK_PULL ) {
			ent->client->ps.loopSound = G_SoundIndex( "sound/abilities/hook_loop.wav" );
		}
	}
}



//==============================================================

/*
==============
ClientImpacts
==============
*/
void ClientImpacts( gentity_t *ent, pmove_t *pm ) {
	int		i, j;
	trace_t	trace;
	gentity_t	*other;

	memset( &trace, 0, sizeof( trace ) );
	for (i=0 ; i<pm->numtouch ; i++) {
		for (j=0 ; j<i ; j++) {
			if (pm->touchents[j] == pm->touchents[i] ) {
				break;
			}
		}
		if (j != i) {
			continue;	// duplicated
		}
		other = &g_entities[ pm->touchents[i] ];

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, other, &trace );
		}

		if ( !other->touch ) {
			continue;
		}

		other->touch( other, ent, &trace );
	}

}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void	G_TouchTriggers( gentity_t *ent ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs;
	// wide enough for an item's whole pickup shape (70 u from the origin or
	// the feet, BG_PlayerTouchesItem) around a +-15 u item box
	static vec3_t	range = { 85, 85, 110 };

	if ( !ent->client ) {
		return;
	}

	// dead clients don't activate triggers!
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	VectorSubtract( ent->client->ps.origin, range, mins );
	VectorAdd( ent->client->ps.origin, range, maxs );

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	// can't use ent->absmin, because that has a one unit pad
	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

	for ( i=0 ; i<num ; i++ ) {
		hit = &g_entities[touch[i]];

		if ( !hit->touch && !ent->touch ) {
			continue;
		}
		if ( !( hit->r.contents & CONTENTS_TRIGGER ) ) {
			continue;
		}

		// ignore most entities if a spectator
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			if ( hit->s.eType != ET_TELEPORT_TRIGGER &&
				// this is ugly but adding a new ET_? type will
				// most likely cause network incompatibilities
				hit->touch != Touch_DoorTrigger) {
				continue;
			}
		}

		// use separate code for determining if an item is picked up
		// so you don't have to actually contact its bounding box
		if ( hit->s.eType == ET_ITEM ) {
			// Strogg's drone picks nothing up
			if ( G_AbilityActive( ent, ACTIVE_DRONE ) ) {
				continue;
			}
			{
				vec3_t	itemOrigin, reach;
				trace_t	los;

				if ( !BG_PlayerTouchesItem( &ent->client->ps, &hit->s, level.time, itemOrigin, reach ) ) {
					continue;
				}
				// never through a floor or a wall
				trap_Trace( &los, itemOrigin, NULL, NULL, reach, ENTITYNUM_NONE, MASK_SOLID );
				if ( los.fraction < 1.0f || los.startsolid ) {
					continue;
				}
			}
		} else {
			if ( !trap_EntityContact( mins, maxs, hit ) ) {
				continue;
			}
		}

		memset( &trace, 0, sizeof(trace) );

		if ( hit->touch ) {
			hit->touch (hit, ent, &trace);
		}

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, hit, &trace );
		}
	}

	// if we didn't touch a jump pad this pmove frame
	if ( ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount ) {
		ent->client->ps.jumppad_frame = 0;
		ent->client->ps.jumppad_ent = 0;
	}
}

/*
=================
G_AbilityActive
=================
*/
qboolean G_AbilityActive( gentity_t *ent, int active ) {
	if ( !ent->client ) {
		return qfalse;
	}
	return BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active == active
		&& BG_AbilityState( &ent->client->ps ) == ABILITY_ACTIVE;
}

/*
=================
G_CutAbilityCooldown

Pickup passives: take a fraction of the full recharge time off the wait
=================
*/
void G_CutAbilityCooldown( gentity_t *ent, float fraction ) {
	int		active;

	if ( !ent->client ) {
		return;
	}
	active = BG_Champion( ent->client->ps.stats[STAT_CHAMPION] )->active;
	// a share of the full cooldown (all charges: Keel's 60 s over three
	// grenades), off the one recharging (champion-audit.md)
	if ( BG_AbilityCharges( &ent->client->ps ) < BG_AbilityParam( active, AP_CHARGES ) ) {
		ent->client->abilityRecharge += fraction * BG_AbilityParam( active, AP_RECHARGE ) * BG_AbilityParam( active, AP_CHARGES );
	}
}

/*
=================
G_GhostWalkEnd

Coming out of ghost walk inside an enemy telefrags them
=================
*/
static void G_GhostWalkEnd( gentity_t *ent ) {
	int			touch[MAX_GENTITIES];
	vec3_t		mins, maxs;
	gentity_t	*other;
	int			i, num;

	ent->s.frame = 0;
	ent->r.contents = CONTENTS_BODY;

	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
	for ( i = 0 ; i < num ; i++ ) {
		other = &g_entities[touch[i]];
		if ( other == ent || !other->client || other->health <= 0 ) {
			continue;
		}
		if ( OnSameTeam( ent, other ) ) {
			continue;
		}
		G_Damage( other, ent, ent, NULL, NULL, g_ghostWalkTelefrag.integer,
			DAMAGE_NO_PROTECTION, MOD_GHOSTWALK );
	}
}

/*
=================
G_UseAbility

EV_USE_ABILITY from pmove: the press delay is over
=================
*/
static void G_UseAbility( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	int			max;

	switch ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->active ) {
	case ACTIVE_HOOK:
		Ability_HookFire( ent );
		break;

	case ACTIVE_INJECTION:
		// a lasting rise in maximum health for this life, and no more debuffs
		if ( client->ps.stats[STAT_MAX_HEALTH] < client->pers.maxHealth + g_injectionMaxGainCap.integer ) {
			client->ps.stats[STAT_MAX_HEALTH] += g_injectionMaxGain.integer;
		}
		max = client->ps.stats[STAT_MAX_HEALTH] + G_ClassInfo( client )->maxHealthBonus;
		G_ClearDamageOverTime( ent );
		client->ps.powerups[PW_SHELLSHOCK] = 0;
		if ( ent->health < max ) {
			ent->health += g_injectionHeal.integer;
			if ( ent->health > max ) {
				ent->health = max;
			}
			client->ps.stats[STAT_HEALTH] = ent->health;
		}
		break;

	case ACTIVE_GHOSTWALK:
		// its own state, not the invisibility power-up: s.frame (16 bits,
		// nothing else uses it on a player) is when it began, for the
		// others' fade (CG_GhostLook); never 0
		ent->s.frame = ( level.time & 0xffff ) ? level.time & 0xffff : 1;
		ent->r.contents = 0;
		break;

	default:
		G_AbilityFire( ent );
		break;
	}
}

/*
=================
G_Regeneration

After a while without damage, refill health to the top of its 25 HP block
=================
*/
static void G_Regeneration( gentity_t *ent, int msec ) {
	gclient_t	*client = ent->client;
	int			top, add;

	if ( !( BG_Champion( client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_REGENERATION ) || !g_passiveRegen.integer ) {
		return;
	}
	if ( ent->health <= 0 || level.time - client->lastDamageTime < g_regenDelay.integer ) {
		client->regenResidual = 0;
		return;
	}
	top = ( ( ent->health + 24 ) / 25 ) * 25;
	if ( ent->health >= top || top > client->ps.stats[STAT_MAX_HEALTH] ) {
		return;
	}
	client->regenResidual += msec * g_regenRate.integer;
	add = client->regenResidual / 1000;
	client->regenResidual -= add * 1000;
	ent->health += add;
	if ( ent->health > top ) {
		ent->health = top;
	}
	client->ps.stats[STAT_HEALTH] = ent->health;
}

/*
=================
G_UpdateRevealedPlayers

Piercing sight: while any player has it running, every player entity is sent
to all clients so it can be shown through walls
=================
*/
void G_UpdateRevealedPlayers( void ) {
	gentity_t	*ent;
	int			visors = 0, i, seers;

	// Piercing Sight shows everyone to the Visor using it, and to nobody
	// else (abilities-data); a mark only to who marked (s.time2's seers)
	for ( i = 0, ent = g_entities ; i < level.maxclients && i < 32 ; i++, ent++ ) {
		if ( ent->inuse && G_AbilityActive( ent, ACTIVE_PIERCINGSIGHT ) ) {
			visors |= 1 << i;
		}
	}
	for ( i = 0, ent = g_entities ; i < level.maxclients ; i++, ent++ ) {
		if ( !ent->inuse || !ent->client ) {
			continue;
		}
		ent->r.svFlags &= ~( SVF_BROADCAST | SVF_BROADCASTMASK );
		seers = visors & ~( i < 32 ? 1 << i : 0 );
		if ( ent->client->ps.eFlags & EF_MARKED ) {
			seers |= ent->s.time2;
		}
		if ( seers ) {
			ent->r.svFlags |= SVF_BROADCASTMASK;
			ent->r.singleClient = seers;
		}
	}
}

/*
=================
G_AbilityUpkeep

Gives back ability charges, follows the ability state that pmove drives,
and keeps the hook entity in step
=================
*/
static void G_AbilityUpkeep( gentity_t *ent, int msec ) {
	gclient_t			*client = ent->client;
	const champion_t	*champ = BG_Champion( client->ps.stats[STAT_CHAMPION] );
	int					state, charges, maxCharges, recharge;

	G_Regeneration( ent, msec );
	G_AbilityClientThink( ent, msec );
	G_WeaponThink( ent );

	// the ability as it stands, for the next spawn (duel.md 1)
	client->pers.keptChampion = client->ps.stats[STAT_CHAMPION];
	client->pers.keptCharges = BG_AbilityCharges( &client->ps );
	client->pers.keptRecharge = client->abilityRecharge;
	client->pers.keptTime = level.time;

	if ( champ->active == ACTIVE_NONE ) {
		return;
	}

	state = BG_AbilityState( &client->ps );
	charges = BG_AbilityCharges( &client->ps );
	maxCharges = BG_AbilityParam( champ->active, AP_CHARGES );
	recharge = BG_AbilityParam( champ->active, AP_RECHARGE );

	if ( champ->active == ACTIVE_HOOK ) {
		// pmove let go, or the hook expired or missed
		if ( client->hook && state != ABILITY_HOOK_FLYING && state != ABILITY_HOOK_PULL ) {
			Weapon_HookFree( client->hook );
		}
		if ( !client->hook ) {
			client->hookDamageTaken = 0;
		}
		if ( !client->hook && ( state == ABILITY_HOOK_FLYING || state == ABILITY_HOOK_PULL ) ) {
			// the wait for the next throw counts from this throw
			// (abilities-data): 1.0 s, 2.0 s after a hook that never bit.
			// The idle time counts up to HOOK_THROW_GAP, so it starts where
			// that leaves it (pmove does the same for its own ends)
			int		gap = state == ABILITY_HOOK_FLYING ? HOOK_MISS_GAP : HOOK_THROW_GAP;
			int		t = HOOK_THROW_GAP - gap + HOOK_SINCE_THROW( &client->ps );

			client->ps.stats[STAT_ABILITY_TIME] = t > HOOK_THROW_GAP ? HOOK_THROW_GAP : t;
			state = ABILITY_IDLE;
			client->ps.stats[STAT_MOVE_STATE] = 0;		// (the swing's rope)
			client->ps.ropeBends = 0;
			client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
		}
		// where the rope bends, for everyone else to draw
		if ( client->hook ) {
			client->hook->s.ropeBends = client->ps.ropeBends;
			memcpy( client->hook->s.ropeBend, client->ps.ropeBend, sizeof( client->hook->s.ropeBend ) );
		}
	}

	// a hook that never bit (missed, expired or called back) costs nothing
	// (champion-audit.md)
	if ( champ->active == ACTIVE_HOOK && client->abilityPrevState == ABILITY_HOOK_FLYING
		&& state == ABILITY_IDLE && charges < maxCharges ) {
		charges++;
	}

	// timed abilities starting and ending
	if ( client->abilityPrevState != ABILITY_ACTIVE && state == ABILITY_ACTIVE ) {
		G_AbilityStarted( ent );
		state = BG_AbilityState( &client->ps );		// (a refused start, as Master of Death's, is idle again)
		charges = BG_AbilityCharges( &client->ps );	// (with its charge given back)
	}
	// champion-audit.md: the end sound on every end (natural, second press,
	// cancel), not on a second press that doesn't end it
	if ( ( client->abilityPrevState == ABILITY_ACTIVE || client->abilityPrevState == ABILITY_RUSH
		|| client->abilityPrevState == ABILITY_HOOK_FLYING || client->abilityPrevState == ABILITY_HOOK_PULL )
		&& state == ABILITY_IDLE ) {
		G_AddEvent( ent, EV_ABILITY_END, champ->active );		// (whose end sound)
	}
	if ( client->abilityPrevState == ABILITY_ACTIVE && state != ABILITY_ACTIVE ) {
		if ( champ->active == ACTIVE_GHOSTWALK ) {
			G_GhostWalkEnd( ent );
		}
		G_AbilityEnded( ent );
	}
	client->abilityPrevState = state;

	// clan arena: active abilities are off
	if ( G_ClanArena() ) {
		charges = maxCharges = 0;
	}

	// one charge recharges at a time; shell shock runs it backwards; the
	// cooldown starts only once the ability is over: timed abilities, the
	// hook, the rush, fireballs and the orb in flight, Strogg's drone
	// (champion-audit.md, deep audit: the data spends the charge at the end)
	if ( charges < maxCharges && G_ShellShockRecharge( client, msec ) ) {
		// held back by the shock
	} else if ( charges < maxCharges && ( client->droneBody || state != ABILITY_IDLE ) ) {
		// paused, not lost: a charge already on its way keeps its progress
	} else if ( charges < maxCharges && !level.timeoutEnd ) {
		client->abilityRecharge += msec;
		if ( client->abilityRecharge >= recharge ) {
			client->abilityRecharge = 0;
			charges++;
		}
	} else {
		client->abilityRecharge = 0;
	}

	BG_SetAbility( &client->ps, state, charges );
	if ( charges < maxCharges ) {
		client->ps.stats[STAT_ABILITY_RECHARGE] = ( recharge - client->abilityRecharge + 99 ) / 100;
	} else {
		client->ps.stats[STAT_ABILITY_RECHARGE] = 0;
	}
}

/*
=================
G_EndRush
=================
*/
void G_EndRush( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( BG_AbilityState( &client->ps ) != ABILITY_RUSH ) {
		return;
	}
	BG_SetAbility( &client->ps, ABILITY_IDLE, BG_AbilityCharges( &client->ps ) );
	client->ps.stats[STAT_ABILITY_TIME] = 0;
	if ( client->ps.weaponTime < BG_AbilityParam( ACTIVE_BULLRUSH, AP_LOCKOUT ) ) {
		client->ps.weaponTime = BG_AbilityParam( ACTIVE_BULLRUSH, AP_LOCKOUT );
	}
}

/*
=================
G_HeavyweightDamage

Collision damage for a given impact speed, 0 below the threshold
=================
*/
static int G_HeavyweightDamage( float speed ) {
	int		damage;

	if ( speed < g_hwSpeed.value ) {
		return 0;
	}
	if ( speed < g_hwSpeed2.value ) {
		damage = g_hwDamage.value + ( g_hwDamage2.value - g_hwDamage.value )
			* ( speed - g_hwSpeed.value ) / ( g_hwSpeed2.value - g_hwSpeed.value );
	} else {
		damage = g_hwDamage2.value + g_hwDamagePer100.value * ( speed - g_hwSpeed2.value ) / 100;
	}
	// the stomp is capped (g_hwDamageCap, 25)
	if ( g_hwDamageCap.integer > 0 && damage > g_hwDamageCap.integer ) {
		damage = g_hwDamageCap.integer;
	}
	return damage;
}

/*
=================
G_CollisionDamage

Heavyweight: running or falling into an enemy hurts them and knocks them back.
Athena's hook swing hurts whoever she swings into.
=================
*/
static void G_CollisionDamage( gentity_t *ent, pmove_t *pm, float impactSpeed, const vec3_t preVelocity ) {
	gclient_t			*client = ent->client;
	const champion_t	*champ = BG_Champion( client->ps.stats[STAT_CHAMPION] );
	gentity_t			*other;
	vec3_t				dir;
	int					i, damage;
	qboolean			swinging;

	if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}
	// Athena's contact hit in the air while hooked (pulled or swinging),
	// and after letting go until she lands (abilities-data), from 480 u/s
	if ( BG_AbilityState( &client->ps ) == ABILITY_HOOK_PULL ) {
		client->hookImpactLive = qtrue;
	} else if ( client->ps.groundEntityNum != ENTITYNUM_NONE ) {
		client->hookImpactLive = qfalse;
	}
	swinging = client->hookImpactLive && client->ps.groundEntityNum == ENTITYNUM_NONE
		&& champ->active == ACTIVE_HOOK;
	if ( !( champ->abilities & MOVEABIL_HEAVYWEIGHT ) && !swinging ) {
		return;
	}

	for ( i = 0 ; i < pm->numtouch ; i++ ) {
		if ( pm->touchents[i] >= MAX_CLIENTS ) {
			continue;
		}
		other = &g_entities[ pm->touchents[i] ];
		if ( other == ent || !other->client || other->health <= 0 || !other->takedamage ) {
			continue;
		}
		if ( OnSameTeam( ent, other ) && ( swinging || ( champ->abilities & MOVEABIL_HEAVYWEIGHT ) ) ) {
			continue;		// Athena's impact, the Bull Rush and the stomp hit enemies only (and teammates don't stop it)
		}
		if ( level.time - client->collisionTime[other->s.number]
			< ( swinging ? g_hookSwingRepeat.integer : g_collisionRepeat.integer ) ) {
			continue;
		}

		if ( ( champ->abilities & MOVEABIL_HEAVYWEIGHT ) && BG_AbilityState( &client->ps ) == ABILITY_RUSH ) {
			// a rush hits for g_rushSlowDamage below the fast speed and
			// g_rushDamage at it (abilities-data: a step, 50 / 75 at 400)
			damage = impactSpeed >= g_rushFastSpeed.value ? g_rushDamage.integer : g_rushSlowDamage.integer;
		} else if ( champ->abilities & MOVEABIL_HEAVYWEIGHT ) {
			damage = G_HeavyweightDamage( impactSpeed );
		} else {
			// the swing hurts only when fast enough: last frame's full
			// speed, 480 u/s (abilities-data)
			damage = VectorLength( preVelocity ) >= g_hookSwingSpeed.value ? g_hookSwingDamage.integer : 0;
		}
		if ( damage <= 0 ) {
			continue;
		}
		client->collisionTime[other->s.number] = level.time;

		VectorSubtract( other->r.currentOrigin, ent->r.currentOrigin, dir );
		dir[2] = 0;
		if ( VectorNormalize( dir ) == 0 ) {
			VectorSet( dir, 0, 0, 1 );
		}
		if ( champ->abilities & MOVEABIL_HEAVYWEIGHT ) {
			// abilities-data: the push is impulse 100 (g_hwKnockback) x1.5 for
			// a Bull Rush hit (to the 132 cap) tilted up 0.2 m, x0.5 for the
			// Heavyweight stomp, flat; each its own damage type
			qboolean	rush = BG_AbilityState( &client->ps ) == ABILITY_RUSH;

			G_NextKnockback( g_hwKnockback.value * ( rush ? 1.5f : 0.5f ) );
			G_NextTilt( rush ? 0.2f : 0 );
			G_Damage( other, ent, ent, dir, other->r.currentOrigin, damage, 0, rush ? MOD_BULLRUSH : MOD_HEAVYWEIGHT );
		} else {
			// abilities.md: Athena's impact, impulse 80 tilted up 12 u (0.3 m)
			G_NextKnockback( 80 );
			G_NextTilt( 0.3f );
			G_Damage( other, ent, ent, dir, other->r.currentOrigin, damage, 0, MOD_HOOKIMPACT );
		}
		if ( other->health > 0 ) {
			if ( BG_AbilityState( &client->ps ) == ABILITY_RUSH ) {
				G_Slow( other, 0.7f, 1000 );		// spirit slow (the rush is ABILITY_RUSH, not ACTIVE)
			}
			// a survivor stops the bull rush; a kill does not
			G_EndRush( ent );
		}
		// abilities.md: Athena's hook lets go after her impact hits; a hit
		// that kills keeps her speed (movement.md)
		if ( !( champ->abilities & MOVEABIL_HEAVYWEIGHT ) && client->hook ) {
			Weapon_HookFree( client->hook );
			if ( other->health <= 0 ) {
				VectorCopy( preVelocity, client->ps.velocity );
			}
		}
	}
}

/*
=================
SpectatorThink
=================
*/
void SpectatorThink( gentity_t *ent, usercmd_t *ucmd ) {
	pmove_t	pm;
	gclient_t	*client;

	client = ent->client;

	if ( client->sess.spectatorState != SPECTATOR_FOLLOW || !( client->ps.pm_flags & PMF_FOLLOW ) ) {
		if ( client->sess.spectatorState == SPECTATOR_FREE ) {
			if ( client->noclip ) {
				client->ps.pm_type = PM_NOCLIP;
			} else {
				client->ps.pm_type = PM_SPECTATOR;
			}
		} else {
			client->ps.pm_type = PM_FREEZE;
		}

		client->ps.speed = 400;	// faster than normal

		// set up for pmove
		memset (&pm, 0, sizeof(pm));
		pm.ps = &client->ps;
		pm.cmd = *ucmd;
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;	// spectators can fly through bodies
		pm.trace = trap_Trace;
		pm.pointcontents = trap_PointContents;

		// perform a pmove
		Pmove (&pm);
		// save results of pmove
		VectorCopy( client->ps.origin, ent->s.origin );

		G_TouchTriggers( ent );
		trap_UnlinkEntity( ent );
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;

	// attack button cycles through spectators
	if ( ( client->buttons & BUTTON_ATTACK ) && ! ( client->oldbuttons & BUTTON_ATTACK ) ) {
		Cmd_FollowCycle_f( ent, 1 );
	}
	// jump while following: back to the free camera (spectator.md keys)
	if ( client->sess.spectatorState == SPECTATOR_FOLLOW && ucmd->upmove > 0 && !client->specJumpHeld ) {
		StopFollowing( ent );
	}
	client->specJumpHeld = ucmd->upmove > 0;
}



/*
=================
ClientInactivityTimer

Returns qfalse if the client is dropped
=================
*/
qboolean ClientInactivityTimer( gclient_t *client ) {
	if ( !g_inactivity.integer || level.warmupTime || level.intermissiontime
		|| client->sess.sessionTeam == TEAM_SPECTATOR ) {
		// give everyone some time, so if the operator sets g_inactivity during
		// gameplay, everyone isn't kicked
		client->inactivityTime = level.time + 60 * 1000;
		client->inactivityWarning = qfalse;
	} else if ( client->pers.cmd.forwardmove || 
		client->pers.cmd.rightmove || 
		client->pers.cmd.upmove ||
		(client->pers.cmd.buttons & BUTTON_ATTACK) ) {
		client->inactivityTime = level.time + g_inactivity.integer * 1000;
		client->inactivityWarning = qfalse;
	} else if ( !client->pers.localClient ) {
		if ( level.time > client->inactivityTime ) {
			// idle in a match: off to the spectators, the seat goes to someone else
			SetTeam( &g_entities[client - level.clients], "spectator" );
			trap_SendServerCommand( client - level.clients, va( "print \"%s\n\"", BG_String( STR_IDLE_MOVED ) ) );
			return qfalse;
		}
		if ( level.time > client->inactivityTime - 10000 && !client->inactivityWarning ) {
			client->inactivityWarning = qtrue;
			trap_SendServerCommand( client - level.clients, va( "cp \"%s\"", BG_String( STR_IDLE_WARNING ) ) );
		}
	}
	return qtrue;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
void ClientTimerActions( gentity_t *ent, int msec ) {
	gclient_t	*client;
#ifdef MISSIONPACK
	int			maxHealth;
#endif

	client = ent->client;
	if ( level.timeoutEnd ) {
		return;		// nothing regenerates or wears off during a timeout
	}
	client->timeResidual += msec;

	while ( client->timeResidual >= 1000 ) {
		client->timeResidual -= 1000;

		// regenerate
#ifdef MISSIONPACK
		if( bg_itemlist[client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
			maxHealth = client->ps.stats[STAT_MAX_HEALTH] / 2;
		}
		else if ( client->ps.powerups[PW_REGEN] ) {
			maxHealth = client->ps.stats[STAT_MAX_HEALTH];
		}
		else {
			maxHealth = 0;
		}
		if( maxHealth ) {
			if ( ent->health < maxHealth ) {
				ent->health += 15;
				if ( ent->health > maxHealth * 1.1 ) {
					ent->health = maxHealth * 1.1;
				}
				G_AddEvent( ent, EV_POWERUP_REGEN, 0 );
			} else if ( ent->health < maxHealth * 2) {
				ent->health += 5;
				if ( ent->health > maxHealth * 2 ) {
					ent->health = maxHealth * 2;
				}
				G_AddEvent( ent, EV_POWERUP_REGEN, 0 );
			}
#else
		if ( client->ps.powerups[PW_REGEN] ) {
			if ( ent->health < client->ps.stats[STAT_MAX_HEALTH]) {
				ent->health += 15;
				if ( ent->health > client->ps.stats[STAT_MAX_HEALTH] * 1.1 ) {
					ent->health = client->ps.stats[STAT_MAX_HEALTH] * 1.1;
				}
				G_AddEvent( ent, EV_POWERUP_REGEN, 0 );
			} else if ( ent->health < client->ps.stats[STAT_MAX_HEALTH] + G_ClassInfo( client )->maxHealthBonus ) {
				// (up to the overheal cap, 175, not twice the maximum)
				ent->health += 5;
				if ( ent->health > client->ps.stats[STAT_MAX_HEALTH] + G_ClassInfo( client )->maxHealthBonus ) {
					ent->health = client->ps.stats[STAT_MAX_HEALTH] + G_ClassInfo( client )->maxHealthBonus;
				}
				G_AddEvent( ent, EV_POWERUP_REGEN, 0 );
			}
#endif
		} else if ( !G_ClanArena() ) {
			// count down health when over max; slow decay skips every other second
			if ( ent->health > client->ps.stats[STAT_MAX_HEALTH] ) {
				if ( ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->passives & PASSIVE_SLOWDECAY )
					&& !client->decaySkipped ) {
					client->decaySkipped = qtrue;
				} else {
					client->decaySkipped = qfalse;
					ent->health--;
				}
			}
		}

		// count down armor when over the class base armor (the clan arena stack stays)
		if ( client->ps.stats[STAT_ARMOR] > G_ClassInfo( client )->baseArmor && !G_ClanArena() ) {
			client->ps.stats[STAT_ARMOR]--;
		}
	}
#ifdef MISSIONPACK
	if( bg_itemlist[client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_AMMOREGEN ) {
		int w, max, inc, t, i;
    int weapList[]={WP_MACHINEGUN,WP_SHOTGUN,WP_GRENADE_LAUNCHER,WP_ROCKET_LAUNCHER,WP_LIGHTNING,WP_RAILGUN,WP_PLASMAGUN,WP_BFG,WP_NAILGUN,WP_PROX_LAUNCHER,WP_CHAINGUN};
    int weapCount = ARRAY_LEN( weapList );
		//
    for (i = 0; i < weapCount; i++) {
		  w = weapList[i];

		  switch(w) {
			  case WP_MACHINEGUN: max = 50; inc = 4; t = 1000; break;
			  case WP_SHOTGUN: max = 10; inc = 1; t = 1500; break;
			  case WP_GRENADE_LAUNCHER: max = 10; inc = 1; t = 2000; break;
			  case WP_ROCKET_LAUNCHER: max = 10; inc = 1; t = 1750; break;
			  case WP_LIGHTNING: max = 50; inc = 5; t = 1500; break;
			  case WP_RAILGUN: max = 10; inc = 1; t = 1750; break;
			  case WP_PLASMAGUN: max = 50; inc = 5; t = 1500; break;
			  case WP_BFG: max = 10; inc = 1; t = 4000; break;
			  case WP_NAILGUN: max = 10; inc = 1; t = 1250; break;
			  case WP_PROX_LAUNCHER: max = 5; inc = 1; t = 2000; break;
			  case WP_CHAINGUN: max = 100; inc = 5; t = 1000; break;
			  default: max = 0; inc = 0; t = 1000; break;
		  }
		  client->ammoTimes[w] += msec;
		  if ( client->ps.ammo[w] >= max ) {
			  client->ammoTimes[w] = 0;
		  }
		  if ( client->ammoTimes[w] >= t ) {
			  while ( client->ammoTimes[w] >= t )
				  client->ammoTimes[w] -= t;
			  client->ps.ammo[w] += inc;
			  if ( client->ps.ammo[w] > max ) {
				  client->ps.ammo[w] = max;
			  }
		  }
    }
	}
#endif
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink( gclient_t *client ) {
	client->ps.eFlags &= ~EF_TALK;
	client->ps.eFlags &= ~EF_FIRING;

	// the level will exit when everyone wants to or after timeouts

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = client->pers.cmd.buttons;
	if ( client->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) & ( client->oldbuttons ^ client->buttons ) ) {
		// this used to be an ^1 but once a player says ready, it should stick
		client->readyToExit = 1;
	}
}


/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
void ClientEvents( gentity_t *ent, int oldEventSequence ) {
	int		i, j;
	int		event;
	gclient_t *client;
	vec3_t	origin, angles;
//	qboolean	fired;
	gitem_t *item;
	gentity_t *drop;

	client = ent->client;

	if ( oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS ) {
		oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
	}
	for ( i = oldEventSequence ; i < client->ps.eventSequence ; i++ ) {
		event = client->ps.events[ i & (MAX_PS_EVENTS-1) ];

		switch ( event ) {
		case EV_USE_ABILITY:
			G_UseAbility( ent );
			break;

		case EV_ABILITY_REPRESS:
			G_AbilityRepress( ent );
			break;

		case EV_FALL_MEDIUM:
		case EV_FALL_FAR:
			break;		// movement.md: no fall damage; the events are only sounds

		case EV_FIRE_WEAPON:
			FireWeapon( ent );
			break;

		case EV_USE_ITEM1:		// teleporter
			// drop flags in CTF
			item = NULL;
			j = 0;

			if ( ent->client->ps.powerups[ PW_REDFLAG ] ) {
				item = BG_FindItemForPowerup( PW_REDFLAG );
				j = PW_REDFLAG;
			} else if ( ent->client->ps.powerups[ PW_BLUEFLAG ] ) {
				item = BG_FindItemForPowerup( PW_BLUEFLAG );
				j = PW_BLUEFLAG;
			} else if ( ent->client->ps.powerups[ PW_NEUTRALFLAG ] ) {
				item = BG_FindItemForPowerup( PW_NEUTRALFLAG );
				j = PW_NEUTRALFLAG;
			}

			if ( item ) {
				drop = Drop_Item( ent, item, 0 );
				// decide how many seconds it has left
				drop->count = ( ent->client->ps.powerups[ j ] - level.time ) / 1000;
				if ( drop->count < 1 ) {
					drop->count = 1;
				}

				ent->client->ps.powerups[ j ] = 0;
			}

#ifdef MISSIONPACK
			if ( g_gametype.integer == GT_HARVESTER ) {
				if ( ent->client->ps.generic1 > 0 ) {
					if ( ent->client->sess.sessionTeam == TEAM_RED ) {
						item = BG_FindItem( "Blue Cube" );
					} else {
						item = BG_FindItem( "Red Cube" );
					}
					if ( item ) {
						for ( j = 0; j < ent->client->ps.generic1; j++ ) {
							drop = Drop_Item( ent, item, 0 );
							if ( ent->client->sess.sessionTeam == TEAM_RED ) {
								drop->spawnflags = TEAM_BLUE;
							} else {
								drop->spawnflags = TEAM_RED;
							}
						}
					}
					ent->client->ps.generic1 = 0;
				}
			}
#endif
			SelectSpawnPoint( ent->client->ps.origin, origin, angles, qfalse );
			TeleportPlayer( ent, origin, angles );
			break;

		case EV_USE_ITEM2:		// medkit: to the maximum, no overheal (FIX-13)
			if ( ent->health < ent->client->ps.stats[STAT_MAX_HEALTH] ) {
				ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
			}

			break;

#ifdef MISSIONPACK
		case EV_USE_ITEM3:		// kamikaze
			// make sure the invulnerability is off
			ent->client->invulnerabilityTime = 0;
			// start the kamikze
			G_StartKamikaze( ent );
			break;

		case EV_USE_ITEM4:		// portal
			if( ent->client->portalID ) {
				DropPortalSource( ent );
			}
			else {
				DropPortalDestination( ent );
			}
			break;
		case EV_USE_ITEM5:		// invulnerability
			ent->client->invulnerabilityTime = level.time + 10000;
			break;
#endif

		default:
			break;
		}
	}

}

#ifdef MISSIONPACK
/*
==============
StuckInOtherClient
==============
*/
static int StuckInOtherClient(gentity_t *ent) {
	int i;
	gentity_t	*ent2;

	ent2 = &g_entities[0];
	for ( i = 0; i < MAX_CLIENTS; i++, ent2++ ) {
		if ( ent2 == ent ) {
			continue;
		}
		if ( !ent2->inuse ) {
			continue;
		}
		if ( !ent2->client ) {
			continue;
		}
		if ( ent2->health <= 0 ) {
			continue;
		}
		//
		if (ent2->r.absmin[0] > ent->r.absmax[0])
			continue;
		if (ent2->r.absmin[1] > ent->r.absmax[1])
			continue;
		if (ent2->r.absmin[2] > ent->r.absmax[2])
			continue;
		if (ent2->r.absmax[0] < ent->r.absmin[0])
			continue;
		if (ent2->r.absmax[1] < ent->r.absmin[1])
			continue;
		if (ent2->r.absmax[2] < ent->r.absmin[2])
			continue;
		return qtrue;
	}
	return qfalse;
}
#endif

void BotTestSolid(vec3_t origin);

/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents( playerState_t *ps ) {
	gentity_t *t;
	int event, seq;
	int extEvent, number;

	// if there are still events pending
	if ( ps->entityEventSequence < ps->eventSequence ) {
		// create a temporary entity for this event which is sent to everyone
		// except the client who generated the event
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		// set external event to zero before calling BG_PlayerStateToEntityState
		extEvent = ps->externalEvent;
		ps->externalEvent = 0;
		// create temporary entity for event
		t = G_TempEntity( ps->origin, event );
		number = t->s.number;
		BG_PlayerStateToEntityState( ps, &t->s, qtrue );
		t->s.number = number;
		t->s.eType = ET_EVENTS + event;
		t->s.eFlags |= EF_PLAYER_EVENT;
		t->s.otherEntityNum = ps->clientNum;
		// send to everyone except the client who generated the event
		t->r.svFlags |= SVF_NOTSINGLECLIENT;
		t->r.singleClient = ps->clientNum;
		// set back external event
		ps->externalEvent = extEvent;
	}
}

/*
=================
G_BerserkGuide

champion-audit.md: Berserk's punch lunges at an enemy on the aim line (the
12 u x 200 u guidance cylinder) who is beyond the punch's own 44 x 128 u;
the nearest such enemy goes to pmove in grapplePoint with LUNGE_TARGET
=================
*/
static void G_BerserkGuide( gentity_t *ent ) {
	gclient_t	*client = ent->client;
	gentity_t	*other, *best = NULL;
	vec3_t		eye, forward, delta, side, target;
	float		along, off, bestAlong = LUNGE_REACH + 1;
	int			i;
	trace_t		tr;

	// (only Berserk's champion: the same bit is Athena's HOOK_SWINGING)
	if ( BG_Champion( client->ps.stats[STAT_CHAMPION] )->active != ACTIVE_BERSERK ) {
		return;
	}
	client->ps.stats[STAT_MOVE_STATE] &= ~LUNGE_TARGET;
	if ( !G_AbilityActive( ent, ACTIVE_BERSERK ) ) {
		return;
	}
	VectorCopy( client->ps.origin, eye );
	eye[2] += client->ps.viewheight;
	AngleVectors( client->ps.viewangles, forward, NULL, NULL );
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		other = &g_entities[i];
		if ( !other->inuse || !G_AbilityEnemy( ent, other ) ) {
			continue;
		}
		VectorCopy( other->r.currentOrigin, target );
		VectorSubtract( target, eye, delta );
		along = DotProduct( delta, forward );
		if ( along < 0 || along > LUNGE_REACH + 16 ) {
			continue;
		}
		VectorMA( delta, -along, forward, side );
		off = VectorLength( side );
		if ( off > LUNGE_GUIDE_RADIUS + 16 ) {
			continue;		// (to the edge of the body)
		}
		if ( along <= LUNGE_HIT_REACH && off <= LUNGE_HIT_RADIUS ) {
			continue;		// in reach: punched, not lunged at
		}
		if ( along >= bestAlong ) {
			continue;
		}
		trap_Trace( &tr, eye, NULL, NULL, target, ent->s.number, MASK_SOLID );
		if ( tr.fraction < 1.0f ) {
			continue;
		}
		best = other;
		bestAlong = along;
	}
	if ( best ) {
		VectorCopy( best->r.currentOrigin, client->ps.grapplePoint );
		client->ps.stats[STAT_MOVE_STATE] |= LUNGE_TARGET;
	}
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
void ClientThink_real( gentity_t *ent ) {
	gclient_t	*client;
	pmove_t		pm;
	int			oldEventSequence;
	int			msec;
	float		impactSpeed;
	vec3_t		preVelocity;
	usercmd_t	*ucmd;

	client = ent->client;

	// don't think if the client is not yet connected (and thus not yet spawned in)
	if (client->pers.connected != CON_CONNECTED) {
		return;
	}
	// mark the time, so the connection sprite can be removed
	ucmd = &ent->client->pers.cmd;

	// sanity check the command time to prevent speedup cheating
	if ( ucmd->serverTime > level.time + 200 ) {
		ucmd->serverTime = level.time + 200;
//		G_Printf("serverTime <<<<<\n" );
	}
	if ( ucmd->serverTime < level.time - 1000 ) {
		ucmd->serverTime = level.time - 1000;
//		G_Printf("serverTime >>>>>\n" );
	} 

	msec = ucmd->serverTime - client->ps.commandTime;
	// following others may result in bad times, but we still want
	// to check for follow toggles
	if ( msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		return;
	}
	if ( msec > 200 ) {
		msec = 200;
	}

	if ( pmove_msec.integer < 8 ) {
		trap_Cvar_Set("pmove_msec", "8");
		trap_Cvar_Update(&pmove_msec);
	}
	else if (pmove_msec.integer > 33) {
		trap_Cvar_Set("pmove_msec", "33");
		trap_Cvar_Update(&pmove_msec);
	}

	if ( pmove_fixed.integer || client->pers.pmoveFixed ) {
		ucmd->serverTime = ((ucmd->serverTime + pmove_msec.integer-1) / pmove_msec.integer) * pmove_msec.integer;
		//if (ucmd->serverTime - client->ps.commandTime <= 0)
		//	return;
	}

	//
	// check for exiting intermission
	//
	if ( level.intermissiontime ) {
		ClientIntermissionThink( client );
		return;
	}

	// spectators don't do much
	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
			return;
		}
		SpectatorThink( ent, ucmd );
		return;
	}

	// check for inactivity timer, but never drop the local client of a non-dedicated server
	if ( !ClientInactivityTimer( client ) ) {
		return;
	}

	// clear the rewards if time
	if ( level.time > client->rewardTime ) {
		client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
	}

	if ( client->noclip ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
		client->ps.pm_type = PM_DEAD;
	} else if ( level.timeoutEnd ) {
		client->ps.pm_type = PM_FREEZE;		// timeout: everyone holds still
	} else if ( G_MatchMode() && ( level.halfTimeEnd
		|| ( G_ClanArena() && level.caState == CA_COUNTDOWN ) || G_RoundDuelHolds() ) ) {
		// half time, round starts: held in place, no firing
		client->ps.pm_type = PM_FREEZE;
	} else {
		client->ps.pm_type = PM_NORMAL;
	}
	// the start countdown: free to move but no firing (warmup.md; no
	// damage between players either, in G_Damage); cgame drops the button
	// the same way when it predicts
	if ( level.warmupTime > 0 && client->ps.stats[STAT_HEALTH] > 0 ) {
		ucmd->buttons &= ~BUTTON_ATTACK;		// (the dead still press fire to respawn)
	}
	// abilities.md: Strogg's input waits 0.25 s after the drone ends
	if ( client->droneEndTime && level.time < client->droneEndTime + DRONE_INPUT_WAIT ) {
		ucmd->forwardmove = ucmd->rightmove = ucmd->upmove = 0;
		ucmd->buttons = 0;
	}

	client->ps.gravity = g_gravity.value;

	// set speed
	client->ps.speed = BG_Champion( client->ps.stats[STAT_CHAMPION] )->runSpeed;
	// slows (spines, Bull Rush hits, Plasma Trail) and Ranger's haste
	if ( client->slowEndTime > level.time ) {
		client->ps.speed *= client->slowScale > 0 ? client->slowScale : 0.7f;
	}
	if ( client->hasteEndTime > level.time ) {
		client->ps.speed *= 1.1f;
	}

#ifdef MISSIONPACK
	if( bg_itemlist[client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_SCOUT ) {
		client->ps.speed *= 1.5;
	}
	else
#endif
	if ( client->ps.powerups[PW_HASTE] ) {
		client->ps.speed *= 1.3;
	}

	// Let go of the hook if we aren't firing
	if ( client->ps.weapon == WP_GRAPPLING_HOOK &&
		client->hook && !( ucmd->buttons & BUTTON_ATTACK ) ) {
		Weapon_HookFree(client->hook);
	}

	// set up for pmove
	oldEventSequence = client->ps.eventSequence;

	G_BerserkGuide( ent );

	memset (&pm, 0, sizeof(pm));

	// check for the hit-scan gauntlet, don't let the action
	// go through as an attack unless it actually hits something
	// (the weapon time counts down inside pmove: a punch due this command
	// is tested now)
	// Berserk's lunge: the punch lands on arrival, as soon as the target is
	// in the punch's reach (abilities-data); the lunge stops and the second
	// of recovery runs from then
	if ( ( client->ps.stats[STAT_MOVE_STATE] & 1023 ) && ( client->ps.stats[STAT_MOVE_STATE] & 1023 ) <= 251
		&& BG_Champion( client->ps.stats[STAT_CHAMPION] )->active == ACTIVE_BERSERK
		&& G_AbilityActive( ent, ACTIVE_BERSERK ) && CheckGauntletAttack( ent ) ) {
		client->ps.stats[STAT_MOVE_STATE] = ( client->ps.stats[STAT_MOVE_STATE] & ~1023 ) | 252;		// (into the stop)
		client->ps.weaponTime = 1000;
		client->ps.weaponstate = WEAPON_FIRING;
	}

	// (stunned, or Master of Death's first half second: no attack at all)
	if ( client->stunEndTime > level.time ) {
		ucmd->buttons &= ~( BUTTON_ATTACK | BUTTON_ABILITY );
	}
	if ( client->modStartTime && level.time - client->modStartTime < 500 ) {
		ucmd->buttons &= ~BUTTON_ATTACK;
	}
	// (the gauntlet held and staying: not while switching away, nor in
	// Ghost Walk, which fires nothing)
	if ( client->ps.weapon == WP_GAUNTLET && !( ucmd->buttons & BUTTON_TALK ) &&
		( ucmd->buttons & BUTTON_ATTACK ) && client->ps.weaponTime <= msec
		&& ( client->ps.weaponstate == WEAPON_READY || client->ps.weaponstate == WEAPON_FIRING )
		&& ( ucmd->weapon == WP_GAUNTLET || G_AbilityActive( ent, ACTIVE_BERSERK ) )
		&& !G_AbilityActive( ent, ACTIVE_GHOSTWALK ) ) {
		pm.gauntletHit = CheckGauntletAttack( ent );
	}

	if ( ent->flags & FL_FORCE_GESTURE ) {
		ent->flags &= ~FL_FORCE_GESTURE;
		ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
	}

#ifdef MISSIONPACK
	// check for invulnerability expansion before doing the Pmove
	if (client->ps.powerups[PW_INVULNERABILITY] ) {
		if ( !(client->ps.pm_flags & PMF_INVULEXPAND) ) {
			vec3_t mins = { -INVUL_RADIUS, -INVUL_RADIUS, -INVUL_RADIUS };
			vec3_t maxs = { INVUL_RADIUS, INVUL_RADIUS, INVUL_RADIUS };
			vec3_t oldmins, oldmaxs;

			VectorCopy (ent->r.mins, oldmins);
			VectorCopy (ent->r.maxs, oldmaxs);
			// expand
			VectorCopy (mins, ent->r.mins);
			VectorCopy (maxs, ent->r.maxs);
			trap_LinkEntity(ent);
			// check if this would get anyone stuck in this player
			if ( !StuckInOtherClient(ent) ) {
				// set flag so the expanded size will be set in PM_CheckDuck
				client->ps.pm_flags |= PMF_INVULEXPAND;
			}
			// set back
			VectorCopy (oldmins, ent->r.mins);
			VectorCopy (oldmaxs, ent->r.maxs);
			trap_LinkEntity(ent);
		}
	}
#endif

	pm.ps = &client->ps;
	pm.cmd = *ucmd;
	if ( pm.ps->pm_type == PM_DEAD ) {
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	}
	else if ( ent->r.svFlags & SVF_BOT ) {
		pm.tracemask = MASK_PLAYERSOLID | CONTENTS_BOTCLIP;
	}
	else {
		pm.tracemask = MASK_PLAYERSOLID;
	}
	if ( G_AbilityActive( ent, ACTIVE_GHOSTWALK ) || G_WarmupPractice() || client->kickEndTime > level.time ) {
		pm.tracemask &= ~CONTENTS_BODY;		// ghost walk and warm-up pass through players
	}
	pm.trace = trap_Trace;
	pm.pointcontents = trap_PointContents;
	pm.debugLevel = g_debugMove.integer;
	pm.noFootsteps = ( g_dmflags.integer & DF_NO_FOOTSTEPS ) > 0;

	pm.pmove_fixed = pmove_fixed.integer | client->pers.pmoveFixed;
	pm.pmove_msec = pmove_msec.integer;
	pm.autoHop = client->pers.autoHop;
	pm.movementOnly = G_WarmupPractice();
	if ( pm.movementOnly ) {
		G_WarmupAmmo( ent );
	}

	impactSpeed = VectorLength( client->ps.velocity );
	VectorCopy( client->ps.velocity, preVelocity );		// (kept through a killing hook impact)

	VectorCopy( client->ps.origin, client->oldOrigin );
	VectorCopy( client->ps.velocity, client->oldVelocity );

#ifdef MISSIONPACK
		if (level.intermissionQueued != 0 && g_singlePlayer.integer) {
			if ( level.time - level.intermissionQueued >= 1000  ) {
				pm.cmd.buttons = 0;
				pm.cmd.forwardmove = 0;
				pm.cmd.rightmove = 0;
				pm.cmd.upmove = 0;
				if ( level.time - level.intermissionQueued >= 2000 && level.time - level.intermissionQueued <= 2500 ) {
					trap_SendConsoleCommand( EXEC_APPEND, "centerview\n");
				}
				ent->client->ps.pm_type = PM_SPINTERMISSION;
			}
		}
		Pmove (&pm);
#else
		Pmove (&pm);
#endif
	if ( client->ps.groundEntityNum != ENTITYNUM_NONE ) {
		client->groundTime = level.time;		// for the airborne medals
	}

	// save results of pmove
	if ( ent->client->ps.eventSequence != oldEventSequence ) {
		ent->eventTime = level.time;
	}
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qtrue );
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
	}
	SendPendingPredictableEvents( &ent->client->ps );

	if ( !( ent->client->ps.eFlags & EF_FIRING ) ) {
		client->fireHeld = qfalse;		// for grapple
	}

	// use the snapped origin for linking so it matches client predicted versions
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );

	VectorCopy (pm.mins, ent->r.mins);
	VectorCopy (pm.maxs, ent->r.maxs);

	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;

	// execute client events
	ClientEvents( ent, oldEventSequence );

	// link entity now, after any personal teleporters have been used
	trap_LinkEntity (ent);
	if ( !ent->client->noclip ) {
		G_TouchTriggers( ent );
	}

	// NOTE: now copy the exact origin over otherwise clients can be snapped into solid
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );

	//test for solid areas in the AAS file
	BotTestAAS(ent->r.currentOrigin);

	// touch other objects
	ClientImpacts( ent, &pm );

	// champion collisions and ability bookkeeping
	G_CollisionDamage( ent, &pm, impactSpeed, preVelocity );
	G_AbilityUpkeep( ent, msec );

	// save results of triggers and client events
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// check for respawning
	if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
		// clan arena: the dead wait for the next round; round duel only respawns mid-round
		if ( ( G_ClanArena() || ( G_RoundDuel() && G_RoundDuelHolds() ) ) && !level.warmupTime ) {
			return;
		}
		// wait for the attack button to be pressed
		if ( level.time > client->respawnTime ) {
			// forcerespawn is to prevent users from waiting out powerups
			if ( ( g_forcerespawn.integer > 0 && 
				( level.time - client->respawnTime ) > g_forcerespawn.integer * 1000 )
				|| ( G_MatchMode() && !G_Sacrifice() && level.time - client->respawnTime >= 300 )
				|| G_Sacrifice() ) {
				ClientRespawn( ent );
				return;
			}
		
			// pressing attack or use is the normal respawn method
			if ( ucmd->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) ) {
				ClientRespawn( ent );
			}
		}
		return;
	}

	// perform once-a-second actions
	ClientTimerActions( ent, msec );
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
void ClientThink( int clientNum ) {
	gentity_t *ent;

	ent = g_entities + clientNum;
	trap_GetUsercmd( clientNum, &ent->client->pers.cmd );

	// mark the time we got info, so we can display the
	// phone jack if they don't get any for a while
	ent->client->lastCmdTime = level.time;

	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		ClientThink_real( ent );
	}
}


void G_RunClient( gentity_t *ent ) {
	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		return;
	}
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink_real( ent );
}


/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame( gentity_t *ent ) {
	gclient_t	*cl;

	// if we are doing a chase cam or a remote view, grab the latest info
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		int		clientNum, flags;

		clientNum = ent->client->sess.spectatorClient;

		// team follow1 and team follow2 go to whatever clients are playing
		if ( clientNum == -1 ) {
			clientNum = level.follow1;
		} else if ( clientNum == -2 ) {
			clientNum = level.follow2;
		}
		if ( clientNum >= 0 ) {
			cl = &level.clients[ clientNum ];
			if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR ) {
				flags = (cl->ps.eFlags & ~(EF_VOTED | EF_TEAMVOTED)) | (ent->client->ps.eFlags & (EF_VOTED | EF_TEAMVOTED));
				ent->client->ps = cl->ps;
				ent->client->ps.pm_flags |= PMF_FOLLOW;
				ent->client->ps.eFlags = flags;
				return;
			}
		}

		if ( ent->client->ps.pm_flags & PMF_FOLLOW ) {
			// drop them to free spectators unless they are dedicated camera followers
			if ( ent->client->sess.spectatorClient >= 0 ) {
				ent->client->sess.spectatorState = SPECTATOR_FREE;
			}

			ClientBegin( ent->client - level.clients );
		}
	}

	if ( ent->client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		ent->client->ps.pm_flags |= PMF_SCOREBOARD;
	} else {
		ent->client->ps.pm_flags &= ~PMF_SCOREBOARD;
	}
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEdFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
void ClientEndFrame( gentity_t *ent ) {
	int			i;

	// the drone's HUD shows Strogg's own body: its health and his armour,
	// in STAT_DEAD_YAW while flying (health in the low byte, armour above)
	if ( ent->client->droneBody ) {
		int		h = ent->client->droneBody->health, a = ent->client->droneArmor;

		ent->client->ps.stats[STAT_DEAD_YAW] = ( h < 0 ? 0 : h > 255 ? 255 : h ) | ( ( a < 0 ? 0 : a > 255 ? 255 : a ) << 8 );
	}

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		SpectatorClientEndFrame( ent );
		return;
	}

	// turn off any expired powerups
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ent->client->ps.powerups[ i ] < level.time ) {
			ent->client->ps.powerups[ i ] = 0;
		}
	}

#ifdef MISSIONPACK
	// set powerup for player animation
	if( bg_itemlist[ent->client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
		ent->client->ps.powerups[PW_GUARD] = level.time;
	}
	if( bg_itemlist[ent->client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_SCOUT ) {
		ent->client->ps.powerups[PW_SCOUT] = level.time;
	}
	if( bg_itemlist[ent->client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_DOUBLER ) {
		ent->client->ps.powerups[PW_DOUBLER] = level.time;
	}
	if( bg_itemlist[ent->client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_AMMOREGEN ) {
		ent->client->ps.powerups[PW_AMMOREGEN] = level.time;
	}
	if ( ent->client->invulnerabilityTime > level.time ) {
		ent->client->ps.powerups[PW_INVULNERABILITY] = level.time;
	}
#endif

	// save network bandwidth
#if 0
	if ( !g_synchronousClients->integer && ent->client->ps.pm_type == PM_NORMAL ) {
		// FIXME: this must change eventually for non-sync demo recording
		VectorClear( ent->client->ps.viewangles );
	}
#endif

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if ( level.intermissiontime ) {
		return;
	}

	// burn from lava, etc
	P_WorldEffects (ent);

	// apply all the damage taken this frame
	P_DamageFeedback (ent);

	// add the EF_CONNECTION flag if we haven't gotten commands recently
	if ( level.time - ent->client->lastCmdTime > 1000 ) {
		ent->client->ps.eFlags |= EF_CONNECTION;
	} else {
		ent->client->ps.eFlags &= ~EF_CONNECTION;
	}

	ent->client->ps.stats[STAT_HEALTH] = ent->health;	// FIXME: get rid of ent->health...

	G_SetClientSound (ent);

	// set the latest infor
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qtrue );
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
	}
	SendPendingPredictableEvents( &ent->client->ps );

	// set the bit for the reachability area the client is currently in
//	i = trap_AAS_PointReachabilityAreaIndex( ent->client->ps.origin );
//	ent->client->areabits[i >> 3] |= 1 << (i & 7);
}


