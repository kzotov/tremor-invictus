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
// g_arcade.c -- arcade modes layered on free-for-all or team deathmatch
//
// g_instagib 1: everyone has the Railgun (and Gauntlet) with endless
// slugs, any rail hit kills, no items on the map.
// g_unholyTrinity 1: everyone starts with Rocket Launcher, Lightning Gun and
// Railgun at base health and full armour; only health and armour stay on the
// map; a point for every 100 damage to enemies on top of frags.

#include "g_local.h"

#define TRINITY_DAMAGE_PER_POINT	100

qboolean G_Instagib( void ) {
	return g_instagib.integer && ( g_gametype.integer == GT_FFA || g_gametype.integer == GT_TEAM );
}

qboolean G_UnholyTrinity( void ) {
	return g_unholyTrinity.integer && !G_Instagib()
		&& ( g_gametype.integer == GT_FFA || g_gametype.integer == GT_TEAM );
}

/*
=================
G_ArcadeKeepsItem

Whether a map item stays in the arcade mode running
=================
*/
qboolean G_ArcadeKeepsItem( const gitem_t *item ) {
	if ( G_Instagib() ) {
		return item->giType == IT_TEAM;
	}
	if ( G_UnholyTrinity() ) {
		return item->giType == IT_HEALTH || item->giType == IT_ARMOR || item->giType == IT_TEAM;
	}
	return qtrue;
}

/*
=================
G_ArcadeLoadout

The weapons and stack a player spawns with in the arcade mode running
=================
*/
void G_ArcadeLoadout( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( G_Instagib() ) {
		client->ps.stats[STAT_WEAPONS] = ( 1 << WP_GAUNTLET ) | ( 1 << WP_RAILGUN );
		client->ps.ammo[WP_RAILGUN] = 999;		// endless in practice; bots skip weapons at -1
		client->ps.weapon = WP_RAILGUN;
		client->ps.weaponstate = WEAPON_READY;
	} else if ( G_UnholyTrinity() ) {
		client->ps.stats[STAT_WEAPONS] = ( 1 << WP_GAUNTLET ) | ( 1 << WP_ROCKET_LAUNCHER )
			| ( 1 << WP_LIGHTNING ) | ( 1 << WP_RAILGUN );
		client->ps.ammo[WP_ROCKET_LAUNCHER] = BG_WeaponParam( WP_ROCKET_LAUNCHER, WPP_AMMOMAX );
		client->ps.ammo[WP_LIGHTNING] = BG_WeaponParam( WP_LIGHTNING, WPP_AMMOMAX );
		client->ps.ammo[WP_RAILGUN] = BG_WeaponParam( WP_RAILGUN, WPP_AMMOMAX );
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH];
		client->ps.stats[STAT_ARMOR] = G_MaxArmor( client );
		client->ps.weapon = WP_ROCKET_LAUNCHER;
		client->ps.weaponstate = WEAPON_READY;
	}
}

/*
=================
G_ArcadeDamage

Instagib: a rail hit kills. Returns the damage to apply.
=================
*/
int G_ArcadeDamage( gentity_t *targ, gentity_t *attacker, int damage, int mod ) {
	if ( G_Instagib() && mod == MOD_RAILGUN && targ->client && attacker && attacker != targ ) {
		return 1000;
	}
	return damage;
}

/*
=================
G_ArcadeDamageDealt

Unholy Trinity: a point for every 100 damage to enemies
=================
*/
void G_ArcadeDamageDealt( gentity_t *attacker, gentity_t *targ, int damage ) {
	if ( !G_UnholyTrinity() || level.warmupTime || !attacker || !attacker->client
		|| !targ->client || attacker == targ || OnSameTeam( attacker, targ ) ) {
		return;
	}
	attacker->client->pers.caDamage += damage;
	while ( attacker->client->pers.caDamage >= TRINITY_DAMAGE_PER_POINT ) {
		attacker->client->pers.caDamage -= TRINITY_DAMAGE_PER_POINT;
		AddScore( attacker, targ->r.currentOrigin, 1 );
	}
}
