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
// bg_misc.c -- both games misc functions, all completely stateless

#include "../qcommon/q_shared.h"
#include "bg_public.h"

/*QUAKED item_***** ( 0 0 0 ) (-16 -16 -16) (16 16 16) suspended
DO NOT USE THIS CLASS, IT JUST HOLDS GENERAL INFORMATION.
The suspended flag will allow items to hang in the air, otherwise they are dropped to the next surface.

If an item is the target of another entity, it will not spawn in until fired.

An item fires all of its targets when it is picked up.  If the toucher can't carry it, the targets won't be fired.

"notfree" if set to 1, don't spawn in free for all games
"notteam" if set to 1, don't spawn in team games
"notsingle" if set to 1, don't spawn in single player games
"wait"	override the default wait before respawning.  -1 = never respawn automatically, which can be used with targeted spawning.
"random" random number of plus or minus seconds varied from the respawn time
"count" override quantity or duration on most items.
*/

gitem_t	bg_itemlist[] = 
{
	{
		NULL,
		NULL,
		{ NULL,
		NULL,
		NULL, NULL} ,
/* icon */		NULL,
/* pickup */	NULL,
		0,
		0,
		0,
/* precache */ "",
/* sounds */ ""
	},	// leave index 0 alone

	//
	// ARMOR
	//

/*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_armor_shard", 
		"sound/misc/ar1_pkup.wav",
		{ "models/powerups/armor/shard.md3", 
		"models/powerups/armor/shard_sphere.md3",
		NULL, NULL} ,
/* icon */		"icons/iconr_shard",
/* pickup */	"Armor Shard",
		5,
		IT_ARMOR,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_armor_combat", 
		"sound/misc/ar2_pkup.wav",
        { "models/powerups/armor/armor_yel.md3",
		NULL, NULL, NULL},
/* icon */		"icons/iconr_yellow",
/* pickup */	"Armor",
		50,
		IT_ARMOR,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_armor_body", 
		"sound/misc/ar2_pkup.wav",
        { "models/powerups/armor/armor_red.md3",
		NULL, NULL, NULL},
/* icon */		"icons/iconr_red",
/* pickup */	"Heavy Armor",
		100,
		IT_ARMOR,
		0,
/* precache */ "",
/* sounds */ ""
	},

	//
	// health
	//
/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_health_small",
		"sound/items/s_health.wav",
        { "models/powerups/health/small_cross.md3", 
		"models/powerups/health/small_sphere.md3", 
		NULL, NULL },
/* icon */		"icons/iconh_green",
/* pickup */	"5 Health",
		5,
		IT_HEALTH,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_health",
		"sound/items/n_health.wav",
        { "models/powerups/health/medium_cross.md3", 
		"models/powerups/health/medium_sphere.md3", 
		NULL, NULL },
/* icon */		"icons/iconh_yellow",
/* pickup */	"25 Health",
		25,
		IT_HEALTH,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_health_large",
		"sound/items/l_health.wav",
        { "models/powerups/health/large_cross.md3", 
		"models/powerups/health/large_sphere.md3", 
		NULL, NULL },
/* icon */		"icons/iconh_red",
/* pickup */	"50 Health",
		50,
		IT_HEALTH,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_health_mega",
		"sound/items/m_health.wav",
        { "models/powerups/health/mega_cross.md3", 
		"models/powerups/health/mega_sphere.md3", 
		NULL, NULL },
/* icon */		"icons/iconh_mega",
/* pickup */	"Mega Health",
		100,
		IT_HEALTH,
		0,
/* precache */ "",
/* sounds */ ""
	},


	//
	// WEAPONS 
	//

/*QUAKED weapon_gauntlet (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_gauntlet", 
		"sound/misc/w_pkup.wav",
        { "models/weapons2/gauntlet/gauntlet.md3",
		NULL, NULL, NULL},
/* icon */		"icons/iconw_gauntlet",
/* pickup */	"Gauntlet",
		0,
		IT_WEAPON,
		WP_GAUNTLET,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_shotgun", 
		"sound/misc/w_pkup.wav",
        { "models/weapons2/shotgun/shotgun.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_shotgun",
/* pickup */	"Shotgun",
		10,
		IT_WEAPON,
		WP_SHOTGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_machinegun", 
		"sound/misc/w_pkup.wav",
        { "models/weapons2/machinegun/machinegun.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_machinegun",
/* pickup */	"Machinegun",
		40,
		IT_WEAPON,
		WP_MACHINEGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_grenadelauncher",
		"sound/misc/w_pkup.wav",
        { "models/weapons2/grenadel/grenadel.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_grenade",
/* pickup */	"Tri-bolt",
		10,
		IT_WEAPON,
		WP_GRENADE_LAUNCHER,
/* precache */ "",
/* sounds */ "sound/weapons/grenade/hgrenb1a.wav sound/weapons/grenade/hgrenb2a.wav"
	},

/*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_rocketlauncher",
		"sound/misc/w_pkup.wav",
        { "models/weapons2/rocketl/rocketl.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_rocket",
/* pickup */	"Rocket Launcher",
		10,
		IT_WEAPON,
		WP_ROCKET_LAUNCHER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_lightning", 
		"sound/misc/w_pkup.wav",
        { "models/weapons2/lightning/lightning.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_lightning",
/* pickup */	"Lightning Gun",
		100,
		IT_WEAPON,
		WP_LIGHTNING,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_railgun", 
		"sound/misc/w_pkup.wav",
        { "models/weapons2/railgun/railgun.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_railgun",
/* pickup */	"Railgun",
		10,
		IT_WEAPON,
		WP_RAILGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_plasmagun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_plasmagun", 
		"sound/misc/w_pkup.wav",
        { "models/weapons2/plasma/plasma.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_plasma",
/* pickup */	"Super Nailgun",
		50,
		IT_WEAPON,
		WP_PLASMAGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_bfg",
		"sound/misc/w_pkup.wav",
        { "models/weapons2/bfg/bfg.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_bfg",
/* pickup */	"BFG10K",
		20,
		IT_WEAPON,
		WP_BFG,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_grapplinghook (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_grapplinghook",
		"sound/misc/w_pkup.wav",
        { "models/weapons2/grapple/grapple.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_grapple",
/* pickup */	"Grappling Hook",
		0,
		IT_WEAPON,
		WP_GRAPPLING_HOOK,
/* precache */ "",
/* sounds */ ""
	},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_shells",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/shotgunam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_shotgun",
/* pickup */	"Shells",
		10,
		IT_AMMO,
		WP_SHOTGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_bullets",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/machinegunam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_machinegun",
/* pickup */	"Bullets",
		50,
		IT_AMMO,
		WP_MACHINEGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_grenades",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/grenadeam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_grenade",
/* pickup */	"Grenades",
		5,
		IT_AMMO,
		WP_GRENADE_LAUNCHER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_cells",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/plasmaam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_plasma",
/* pickup */	"Cells",
		30,
		IT_AMMO,
		WP_PLASMAGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_lightning (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_lightning",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/lightningam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_lightning",
/* pickup */	"Lightning",
		60,
		IT_AMMO,
		WP_LIGHTNING,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_rockets",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/rocketam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_rocket",
/* pickup */	"Rockets",
		5,
		IT_AMMO,
		WP_ROCKET_LAUNCHER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_slugs",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/railgunam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_railgun",
/* pickup */	"Slugs",
		10,
		IT_AMMO,
		WP_RAILGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_bfg",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/bfgam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_bfg",
/* pickup */	"Bfg Ammo",
		15,
		IT_AMMO,
		WP_BFG,
/* precache */ "",
/* sounds */ ""
	},

	//
	// HOLDABLE ITEMS
	//
/*QUAKED holdable_teleporter (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"holdable_teleporter", 
		"sound/items/holdable.wav",
        { "models/powerups/holdable/teleporter.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/teleporter",
/* pickup */	"Personal Teleporter",
		60,
		IT_HOLDABLE,
		HI_TELEPORTER,
/* precache */ "",
/* sounds */ ""
	},
/*QUAKED holdable_medkit (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"holdable_medkit", 
		"sound/items/holdable.wav",
        { 
		"models/powerups/holdable/medkit.md3", 
		"models/powerups/holdable/medkit_sphere.md3",
		NULL, NULL},
/* icon */		"icons/medkit",
/* pickup */	"Medkit",
		60,
		IT_HOLDABLE,
		HI_MEDKIT,
/* precache */ "",
/* sounds */ "sound/items/use_medkit.wav"
	},

	//
	// POWERUP ITEMS
	//
/*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_quad", 
		"sound/items/quaddamage.wav",
        { "models/powerups/instant/quad.md3", 
        "models/powerups/instant/quad_ring.md3",
		NULL, NULL },
/* icon */		"icons/quad",
/* pickup */	"Quad Damage",
		30,
		IT_POWERUP,
		PW_QUAD,
/* precache */ "",
/* sounds */ "sound/items/damage2.wav sound/items/damage3.wav"
	},

/*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_enviro",
		"sound/items/protect.wav",
        { "models/powerups/instant/enviro.md3", 
		"models/powerups/instant/enviro_ring.md3", 
		NULL, NULL },
/* icon */		"icons/envirosuit",
/* pickup */	"Battle Suit",
		30,
		IT_POWERUP,
		PW_BATTLESUIT,
/* precache */ "",
/* sounds */ "sound/items/airout.wav sound/items/protect3.wav"
	},

/*QUAKED item_haste (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_haste",
		"sound/items/haste.wav",
        { "models/powerups/instant/haste.md3", 
		"models/powerups/instant/haste_ring.md3", 
		NULL, NULL },
/* icon */		"icons/haste",
/* pickup */	"Speed",
		30,
		IT_POWERUP,
		PW_HASTE,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_invis (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_invis",
		"sound/items/invisibility.wav",
        { "models/powerups/instant/invis.md3", 
		"models/powerups/instant/invis_ring.md3", 
		NULL, NULL },
/* icon */		"icons/invis",
/* pickup */	"Invisibility",
		30,
		IT_POWERUP,
		PW_INVIS,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_regen",
		"sound/items/regeneration.wav",
        { "models/powerups/instant/regen.md3", 
		"models/powerups/instant/regen_ring.md3", 
		NULL, NULL },
/* icon */		"icons/regen",
/* pickup */	"Regeneration",
		30,
		IT_POWERUP,
		PW_REGEN,
/* precache */ "",
/* sounds */ "sound/items/regen.wav"
	},

/*QUAKED item_flight (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"item_flight",
		"sound/items/flight.wav",
        { "models/powerups/instant/flight.md3", 
		"models/powerups/instant/flight_ring.md3", 
		NULL, NULL },
/* icon */		"icons/flight",
/* pickup */	"Flight",
		60,
		IT_POWERUP,
		PW_FLIGHT,
/* precache */ "",
/* sounds */ "sound/items/flight.wav"
	},

/*QUAKED team_CTF_redflag (1 0 0) (-16 -16 -16) (16 16 16)
Only in CTF games
*/
	{
		"team_CTF_redflag",
		NULL,
        { "models/flags/r_flag.md3",
		NULL, NULL, NULL },
/* icon */		"icons/iconf_red1",
/* pickup */	"Red Flag",
		0,
		IT_TEAM,
		PW_REDFLAG,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED team_CTF_blueflag (0 0 1) (-16 -16 -16) (16 16 16)
Only in CTF games
*/
	{
		"team_CTF_blueflag",
		NULL,
        { "models/flags/b_flag.md3",
		NULL, NULL, NULL },
/* icon */		"icons/iconf_blu1",
/* pickup */	"Blue Flag",
		0,
		IT_TEAM,
		PW_BLUEFLAG,
/* precache */ "",
/* sounds */ ""
	},

#ifdef MISSIONPACK
/*QUAKED holdable_kamikaze (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"holdable_kamikaze", 
		"sound/items/holdable.wav",
        { "models/powerups/kamikazi.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/kamikaze",
/* pickup */	"Kamikaze",
		60,
		IT_HOLDABLE,
		HI_KAMIKAZE,
/* precache */ "",
/* sounds */ "sound/items/kamikazerespawn.wav"
	},

/*QUAKED holdable_portal (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"holdable_portal", 
		"sound/items/holdable.wav",
        { "models/powerups/holdable/porter.md3",
		NULL, NULL, NULL},
/* icon */		"icons/portal",
/* pickup */	"Portal",
		60,
		IT_HOLDABLE,
		HI_PORTAL,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED holdable_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"holdable_invulnerability", 
		"sound/items/holdable.wav",
        { "models/powerups/holdable/invulnerability.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/invulnerability",
/* pickup */	"Invulnerability",
		60,
		IT_HOLDABLE,
		HI_INVULNERABILITY,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_nails (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_nails",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/nailgunam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_nailgun",
/* pickup */	"Nails",
		20,
		IT_AMMO,
		WP_NAILGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_mines (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_mines",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/proxmineam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_proxlauncher",
/* pickup */	"Proximity Mines",
		10,
		IT_AMMO,
		WP_PROX_LAUNCHER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED ammo_belt (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"ammo_belt",
		"sound/misc/am_pkup.wav",
        { "models/powerups/ammo/chaingunam.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/icona_chaingun",
/* pickup */	"Chaingun Belt",
		100,
		IT_AMMO,
		WP_CHAINGUN,
/* precache */ "",
/* sounds */ ""
	},

	//
	// PERSISTANT POWERUP ITEMS
	//
/*QUAKED item_scout (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
*/
	{
		"item_scout",
		"sound/items/scout.wav",
        { "models/powerups/scout.md3", 
		NULL, NULL, NULL },
/* icon */		"icons/scout",
/* pickup */	"Scout",
		30,
		IT_PERSISTANT_POWERUP,
		PW_SCOUT,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_guard (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
*/
	{
		"item_guard",
		"sound/items/guard.wav",
        { "models/powerups/guard.md3", 
		NULL, NULL, NULL },
/* icon */		"icons/guard",
/* pickup */	"Guard",
		30,
		IT_PERSISTANT_POWERUP,
		PW_GUARD,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_doubler (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
*/
	{
		"item_doubler",
		"sound/items/doubler.wav",
        { "models/powerups/doubler.md3", 
		NULL, NULL, NULL },
/* icon */		"icons/doubler",
/* pickup */	"Doubler",
		30,
		IT_PERSISTANT_POWERUP,
		PW_DOUBLER,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_doubler (.3 .3 1) (-16 -16 -16) (16 16 16) suspended redTeam blueTeam
*/
	{
		"item_ammoregen",
		"sound/items/ammoregen.wav",
        { "models/powerups/ammo.md3",
		NULL, NULL, NULL },
/* icon */		"icons/ammo_regen",
/* pickup */	"Ammo Regen",
		30,
		IT_PERSISTANT_POWERUP,
		PW_AMMOREGEN,
/* precache */ "",
/* sounds */ ""
	},

	/*QUAKED team_CTF_neutralflag (0 0 1) (-16 -16 -16) (16 16 16)
Only in One Flag CTF games
*/
	{
		"team_CTF_neutralflag",
		NULL,
        { "models/flags/n_flag.md3",
		NULL, NULL, NULL },
/* icon */		"icons/iconf_neutral1",
/* pickup */	"Neutral Flag",
		0,
		IT_TEAM,
		PW_NEUTRALFLAG,
/* precache */ "",
/* sounds */ ""
	},

	{
		"item_redcube",
		"sound/misc/am_pkup.wav",
        { "models/powerups/orb/r_orb.md3",
		NULL, NULL, NULL },
/* icon */		"icons/iconh_rorb",
/* pickup */	"Red Cube",
		0,
		IT_TEAM,
		0,
/* precache */ "",
/* sounds */ ""
	},

	{
		"item_bluecube",
		"sound/misc/am_pkup.wav",
        { "models/powerups/orb/b_orb.md3",
		NULL, NULL, NULL },
/* icon */		"icons/iconh_borb",
/* pickup */	"Blue Cube",
		0,
		IT_TEAM,
		0,
/* precache */ "",
/* sounds */ ""
	},
/*QUAKED weapon_nailgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_nailgun", 
		"sound/misc/w_pkup.wav",
        { "models/weapons/nailgun/nailgun.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_nailgun",
/* pickup */	"Nailgun",
		10,
		IT_WEAPON,
		WP_NAILGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_prox_launcher (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_prox_launcher", 
		"sound/misc/w_pkup.wav",
        { "models/weapons/proxmine/proxmine.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_proxlauncher",
/* pickup */	"Prox Launcher",
		5,
		IT_WEAPON,
		WP_PROX_LAUNCHER,
/* precache */ "",
/* sounds */ "sound/weapons/proxmine/wstbtick.wav "
			"sound/weapons/proxmine/wstbactv.wav "
			"sound/weapons/proxmine/wstbimpl.wav "
			"sound/weapons/proxmine/wstbimpm.wav "
			"sound/weapons/proxmine/wstbimpd.wav "
			"sound/weapons/proxmine/wstbactv.wav"
	},

/*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_chaingun", 
		"sound/misc/w_pkup.wav",
        { "models/weapons/vulcan/vulcan.md3", 
		NULL, NULL, NULL},
/* icon */		"icons/iconw_chaingun",
/* pickup */	"Chaingun",
		80,
		IT_WEAPON,
		WP_CHAINGUN,
/* precache */ "",
/* sounds */ "sound/weapons/vulcan/wvulwind.wav"
	},
#endif

/*QUAKED weapon_hmg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_hmg",
		"sound/misc/w_pkup.wav",
        { "models/weapons2/machinegun/machinegun.md3",
		NULL, NULL, NULL},
/* icon */		"icons/iconw_machinegun",
/* pickup */	"Heavy Machinegun",
		100,
		IT_WEAPON,
		WP_HEAVY_MACHINEGUN,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED weapon_ssg (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
*/
	{
		"weapon_ssg",
		"sound/misc/w_pkup.wav",
        { "models/weapons2/shotgun/shotgun.md3",
		NULL, NULL, NULL},
/* icon */		"icons/iconw_shotgun",
/* pickup */	"Super Shotgun",
		10,
		IT_WEAPON,
		WP_SUPER_SHOTGUN,
/* precache */ "",
/* sounds */ ""
	},

#ifndef MISSIONPACK
/*QUAKED weapon_nailgun (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
a starting weapon; not placed on maps
*/
	{
		"weapon_nailgun",
		"sound/misc/w_pkup.wav",
        { "models/weapons2/plasma/plasma.md3",
		NULL, NULL, NULL},
/* icon */		"icons/iconw_plasma",
/* pickup */	"Nailgun",
		100,
		IT_WEAPON,
		WP_NAILGUN,
/* precache */ "",
/* sounds */ ""
	},
#endif

/*QUAKED item_hourglass (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
takes a tenth of the full wait off the ability's recharge
*/
	{
		"item_hourglass",
		"sound/items/holdable.wav",
		{ "models/powerups/holdable/teleporter.md3",
		NULL, NULL, NULL},
/* icon */		"icons/teleporter",
/* pickup */	"Hourglass",
		10,
		IT_HOURGLASS,
		0,
/* precache */ "",
/* sounds */ ""
	},

/*QUAKED item_ammo_box (.3 .3 1) (-16 -16 -16) (16 16 16) suspended
ammo for every carried weapon
*/
	{
		"item_ammo_box",
		"sound/misc/am_pkup.wav",
		{ "models/powerups/ammo/bfgam.md3",
		NULL, NULL, NULL},
/* icon */		"icons/icona_bfg",
/* pickup */	"Ammo Box",
		0,
		IT_AMMO,
		WP_NONE,
/* precache */ "",
/* sounds */ ""
	},

	// end of list marker
	{NULL}
};

int		bg_numItems = ARRAY_LEN( bg_itemlist ) - 1;


/*
==============================================================================

CHAMPIONS

Body class, run speed, jump speed cap and movement style per champion.
Retune here; the global movement numbers are cvars (see bg_pmove.c).

Player models are stand-ins from the player's own Quake III Arena install
(models/players/<model>), never shipped. Swap the model column for free
models in a standalone build.
klesk, xaero, sarge and hunter are kept for the hidden champions; don't use
them as stand-ins.

==============================================================================
*/

// health counts down to max health (100 with no handicap) for every class
const classInfo_t bg_classInfo[CLASS_NUM_CLASSES] = {
	// half-width, base armor, mega health bonus, max armor, spawn armor, spawn armor in duel
	{ 18, 75, 75, 150, 25, 0 },		// CLASS_LIGHT (QC's 36 unit box)
	{ 18, 100, 75, 175, 50, 25 },	// CLASS_MEDIUM
	{ 20, 125, 75, 200, 75, 50 }	// CLASS_HEAVY (40 units)
};

const champion_t bg_championList[] = {
	// name, body class, run speed, jump speed cap, style, movement abilities, active ability, passives,
	// player model, display name
	{ "ranger", CLASS_MEDIUM, 320, 900, MOVE_STANDARD, 0, ACTIVE_ORB, PASSIVE_SELFDAMAGE, "ranger", STR_CHAMPION_RANGER },	// default, must stay first
	{ "anarki", CLASS_LIGHT, 320, 620, MOVE_AIRCONTROL, 0, ACTIVE_INJECTION, 0, "anarki", STR_CHAMPION_ANARKI },
	{ "athena", CLASS_LIGHT, 320, 750, MOVE_STANDARD, MOVEABIL_RAMPJUMP, ACTIVE_HOOK, 0, "crash", STR_CHAMPION_ATHENA },
	{ "nyx", CLASS_LIGHT, 320, 1000, MOVE_STANDARD, MOVEABIL_WALLJUMP | MOVEABIL_EASYSTRAFE, ACTIVE_GHOSTWALK, 0, "mynx", STR_CHAMPION_NYX },
	{ "slash", CLASS_LIGHT, 320, 700, MOVE_SLIDE, 0, ACTIVE_PLASMATRAIL, 0, "slash", STR_CHAMPION_SLASH },
	{ "bj", CLASS_MEDIUM, 320, 900, MOVE_STANDARD, 0, ACTIVE_DUALWIELD, PASSIVE_REGENERATION, "bitterman", STR_CHAMPION_BJ },
	{ "deathknight", CLASS_MEDIUM, 320, 900, MOVE_STANDARD, 0, ACTIVE_FLAMESTRIKE, PASSIVE_FIREPROOF | PASSIVE_CHARRING, "bones", STR_CHAMPION_DEATHKNIGHT },
	{ "doomslayer", CLASS_MEDIUM, 320, 700, MOVE_STANDARD, MOVEABIL_DOUBLEJUMP, ACTIVE_BERSERK, 0, "doom", STR_CHAMPION_DOOMSLAYER },
	{ "eisen", CLASS_MEDIUM, 320, 700, MOVE_STANDARD, 0, ACTIVE_TURRET, PASSIVE_ARMORCOOLDOWN | PASSIVE_ARMORREGEN, "biker", STR_CHAMPION_EISEN },
	{ "galena", CLASS_MEDIUM, 320, 900, MOVE_STANDARD, 0, ACTIVE_TOTEM, PASSIVE_HEALTHCOOLDOWN | PASSIVE_SLOWDECAY, "lucy", STR_CHAMPION_GALENA },
	{ "strogg", CLASS_MEDIUM, 320, 650, MOVE_SLIDE, 0, ACTIVE_DRONE, PASSIVE_STROYENT, "grunt", STR_CHAMPION_STROGG },
	{ "visor", CLASS_MEDIUM, 320, 0, MOVE_STANDARD, 0, ACTIVE_PIERCINGSIGHT, PASSIVE_FULLSPRINT, "visor", STR_CHAMPION_VISOR },
	{ "clutch", CLASS_HEAVY, 320, 600, MOVE_STANDARD, MOVEABIL_AIRDODGE | MOVEABIL_ACCELERATION, ACTIVE_BARRIER, 0, "tankjr", STR_CHAMPION_CLUTCH },
	{ "keel", CLASS_HEAVY, 320, 800, MOVE_STANDARD, 0, ACTIVE_GRENADES, PASSIVE_AMMOCOOLDOWN | PASSIVE_SPLASHRESIST, "keel", STR_CHAMPION_KEEL },
	{ "scalebearer", CLASS_HEAVY, 320, 800, MOVE_STANDARD, MOVEABIL_HEAVYWEIGHT | MOVEABIL_ACCELERATION | MOVEABIL_EASYSTRAFE, ACTIVE_BULLRUSH, 0, "razor", STR_CHAMPION_SCALEBEARER },
	{ "sorlag", CLASS_HEAVY, 320, 600, MOVE_AIRCONTROL, MOVEABIL_BUNNYHOP, ACTIVE_ACIDSPIT, PASSIVE_ACIDPROOF, "sorlag", STR_CHAMPION_SORLAG },
	{ "wrack", CLASS_MEDIUM, 320, 560, MOVE_STANDARD, MOVEABIL_ISOAIR, ACTIVE_SHELLSHOCK, PASSIVE_OLDSOLDIER, "ranger/wrack", STR_CHAMPION_WRACK },
	{ "sarge", CLASS_MEDIUM, 320, 1000, MOVE_STANDARD, 0, ACTIVE_SUPPLYDROP, PASSIVE_DRILLSERGEANT, "sarge", STR_CHAMPION_SARGE },
	{ "klesk", CLASS_LIGHT, 320, 1000, MOVE_STANDARD, MOVEABIL_WALLJUMP, ACTIVE_SPINEVOLLEY, PASSIVE_HIVEREGEN, "klesk", STR_CHAMPION_KLESK },
	{ "hunter", CLASS_LIGHT, 320, 1000, MOVE_STANDARD, 0, ACTIVE_MARKPREY, PASSIVE_LIGHTFEET, "hunter", STR_CHAMPION_HUNTER },
	{ "xaero", CLASS_HEAVY, 320, 1000, MOVE_STANDARD, 0, ACTIVE_ETERNAL, PASSIVE_UNDYING, "xaero", STR_CHAMPION_XAERO },
	{ "raine", CLASS_LIGHT, 320, 1000, MOVE_STANDARD, 0, ACTIVE_DOWNPOUR, PASSIVE_SLIPSTREAM, "major", STR_CHAMPION_RAINE },
	{ "beastmaster", CLASS_HEAVY, 320, 1000, MOVE_STANDARD, 0, ACTIVE_HOUND, PASSIVE_THICKHIDE, "uriel", STR_CHAMPION_BEASTMASTER },
	{ "unumrey", CLASS_MEDIUM, 320, 1000, MOVE_STANDARD, 0, ACTIVE_RIFTANCHOR, PASSIVE_ECHO, "orbb", STR_CHAMPION_UNUMREY }
};

const int bg_numChampions = ARRAY_LEN( bg_championList );

/*
==============================================================================

STRINGS

English is the only language for now. A translation is another table in
the same order, chosen by a language cvar.

==============================================================================
*/

static const char *bg_stringsEnglish[STR_NUM_STRINGS] = {
	"",

	"Ranger",
	"Anarki",
	"Athena",
	"Nyx",
	"Slash",
	"B.J. Blazkowicz",
	"Death Knight",
	"Doom Slayer",
	"Eisen",
	"Galena",
	"Strogg & Peeker",
	"Visor",
	"Clutch",
	"Keel",
	"Scalebearer",
	"Sorlag",

	"Light",
	"Medium",
	"Heavy",

	"HOOK",
	"BULL RUSH",
	"INJECTION",
	"DUAL WIELD",
	"BARRIER",
	"FLAME STRIKE",
	"BERSERK",
	"SENTRY TURRET",
	"UNHOLY TOTEM",
	"GRENADE SWARM",
	"GHOST WALK",
	"DIRE ORB",
	"PLASMA TRAIL",
	"ACID SPIT",
	"DRONE STRIKE",
	"PIERCING SIGHT",
	"next in %i s",

	"CHAMPION",
	"CHAMPION",
	"Takes effect when you next spawn",
	"BACK",
	"PLAYER COLORS",
	"PLAYER COLORS",
	"Enemy",
	"Team",
	"Skin:",
	"Head:",
	"Upper:",
	"Lower:",
	"Same for all:",
	"Off",
	"Bright",
	"Sport",
	"Green",
	"Cyan",
	"Red",
	"Blue",
	"Yellow",
	"Orange",
	"Magenta",
	"Purple",
	"Pink",
	"White",
	"Lime",
	"Teal",

	"%s in %i seconds",
	"was trampled by",
	"was blasted by the orb of",
	"was scorched by",
	"was burned by",
	"was dissolved by",
	"stepped on the totem of",
	"was gunned down by the turret of",
	"was cut down by the laser of",
	"walked into the plasma trail of",
	"was shot down by the drone of",

	"%s is ready",
	"%s is not ready",
	"Warmup: type readyup when you are ready",
	"Overtime: %i minutes",
	"Sudden death: the next frag wins",
	"The match was stopped; back to warmup",
	"Champion locked until the match ends; your pick applies next match",
	"Half time: red %i, blue %i. Teams swap bases",
	"Second half",
	"Round %i",
	"Fight!",
	"Red wins the round",
	"Blue wins the round",
	"The round is a draw",
	"The soul appears in %i seconds",
	"The soul has appeared",
	"%s planted the soul",
	"%s extracted the soul",
	"The soul was dropped",
	"The soul returned",
	"Red completes the sacrifice",
	"Blue completes the sacrifice",
	"Sudden death: the next team to sacrifice wins",
	"Red %i%%   Blue %i%%",
	"Soul: not yet",
	"Soul: %s",
	"Soul: on the ground",
	"Soul: at red's obelisk",
	"Soul: at blue's obelisk",
	"Extracting %i%%",
	"Your pick: type pick <champion> (%i s)",
	"%s picks %s",
	"Round %i",
	"%s wins the round",
	"Sudden death: the next frag wins the round",
	"Your next champion: type next 1, 2 or 3",

	"PLAY",
	"CHAMPIONS",
	"PLAYER COLORS",
	"SETTINGS",
	"DEMOS",
	"QUIT",
	"QUIT THE GAME?",
	"Name:",
	"Free software under the GPL. Game data loads from your Quake III install.",
	"PRACTICE",
	"FIND SERVERS",
	"CREATE SERVER",
	"RESUME",
	"TEAM",
	"VOTE",
	"ADD BOTS",
	"REMOVE BOTS",
	"LEAVE MATCH",
	"Duel",
	"Team Deathmatch",
	"Capture the Flag",
	"Clan Arena",
	"Sacrifice",
	"Round Duel",
	"Free for All",
	"Loading",
	"VOTE FOR THE NEXT MAP",
	"Press 1, 2 or 3 to vote; you can change your vote",
	"Voting ends in %i seconds",
	"Votes: %i",
	"You can call one vote every 30 seconds.",
	"Votes: map <name>, nextmap, map_restart, kick <player>, shuffle, teamsize <n>, timelimit <min>, fraglimit <n>, capturelimit <n>, roundlimit <n>, mode <ffa|duel|tdm|ctf|ca|sacrifice|roundduel>, cointoss (the server may allow fewer).",
	"No such map on the server.",
	"Only in team modes.",
	"Modes: ffa, duel, tdm, ctf, ca, sacrifice, roundduel.",
	"That team is full.",
	"Coin toss: heads",
	"Coin toss: tails",
	"That vote needs a number from 0 to 999.",
	"VOTE(%is): %s  Yes(%s): %i  No(%s): %i",
	"or press Esc and choose Vote",
	"An admin muted your chat.",
	"Your chat is back on.",
	"Banned by an admin",
	"You have no admin rights on this server.",
	"That team is locked.",
	"TEAM ORDERS",
	"TIP",
	"Major items come back on a fixed timer: count from the moment someone takes them.",
	"Heavy Armor and Mega Health can take you over your maximum; the extra slowly drains away.",
	"Armour shards drop where a player dies, and they can push your armour over its maximum.",
	"A rocket at your feet throws you higher than a jump; it costs a little health.",
	"Your ability recharges over time; Hourglass pickups cut what is left of the wait.",
	"Strafe jumping builds speed: hold forward and a side key, and turn smoothly in the air.",
	"The Railgun hums while you hold it. Others can hear you coming.",
	"Hits register on the server; within a small window, where you aimed is what counts.",
	"Quad Damage and Protection appear on a timer; the announcer warns everyone 10 seconds before.",
	"Holding Protection, you can rocket jump almost for free.",
	"Heavy champions are wider targets but carry more armour.",
	"Switch weapons before a fight, not during it: a switch takes a moment.",
	"Bind your ability to a key you can press without letting go of movement.",
	"Control the Mega Health and Heavy Armor timings and your opponent will run out of stack first.",
	"Tied at the end? Overtime keeps adding two minutes until someone leads.",
	"Stay near your team: a trade is better than a lonely death.",
	"Power-up carriers glow for their teammates; follow and protect them.",
	"Your flag must be home to score a capture.",
	"A dropped flag goes home on its own after 30 seconds, or at once if a teammate touches it.",
	"No items and no respawns: everyone starts the round fully loaded. Use your rockets to move.",
	"Your own rockets don't hurt you in Clan Arena.",
	"Plant the soul at your obelisk; every second it rests there fills your team's meter.",
	"Stand at the enemy obelisk with no defender in sight to pull the soul out.",
	"Draft three champions that cover each other's weaknesses.",
	"When one champion falls you pick the next: answer what your opponent is playing.",
	"Throw the Dire Orb, then teleport to it, or onto an enemy it touches.",
	"Injection heals you over your maximum and clears burning and acid.",
	"The hook lets go by itself when you arrive; hold the button to swing instead.",
	"Jump off the top of a ramp or step to launch much higher.",
	"Ghost Walk makes you untouchable for a moment, but you can't shoot.",
	"Jump off walls to change direction in the air.",
	"Land while holding crouch to start a slide; falling charges it.",
	"Lay a Plasma Trail behind you, then press again to blow it up.",
	"Dual Wield makes your weapons fire much faster for a few seconds.",
	"Flame Strike leaves fire on the ground: block a doorway with it.",
	"You get a second jump in the air.",
	"Berserk turns your fists into one-punch weapons for a short time.",
	"Place the turret where you hold an item; it watches while you fight elsewhere.",
	"Totems heal teammates and burst on enemies who walk into them.",
	"Every kill drops a cell that heals you.",
	"Piercing Sight shows enemies through walls for a few seconds.",
	"Dodge in the air or on the ground; you have three charges that refill slowly.",
	"The longer you run straight, the faster you go.",
	"Your grenades bounce once and explode on the second hit.",
	"Bull Rush hits harder the faster you are moving.",
	"Keep hopping in one direction to build speed.",
	"Acid pools hurt anyone who walks through them.",
	"Shell Shock stops enemy abilities inside the zone and turns their cooldowns back.",
	"Drop a supply crate for armour and ammo; enemies can destroy it.",
	"Spine hits slow the target down.",
	"A marked target is visible to your whole team and takes more damage.",
	"Eternal Champion hits harder but makes you take more damage too.",
	"Call a storm on enemies who stand still.",
	"The hound hunts the nearest enemy you can see.",
	"Leave an anchor before a fight; recall to it when things go wrong.",
	"First Blood",
	"Midair",
	"Airborne",
	"Combo Kill",
	"Collateral Damage",
	"Double Whammy",
	"Denied",
	"Revenge",
	"Party Breaker",
	"Perforated",
	"Point Blank",
	"Precise",
	"Ring Out",
	"Stopwatch",
	"Tank",
	"Die Hard",
	"From the Grave",
	"Kamikaze",
	"Showstopper",
	"Triple Kill",
	"Fighter",
	"Avatar of Death",
	"Killing Spree",
	"Rage",
	"Frenzy",
	"Rampage",
	"Dominating",
	"Powerup Massacre",
	"Gauntlet God",
	"Machinegun God",
	"Shotgun God",
	"Nailgun God",
	"Tri-bolt God",
	"Rocket God",
	"Lightning God",
	"Rail God",
	"Damage Dealer",
	"Net Master",
	"Assistant",
	"PAUSED",
	"Space play   Up/Down speed   Left/Right 10 s   PgUp/PgDn 1 min   Home restart   H hide HUD   Esc leave",
	"REC",
	"Wrack",
	"Klesk",
	"Xaero",
	"Sarge",
	"Hunter",
	"Raine",
	"Beastmaster",
	"Unumrey",
	"SHELL SHOCK",
	"SPINE VOLLEY",
	"ETERNAL CHAMPION",
	"SUPPLY DROP",
	"MARK PREY",
	"DOWNPOUR",
	"RELEASE THE HOUND",
	"RIFT ANCHOR",
	"was pierced by the spines of",
	"was caught in the storm of",
	"was mauled by the hound of",
	"VIDEO",
	"MOUSE",
	"CONTROLS",
	"GAME",
	"SOUND",
	"DEFAULTS",
	"APPLY",
	"CREDITS",
	"BACK",
	"Some changes apply after a restart: press Apply",
	"Off",
	"On",
	"Unlimited",
	"Desktop",
	"Renderer",
	"Metal is the macOS default; OpenGL is the fallback. Applies after a restart.",
	"Resolution",
	"Desktop uses the display's own size. Applies after a restart.",
	"Display",
	"Fullscreen, a window, or a borderless window. Applies after a restart.",
	"Fullscreen",
	"Windowed",
	"Borderless",
	"Vertical sync",
	"Waits for the display's refresh; adds input delay. Applies after a restart.",
	"Frame cap",
	"Most frames drawn per second.",
	"Field of view",
	"Horizontal field of view at 4:3; wider screens see more at the sides.",
	"Zoom field of view",
	"Field of view while zoomed.",
	"Brightness",
	"Overall brightness of the picture.",
	"Texture detail",
	"Lower detail uses smaller textures. Applies after a restart.",
	"High",
	"Medium",
	"Low",
	"Lowest",
	"Texture filter",
	"How textures are smoothed at a distance.",
	"Bilinear",
	"Trilinear",
	"Sensitivity",
	"Turn speed. With mouse CPI set, this is degrees per centimetre of mouse travel.",
	"Mouse CPI",
	"Your mouse's counts per inch; 0 turns this off.",
	"Turn per cm",
	"How far you turn per centimetre, to match other games.",
	"%.2f degrees per cm, %.1f cm per 360",
	"set mouse CPI to see this",
	"Raw input",
	"Reads the mouse directly, without the system's acceleration.",
	"Acceleration",
	"0 turns acceleration off.",
	"Acceleration offset",
	"Mouse speed where acceleration starts.",
	"Acceleration power",
	"2 is linear; higher grows faster.",
	"Sensitivity cap",
	"Highest sensitivity acceleration can reach; 0 is no cap.",
	"Invert mouse",
	"Moving the mouse forward looks down.",
	"Smoothing",
	"Averages the last two mouse readings; adds delay.",
	"Enter or click, then press a key. Backspace clears the keys.",
	"press a key, Esc to cancel",
	"Forward",
	"Back",
	"Left",
	"Right",
	"Jump",
	"Crouch",
	"Walk",
	"Attack",
	"Zoom",
	"Next weapon",
	"Previous weapon",
	"Weapon %i",
	"Ability",
	"Chat",
	"Team chat",
	"Voice chat",
	"Follow next",
	"Follow previous",
	"Scores",
	"Screenshot",
	"Console",
	"Chat history",
	"Auto-switch on pickup",
	"Switch to a better weapon when you pick it up.",
	"Auto-hop",
	"Holding jump jumps again on landing.",
	"Crosshair",
	"Crosshair shape.",
	"Crosshair size",
	"Crosshair size in screen units.",
	"Ability arc",
	"Where the ability cooldown arc sits around the crosshair.",
	"Top",
	"Bottom",
	"Left",
	"Right",
	"HUD",
	"The new HUD or the classic one.",
	"Classic",
	"New",
	"Kill feed rows",
	"How many kills the feed shows at once.",
	"Team overlay",
	"Teammates' health, armour and place in team modes.",
	"FPS counter",
	"Frames per second in a corner.",
	"Lagometer",
	"Network graph in a corner.",
	"Time nudge",
	"Draws others closer to now; below 0 adds extrapolation.",
	"Projectile nudge",
	"Moves others' projectiles forward by your ping.",
	"Record every match",
	"Records a demo of each match, and a screenshot at the end.",
	"Demo",
	"Demo and screenshot",
	"Master volume",
	"Loudness of everything.",
	"Music volume",
	"Loudness of music.",
	"Voice chat",
	"Talk to other players with a microphone.",
	"Metal",
	"OpenGL 1",
	"OpenGL 2",
	"Fires a hook that pulls you to where it lands; three charges.",
	"Charges forward, knocking back and hurting anyone in the way.",
	"Heals over your maximum health at once.",
	"Fires a second copy of your weapon for a few seconds.",
	"Raises a shield in front of you that stops shots, then fires a laser.",
	"Throws fireballs that burst and set the ground burning.",
	"Fists only: fast, heavy punches and healing on each kill.",
	"Places a sentry gun that fires at enemies it sees.",
	"Throws a totem that heals you and harms enemies who touch it.",
	"Throws a grenade that bounces, then bursts; three charges.",
	"Turns you invisible and untouchable for a short time.",
	"Throws an orb; press again to teleport to it.",
	"Leaves a trail behind you; press again to set it off.",
	"Spits acid that leaves burning pools on the ground.",
	"Sends a drone that flies ahead and bursts on command.",
	"Shows enemies through walls for a few seconds.",
	"Makes a zone where enemies' abilities stop and can't start.",
	"Fires a fan of spines that hurt and slow enemies.",
	"For a while you deal more damage and take more.",
	"Drops a crate of armour and ammo for your team.",
	"Fires a dart; the enemy it hits shows through walls.",
	"Calls a storm down on a spot, hurting anyone inside.",
	"Releases a hound that hunts the nearest enemy.",
	"Sets an anchor; press again to return to it.",
	"Health refills to the top of its 25-point block.",
	"Takes less damage from own blasts.",
	"Takes less splash damage.",
	"Fire and lava do no harm.",
	"Acid and slime do no harm.",
	"Armour pickups shorten the ability's wait.",
	"Health pickups shorten the ability's wait.",
	"Ammo pickups shorten the ability's wait.",
	"Keeps more speed in the air.",
	"Melee hits set the target burning.",
	"Kills drop cells that heal this champion.",
	"Own blasts hurt less and push harder.",
	"Health comes back to base after a quiet while.",
	"Once a life, a killing hit leaves 1 health.",
	"Switches weapons faster.",
	"No falling damage; silent steps and landings.",
	"Turns faster in the air.",
	"Takes less splash damage from others.",
	"Part of each hit taken heals back.",
	"Armour regrows to 50 after a quiet while.",
	"Health over the maximum wears off at half speed.",
	"Health",
	"Armour",
	"Run speed",
	"Movement",
	"Ability",
	"Passive",
	"Starting weapon",
	"SELECT",
	"current champion",
	"%i charges, %i s each",
	"every %i s",
	"%i base, %i max",
	"Standard",
	"Air control",
	"Crouch slide",
	"wall jump",
	"double jump",
	"dodge",
	"speeds up running straight",
	"speed hurts on impact",
	"ramp jump",
	"bunny hop",
	"easy strafe jump",
	"steers in the air",
	"Machinegun",
	"Shotgun",
	"Nailgun",
	"Mode",
	"Opponents",
	"Teammates",
	"Bot skill",
	"Time limit, minutes",
	"Frag limit",
	"Half length, minutes",
	"Capture limit",
	"Rounds to win",
	"START",
	"page %i of %i",
	"No maps support this mode",
	"0 is no limit",
	"DEMOS",
	"PLAY",
	"RENAME",
	"DELETE",
	"OK",
	"Date",
	"Map",
	"Mode",
	"Length",
	"Players",
	"No demos yet. Matches are recorded to demos/ when Record every match is on.",
	"made with an older version",
	"DELETE THIS DEMO?",
	"New name",
	"Letters, digits and _-() only; Enter renames, Esc cancels.",
	"%i demos",
	"CREDITS",
	"Tremor Invictus",
	"Made by the Tremor Invictus contributors",
	"github.com/kzotov/tremor-invictus",
	"Built on ioquake3 and the Quake III Arena source code",
	"Quake III Arena source code (c) 1999-2005 id Software, Inc.",
	"This program is free software under the GNU General Public",
	"License, version 2 or any later version, with NO WARRANTY.",
	"Logo lettering: Cinzel by Natanael Gama, SIL Open Font License 1.1",
	"Third-party libraries and their licences",
	"SDL2 and zlib: zlib licence      libjpeg: IJG licence",
	"libcurl: curl licence      OpenAL Soft: GNU LGPL",
	"Ogg, Vorbis, Opus and opusfile: BSD licence",
	"ioquake3 contributors",
	"any key: next page",
	"any key: back",
	"SCORE",
	"DEATHS",
	"DAMAGE",
	"PING",
	"NAME",
	"ACCURACY",
	"%s called a timeout",
	"Timeout over",
	"No timeout now",
	"Your timeout is used",
	"Timeouts are for duel and team deathmatch",
	"TIMEOUT %i",
	"Damage given",
	"Damage taken",
	"Mega health",
	"Heavy armour",
	"Light armour",
	"Accuracy",
	"SLIDE %.1f",
	"All",
	"FIND SERVERS",
	"Servers",
	"Internet",
	"Local network",
	"Favourites",
	"Mode",
	"Search",
	"Hide empty",
	"Hide full",
	"Hide passworded",
	"Max ping",
	"Any",
	"Server",
	"Map",
	"Players",
	"Ping",
	"REFRESH",
	"CONNECT",
	"ADD FAVOURITE",
	"REMOVE FAVOURITE",
	"Looking for servers...",
	"%i of %i servers",
	"No master server is set (sv_master1).",
	"No servers found.",
	"Retina",
	"Draws at the display's full resolution; sharper, slower. Applies after a restart.",
	"Zoom sensitivity",
	"Sensitivity while zoomed; Auto scales with the zoom.",
	"Auto",
	"Crosshair colour",
	"Crosshair colour.",
	"White",
	"Red",
	"Green",
	"Yellow",
	"Blue",
	"Cyan",
	"Pink",
	"Orange",
	"Speedometer",
	"Your speed under the crosshair."
};

/*
==============
BG_String
==============
*/
const char *BG_String( int id ) {
	if ( id <= STR_NONE || id >= STR_NUM_STRINGS ) {
		return "";
	}
	return bg_stringsEnglish[id];
}

/*
==============
BG_ChampionForName

Returns the index of the named champion, or 0 (the default) if unknown
==============
*/
int BG_ChampionForName( const char *name ) {
	int		i;

	for ( i = 0 ; i < bg_numChampions ; i++ ) {
		if ( !Q_stricmp( name, bg_championList[i].name ) ) {
			return i;
		}
	}
	return 0;
}

/*
==============
BG_ChampionForModel

Returns the champion that uses this player model ("name" or "name/skin"),
or -1 if none does
==============
*/
int BG_ChampionForModel( const char *model ) {
	char	name[MAX_QPATH];
	char	*slash;
	int		i;

	Q_strncpyz( name, model, sizeof( name ) );
	slash = strchr( name, '/' );
	if ( slash ) {
		*slash = 0;
	}
	for ( i = 0 ; i < bg_numChampions ; i++ ) {
		if ( !Q_stricmp( name, bg_championList[i].model ) ) {
			return i;
		}
	}
	return -1;
}

/*
==============
BG_Champion
==============
*/
const champion_t *BG_Champion( int index ) {
	if ( index < 0 || index >= bg_numChampions ) {
		index = 0;
	}
	return &bg_championList[index];
}

/*
==============================================================================

MEDALS

==============================================================================
*/

const medalInfo_t bg_medalInfo[MEDAL_NUM] = {
	{ STR_MEDAL_FIRSTBLOOD,	"firstblood" },
	{ STR_MEDAL_MIDAIR,		"midair" },
	{ STR_MEDAL_AIRBORNE,		"airborne" },
	{ STR_MEDAL_COMBOKILL,		"combokill" },
	{ STR_MEDAL_COLLATERAL,	"collateral" },
	{ STR_MEDAL_DOUBLEWHAMMY,	"doublewhammy" },
	{ STR_MEDAL_DENIED,		"denied" },
	{ STR_MEDAL_REVENGE,		"revenge" },
	{ STR_MEDAL_PARTYBREAKER,	"partybreaker" },
	{ STR_MEDAL_PERFORATED,	"perforated" },
	{ STR_MEDAL_POINTBLANK,	"pointblank" },
	{ STR_MEDAL_PRECISE,		"precise" },
	{ STR_MEDAL_RINGOUT,		"ringout" },
	{ STR_MEDAL_STOPWATCH,		"stopwatch" },
	{ STR_MEDAL_TANK,		"tank" },
	{ STR_MEDAL_DIEHARD,		"diehard" },
	{ STR_MEDAL_FROMTHEGRAVE,	"fromthegrave" },
	{ STR_MEDAL_KAMIKAZE,		"kamikaze" },
	{ STR_MEDAL_SHOWSTOPPER,	"showstopper" },
	{ STR_MEDAL_TRIPLEKILL,	"triplekill" },
	{ STR_MEDAL_FIGHTER,		"fighter" },
	{ STR_MEDAL_AVATAR,		"avatarofdeath" },
	{ STR_MEDAL_SPREE,		"killingspree" },
	{ STR_MEDAL_RAGE,		"rage" },
	{ STR_MEDAL_FRENZY,		"frenzy" },
	{ STR_MEDAL_RAMPAGE,		"rampage" },
	{ STR_MEDAL_DOMINATING,	"dominating" },
	{ STR_MEDAL_PUMASSACRE,	"powerupmassacre" },
	{ STR_MEDAL_GOD_GAUNTLET,	"gauntletgod" },
	{ STR_MEDAL_GOD_MG,		"machinegungod" },
	{ STR_MEDAL_GOD_SG,		"shotgungod" },
	{ STR_MEDAL_GOD_NG,		"nailgungod" },
	{ STR_MEDAL_GOD_TB,		"triboltgod" },
	{ STR_MEDAL_GOD_RL,		"rocketgod" },
	{ STR_MEDAL_GOD_LG,		"lightninggod" },
	{ STR_MEDAL_GOD_RG,		"railgod" },
	{ STR_MEDAL_DAMAGEDEALER,	"damagedealer" },
	{ STR_MEDAL_NETMASTER,		"netmaster" },
	{ STR_MEDAL_ASSISTANT,		"assistant" }
};

/*
==============================================================================

LOADING-SCREEN TIPS

==============================================================================
*/

const tipInfo_t bg_tips[] = {
	{ TIP_GENERAL, "", STR_TIP_GEN_1 },
	{ TIP_GENERAL, "", STR_TIP_GEN_2 },
	{ TIP_GENERAL, "", STR_TIP_GEN_3 },
	{ TIP_GENERAL, "", STR_TIP_GEN_4 },
	{ TIP_GENERAL, "", STR_TIP_GEN_5 },
	{ TIP_GENERAL, "", STR_TIP_GEN_6 },
	{ TIP_GENERAL, "", STR_TIP_GEN_7 },
	{ TIP_GENERAL, "", STR_TIP_GEN_8 },
	{ TIP_GENERAL, "", STR_TIP_GEN_9 },
	{ TIP_GENERAL, "", STR_TIP_GEN_10 },
	{ TIP_GENERAL, "", STR_TIP_GEN_11 },
	{ TIP_GENERAL, "", STR_TIP_GEN_12 },
	{ TIP_GENERAL, "", STR_TIP_GEN_13 },
	{ TIP_MODE, "duel", STR_TIP_MODE_DUEL_1 },
	{ TIP_MODE, "duel", STR_TIP_MODE_DUEL_2 },
	{ TIP_MODE, "tdm", STR_TIP_MODE_TDM_1 },
	{ TIP_MODE, "tdm", STR_TIP_MODE_TDM_2 },
	{ TIP_MODE, "ctf", STR_TIP_MODE_CTF_1 },
	{ TIP_MODE, "ctf", STR_TIP_MODE_CTF_2 },
	{ TIP_MODE, "ca", STR_TIP_MODE_CA_1 },
	{ TIP_MODE, "ca", STR_TIP_MODE_CA_2 },
	{ TIP_MODE, "sacrifice", STR_TIP_MODE_SACRIFICE_1 },
	{ TIP_MODE, "sacrifice", STR_TIP_MODE_SACRIFICE_2 },
	{ TIP_MODE, "roundduel", STR_TIP_MODE_ROUNDDUEL_1 },
	{ TIP_MODE, "roundduel", STR_TIP_MODE_ROUNDDUEL_2 },
	{ TIP_CHAMPION, "ranger", STR_TIP_CH_RANGER_1 },
	{ TIP_CHAMPION, "anarki", STR_TIP_CH_ANARKI_1 },
	{ TIP_CHAMPION, "athena", STR_TIP_CH_ATHENA_1 },
	{ TIP_CHAMPION, "athena", STR_TIP_CH_ATHENA_2 },
	{ TIP_CHAMPION, "nyx", STR_TIP_CH_NYX_1 },
	{ TIP_CHAMPION, "nyx", STR_TIP_CH_NYX_2 },
	{ TIP_CHAMPION, "slash", STR_TIP_CH_SLASH_1 },
	{ TIP_CHAMPION, "slash", STR_TIP_CH_SLASH_2 },
	{ TIP_CHAMPION, "bj", STR_TIP_CH_BJ_1 },
	{ TIP_CHAMPION, "deathknight", STR_TIP_CH_DEATHKNIGHT_1 },
	{ TIP_CHAMPION, "doomslayer", STR_TIP_CH_DOOMSLAYER_1 },
	{ TIP_CHAMPION, "doomslayer", STR_TIP_CH_DOOMSLAYER_2 },
	{ TIP_CHAMPION, "eisen", STR_TIP_CH_EISEN_1 },
	{ TIP_CHAMPION, "galena", STR_TIP_CH_GALENA_1 },
	{ TIP_CHAMPION, "strogg", STR_TIP_CH_STROGG_1 },
	{ TIP_CHAMPION, "visor", STR_TIP_CH_VISOR_1 },
	{ TIP_CHAMPION, "clutch", STR_TIP_CH_CLUTCH_1 },
	{ TIP_CHAMPION, "clutch", STR_TIP_CH_CLUTCH_2 },
	{ TIP_CHAMPION, "keel", STR_TIP_CH_KEEL_1 },
	{ TIP_CHAMPION, "scalebearer", STR_TIP_CH_SCALEBEARER_1 },
	{ TIP_CHAMPION, "sorlag", STR_TIP_CH_SORLAG_1 },
	{ TIP_CHAMPION, "sorlag", STR_TIP_CH_SORLAG_2 },
	{ TIP_CHAMPION, "wrack", STR_TIP_CH_WRACK_1 },
	{ TIP_CHAMPION, "sarge", STR_TIP_CH_SARGE_1 },
	{ TIP_CHAMPION, "klesk", STR_TIP_CH_KLESK_1 },
	{ TIP_CHAMPION, "hunter", STR_TIP_CH_HUNTER_1 },
	{ TIP_CHAMPION, "xaero", STR_TIP_CH_XAERO_1 },
	{ TIP_CHAMPION, "raine", STR_TIP_CH_RAINE_1 },
	{ TIP_CHAMPION, "beastmaster", STR_TIP_CH_BEASTMASTER_1 },
	{ TIP_CHAMPION, "unumrey", STR_TIP_CH_UNUMREY_1 },
};
const int bg_numTips = ARRAY_LEN( bg_tips );

/*
==============================================================================

ABILITY TIMING

Defaults for the ab_<name>_<param> cvars: charges, recharge msec, delay msec,
duration msec, weapon lockout msec. Numbers are placeholders until measured.

==============================================================================
*/

const abilityInfo_t bg_abilityInfo[ACTIVE_NUM_ABILITIES] = {
	{ "none",			STR_NONE,					qfalse,	{ "0", "0", "0", "0", "0" } },
	{ "hook",			STR_ABILITY_HOOK,			qfalse,	{ "3", "15000", "100", "0", "500" } },
	{ "bullrush",		STR_ABILITY_BULLRUSH,		qtrue,	{ "1", "60000", "0", "3000", "400" } },
	{ "injection",		STR_ABILITY_INJECTION,		qfalse,	{ "1", "60000", "0", "5000", "1200" } },
	{ "dualwield",		STR_ABILITY_DUALWIELD,		qfalse,	{ "1", "60000", "0", "5500", "400" } },
	{ "barrier",		STR_ABILITY_BARRIER,		qtrue,	{ "1", "60000", "250", "8000", "500" } },
	{ "flamestrike",	STR_ABILITY_FLAMESTRIKE,	qfalse,	{ "1", "60000", "125", "5000", "0" } },
	{ "berserk",		STR_ABILITY_BERSERK,		qtrue,	{ "1", "60000", "800", "3000", "200" } },
	{ "turret",			STR_ABILITY_TURRET,			qfalse,	{ "1", "60000", "0", "0", "500" } },
	{ "totem",			STR_ABILITY_TOTEM,			qfalse,	{ "1", "60000", "0", "0", "500" } },
	{ "grenades",		STR_ABILITY_GRENADES,		qfalse,	{ "3", "20000", "0", "0", "500" } },
	{ "ghostwalk",		STR_ABILITY_GHOSTWALK,		qtrue,	{ "1", "60000", "0", "3000", "500" } },
	{ "orb",			STR_ABILITY_ORB,			qfalse,	{ "1", "60000", "125", "16000", "500" } },
	{ "plasmatrail",	STR_ABILITY_PLASMATRAIL,	qfalse,	{ "1", "60000", "250", "3000", "0" } },
	{ "acidspit",		STR_ABILITY_ACIDSPIT,		qfalse,	{ "1", "60000", "0", "1000", "1300" } },
	{ "drone",			STR_ABILITY_DRONE,			qfalse,	{ "1", "60000", "500", "45000", "0" } },
	{ "piercingsight",	STR_ABILITY_PIERCINGSIGHT,	qfalse,	{ "1", "60000", "0", "5000", "0" } },
	{ "shellshock",		STR_ABILITY_SHELLSHOCK,		qfalse,	{ "1", "60000", "0", "0", "500" } },
	{ "spinevolley",	STR_ABILITY_SPINEVOLLEY,	qfalse,	{ "1", "60000", "100", "0", "500" } },
	{ "eternal",		STR_ABILITY_ETERNAL,		qfalse,	{ "1", "60000", "250", "5000", "0" } },
	{ "supplydrop",		STR_ABILITY_SUPPLYDROP,		qfalse,	{ "1", "60000", "0", "0", "500" } },
	{ "markprey",		STR_ABILITY_MARKPREY,		qfalse,	{ "1", "60000", "0", "0", "250" } },
	{ "downpour",		STR_ABILITY_DOWNPOUR,		qfalse,	{ "1", "60000", "0", "0", "500" } },
	{ "hound",			STR_ABILITY_HOUND,			qfalse,	{ "1", "60000", "250", "0", "500" } },
	{ "riftanchor",		STR_ABILITY_RIFTANCHOR,		qtrue,	{ "1", "60000", "0", "6000", "500" } }
};

/*
==============================================================================

WEAPON NUMBERS

Defaults for the wp_<name>_<param> cvars, in weaponParam_t order:
damage, refire, range, spread, pellets, knock, knock in air, knock on self,
splash, splash at the edge, radius, speed, ammo start, ammo max.
Weapons not listed keep their classic behaviour.

==============================================================================
*/

const weaponNumbers_t bg_weaponInfo[] = {
	{ WP_GAUNTLET,			"gauntlet",	{ "75", "500", "30", "0", "0", "45", "50", "0", "0", "0", "0", "0", "-1", "-1" } },
	{ WP_MACHINEGUN,		"mg",		{ "6", "100", "6000", "0.02", "0", "7", "7", "0", "0", "0", "0", "0", "100", "150" } },
	{ WP_SHOTGUN,			"sg",		{ "5", "850", "2000", "0.085", "16", "2.5", "2.5", "0", "0", "0", "0", "0", "10", "25" } },
	{ WP_ROCKET_LAUNCHER,	"rl",		{ "100", "800", "0", "0", "0", "90", "90", "120", "100", "10", "120", "1000", "10", "25" } },
	{ WP_LIGHTNING,			"lg",		{ "6", "44", "768", "0", "0", "7", "7.1", "0", "0", "0", "0", "0", "100", "200" } },
	{ WP_RAILGUN,			"rg",		{ "80", "1500", "6000", "0", "0", "50", "50", "0", "0", "0", "0", "0", "10", "10" } },
	{ WP_HEAVY_MACHINEGUN,	"hmg",		{ "9", "100", "6000", "0.01", "0", "7", "7", "0", "0", "0", "0", "0", "100", "150" } },
	{ WP_SUPER_SHOTGUN,		"ssg",		{ "5", "1000", "2000", "0.085", "16", "2.5", "2.5", "0", "0", "0", "0", "0", "10", "25" } },
	{ WP_NAILGUN,			"ng",		{ "15", "100", "0", "0", "0", "6", "6", "28", "12", "0", "20", "1800", "100", "150" } },
	{ WP_SUPER_NAILGUN,		"sng",		{ "20", "100", "0", "0", "0", "6", "6", "28", "15", "0", "20", "2000", "100", "150" } },
	{ WP_TRIBOLT,			"tribolt",	{ "35", "1300", "0", "0", "0", "50", "50", "50", "50", "10", "120", "1600", "10", "15" } }
};

/*
==============
BG_AmmoPartner

The other weapon sharing this weapon's ammo pool, or WP_NONE
==============
*/
int BG_AmmoPartner( int weapon ) {
	switch ( weapon ) {
	case WP_MACHINEGUN:			return WP_HEAVY_MACHINEGUN;
	case WP_HEAVY_MACHINEGUN:	return WP_MACHINEGUN;
	case WP_SHOTGUN:			return WP_SUPER_SHOTGUN;
	case WP_SUPER_SHOTGUN:		return WP_SHOTGUN;
	case WP_NAILGUN:			return WP_SUPER_NAILGUN;
	case WP_SUPER_NAILGUN:		return WP_NAILGUN;
	default:					return WP_NONE;
	}
}

/*
==============
BG_WeaponVisual

The weapon whose effects a new weapon borrows on the client
==============
*/
int BG_WeaponVisual( int weapon ) {
	switch ( weapon ) {
	case WP_HEAVY_MACHINEGUN:	return WP_MACHINEGUN;
	case WP_SUPER_SHOTGUN:		return WP_SHOTGUN;
#ifndef MISSIONPACK
	case WP_NAILGUN:			return WP_PLASMAGUN;
#endif
	default:					return weapon;
	}
}

/*
==============
BG_WeaponSlot

Weapon key: 1 gauntlet, 2 machineguns, 3 shotguns, 4 nailguns, 5 tri-bolt,
6 rocket launcher, 7 lightning gun, 8 railgun
==============
*/
int BG_WeaponSlot( int weapon ) {
	switch ( weapon ) {
	case WP_GAUNTLET:									return 1;
	case WP_MACHINEGUN: case WP_HEAVY_MACHINEGUN:		return 2;
	case WP_SHOTGUN: case WP_SUPER_SHOTGUN:				return 3;
	case WP_NAILGUN: case WP_SUPER_NAILGUN:				return 4;
	case WP_TRIBOLT:									return 5;
	case WP_ROCKET_LAUNCHER:							return 6;
	case WP_LIGHTNING:									return 7;
	case WP_RAILGUN:									return 8;
	default:											return 0;
	}
}

/*
==============
BG_StartingWeapon

Starting weapons are chosen before spawning, never placed or dropped
==============
*/
qboolean BG_StartingWeapon( int weapon ) {
	return weapon == WP_MACHINEGUN || weapon == WP_SHOTGUN || weapon == WP_NAILGUN;
}

const int bg_numWeaponInfo = ARRAY_LEN( bg_weaponInfo );

/*
==============
BG_SetAbility
==============
*/
void BG_SetAbility( playerState_t *ps, int state, int charges ) {
	ps->stats[STAT_ABILITY] = ( charges & ABILITY_CHARGES_MASK ) | ( state << ABILITY_STATE_SHIFT );
}


/*
==============
BG_FindItemForPowerup
==============
*/
gitem_t	*BG_FindItemForPowerup( powerup_t pw ) {
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( (bg_itemlist[i].giType == IT_POWERUP || 
					bg_itemlist[i].giType == IT_TEAM ||
					bg_itemlist[i].giType == IT_PERSISTANT_POWERUP) && 
			bg_itemlist[i].giTag == pw ) {
			return &bg_itemlist[i];
		}
	}

	return NULL;
}


/*
==============
BG_FindItemForHoldable
==============
*/
gitem_t	*BG_FindItemForHoldable( holdable_t pw ) {
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( bg_itemlist[i].giType == IT_HOLDABLE && bg_itemlist[i].giTag == pw ) {
			return &bg_itemlist[i];
		}
	}

	Com_Error( ERR_DROP, "HoldableItem not found" );

	return NULL;
}


/*
===============
BG_FindItemForWeapon

===============
*/
gitem_t	*BG_FindItemForWeapon( weapon_t weapon ) {
	gitem_t	*it;
	
	for ( it = bg_itemlist + 1 ; it->classname ; it++) {
		if ( it->giType == IT_WEAPON && it->giTag == weapon ) {
			return it;
		}
	}

	Com_Error( ERR_DROP, "Couldn't find item for weapon %i", weapon);
	return NULL;
}

/*
===============
BG_FindItem

===============
*/
gitem_t	*BG_FindItem( const char *pickupName ) {
	gitem_t	*it;
	
	for ( it = bg_itemlist + 1 ; it->classname ; it++ ) {
		if ( !Q_stricmp( it->pickup_name, pickupName ) )
			return it;
	}

	return NULL;
}

/*
============
BG_PlayerTouchesItem

Items can be picked up without actually touching their physical bounds to make
grabbing them easier
============
*/
qboolean	BG_PlayerTouchesItem( playerState_t *ps, entityState_t *item, int atTime ) {
	vec3_t		origin;

	BG_EvaluateTrajectory( &item->pos, atTime, origin );

	// we are ignoring ducked differences here
	if ( ps->origin[0] - origin[0] > 44
		|| ps->origin[0] - origin[0] < -50
		|| ps->origin[1] - origin[1] > 36
		|| ps->origin[1] - origin[1] < -36
		|| ps->origin[2] - origin[2] > 36
		|| ps->origin[2] - origin[2] < -36 ) {
		return qfalse;
	}

	return qtrue;
}



/*
================
BG_CanItemBeGrabbed

Returns false if the item should not be picked up.
This needs to be the same for client side prediction and server use.
================
*/
/*
================
BG_ItemTimerClass

Which spectator item timer an item belongs to (cg_specItemTimers bits):
1 power-ups, 2 mega health, 4 heavy armour, 8 light armour; 0 for none
================
*/
int BG_ItemTimerClass( const gitem_t *item ) {
	switch ( item->giType ) {
	case IT_POWERUP:	return 1;
	case IT_HEALTH:		return item->quantity == 100 ? 2 : 0;
	case IT_ARMOR:		return item->quantity >= 100 ? 4 : item->quantity >= 50 ? 8 : 0;
	default:			return 0;
	}
}

/*
================
BG_AmmoBoxAmount

What an ammo box gives a weapon: a lot for the fast-firing ones, a few
shots for the rest
================
*/
int BG_AmmoBoxAmount( int weapon ) {
	switch ( weapon ) {
	case WP_MACHINEGUN: case WP_HEAVY_MACHINEGUN:
	case WP_NAILGUN: case WP_SUPER_NAILGUN:
	case WP_LIGHTNING:
		return 50;
	case WP_SHOTGUN: case WP_SUPER_SHOTGUN:
	case WP_ROCKET_LAUNCHER: case WP_TRIBOLT: case WP_RAILGUN:
		return 5;
	default:
		return 0;
	}
}

/*
================
BG_AmmoBoxUseful

True when some carried weapon is below its ammo maximum
================
*/
qboolean BG_AmmoBoxUseful( const playerState_t *ps ) {
	int		w, max;

	for ( w = WP_GAUNTLET + 1; w < WP_NUM_WEAPONS; w++ ) {
		if ( !( ps->stats[STAT_WEAPONS] & ( 1 << w ) ) || !BG_AmmoBoxAmount( w ) ) {
			continue;
		}
		max = BG_WeaponTuned( w ) && BG_WeaponParam( w, WPP_AMMOMAX ) > 0 ? BG_WeaponParam( w, WPP_AMMOMAX ) : 200;
		if ( ps->ammo[w] < max ) {
			return qtrue;
		}
	}
	return qfalse;
}

qboolean BG_CanItemBeGrabbed( int gametype, const entityState_t *ent, const playerState_t *ps ) {
	gitem_t	*item;
#ifdef MISSIONPACK
	int		upperBound;
#endif

	if ( ent->modelindex < 1 || ent->modelindex >= bg_numItems ) {
		Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: index out of range" );
	}

	item = &bg_itemlist[ent->modelindex];

	switch( item->giType ) {
	case IT_WEAPON:
		return qtrue;	// weapons are always picked up

	case IT_AMMO:
		if ( item->giTag == WP_NONE ) {
			return BG_AmmoBoxUseful( ps );
		}
		if ( ps->ammo[ item->giTag ] >= ( BG_WeaponTuned( item->giTag ) && BG_WeaponParam( item->giTag, WPP_AMMOMAX ) > 0
			? BG_WeaponParam( item->giTag, WPP_AMMOMAX ) : 200 ) ) {
			return qfalse;		// can't hold any more
		}
		return qtrue;

	case IT_ARMOR:
#ifdef MISSIONPACK
		if( bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_SCOUT ) {
			return qfalse;
		}

		// we also clamp armor to the maxhealth for handicapping
		if( bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
			upperBound = ps->stats[STAT_MAX_HEALTH];
		}
		else {
			upperBound = ps->stats[STAT_MAX_HEALTH] * 2;
		}

		if ( ps->stats[STAT_ARMOR] >= upperBound ) {
			return qfalse;
		}
#else
		{
			// light armour only below the class base; heavy armour and
			// shards up to the class maximum (Eisen 150); as Pickup_Armor
			const champion_t	*champ = BG_Champion( ps->stats[STAT_CHAMPION] );
			int					cap = bg_classInfo[champ->bodyClass].maxArmor;

			if ( !Q_stricmp( champ->name, "eisen" ) ) {
				cap = 150;
			}
			if ( item->quantity >= 25 && item->quantity < 100 ) {
				cap = bg_classInfo[champ->bodyClass].baseArmor;
			}
			if ( ps->stats[STAT_ARMOR] >= cap ) {
				return qfalse;
			}
		}
#endif
		return qtrue;

	case IT_HEALTH:
		// small and mega healths will go over the max, otherwise
		// don't pick up if already at max
#ifdef MISSIONPACK
		if( bg_itemlist[ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD ) {
		}
		else
#endif
		// mega health up to the class bonus over max; the rest only below max
		if ( item->quantity == 100 ) {
			if ( ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH]
				+ bg_classInfo[BG_Champion( ps->stats[STAT_CHAMPION] )->bodyClass].maxHealthBonus ) {
				return qfalse;
			}
			return qtrue;
		}

		if ( ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH] ) {
			return qfalse;
		}
		return qtrue;

	case IT_POWERUP:
		return qtrue;	// powerups are always picked up

	case IT_HOURGLASS:
		{
			// only while the ability is recharging
			int		active = BG_Champion( ps->stats[STAT_CHAMPION] )->active;

			return active > ACTIVE_NONE && BG_AbilityCharges( ps ) < BG_AbilityParam( active, AP_CHARGES );
		}

#ifdef MISSIONPACK
	case IT_PERSISTANT_POWERUP:
		// can only hold one item at a time
		if ( ps->stats[STAT_PERSISTANT_POWERUP] ) {
			return qfalse;
		}

		// check team only
		if( ( ent->generic1 & 2 ) && ( ps->persistant[PERS_TEAM] != TEAM_RED ) ) {
			return qfalse;
		}
		if( ( ent->generic1 & 4 ) && ( ps->persistant[PERS_TEAM] != TEAM_BLUE ) ) {
			return qfalse;
		}

		return qtrue;
#endif

	case IT_TEAM: // team items, such as flags
#ifdef MISSIONPACK		
		if( gametype == GT_1FCTF ) {
			// neutral flag can always be picked up
			if( item->giTag == PW_NEUTRALFLAG ) {
				return qtrue;
			}
			if (ps->persistant[PERS_TEAM] == TEAM_RED) {
				if (item->giTag == PW_BLUEFLAG  && ps->powerups[PW_NEUTRALFLAG] ) {
					return qtrue;
				}
			} else if (ps->persistant[PERS_TEAM] == TEAM_BLUE) {
				if (item->giTag == PW_REDFLAG  && ps->powerups[PW_NEUTRALFLAG] ) {
					return qtrue;
				}
			}
		}
#endif
		if( gametype == GT_CTF ) {
			// ent->modelindex2 is non-zero on items if they are dropped
			// we need to know this because we can pick up our dropped flag (and return it)
			// but we can't pick up our flag at base
			if (ps->persistant[PERS_TEAM] == TEAM_RED) {
				if (item->giTag == PW_BLUEFLAG ||
					(item->giTag == PW_REDFLAG && ent->modelindex2) ||
					(item->giTag == PW_REDFLAG && ps->powerups[PW_BLUEFLAG]) )
					return qtrue;
			} else if (ps->persistant[PERS_TEAM] == TEAM_BLUE) {
				if (item->giTag == PW_REDFLAG ||
					(item->giTag == PW_BLUEFLAG && ent->modelindex2) ||
					(item->giTag == PW_BLUEFLAG && ps->powerups[PW_REDFLAG]) )
					return qtrue;
			}
		}

#ifdef MISSIONPACK
		if( gametype == GT_HARVESTER ) {
			return qtrue;
		}
#endif
		return qfalse;

	case IT_HOLDABLE:
		// can only hold one item at a time
		if ( ps->stats[STAT_HOLDABLE_ITEM] ) {
			return qfalse;
		}
		return qtrue;

        case IT_BAD:
            Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: IT_BAD" );
        default:
#ifndef Q3_VM
#ifndef NDEBUG
          Com_Printf("BG_CanItemBeGrabbed: unknown enum %d\n", item->giType );
#endif
#endif
         break;
	}

	return qfalse;
}

//======================================================================

/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result ) {
	float		deltaTime;
	float		phase;

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorCopy( tr->trBase, result );
		break;
	case TR_LINEAR:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		phase = sin( deltaTime * M_PI * 2 );
		VectorMA( tr->trBase, phase, tr->trDelta, result );
		break;
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		if ( deltaTime < 0 ) {
			deltaTime = 0;
		}
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;		// FIXME: local gravity...
		break;
	default:
		Com_Error( ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trType );
		break;
	}
}

/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result ) {
	float	deltaTime;
	float	phase;

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorClear( result );
		break;
	case TR_LINEAR:
		VectorCopy( tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		phase = cos( deltaTime * M_PI * 2 );	// derivative of sin = cos
		phase *= 0.5;
		VectorScale( tr->trDelta, phase, result );
		break;
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			VectorClear( result );
			return;
		}
		VectorCopy( tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorCopy( tr->trDelta, result );
		result[2] -= DEFAULT_GRAVITY * deltaTime;		// FIXME: local gravity...
		break;
	default:
		Com_Error( ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trType );
		break;
	}
}

char *eventnames[] = {
	"EV_NONE",

	"EV_FOOTSTEP",
	"EV_FOOTSTEP_METAL",
	"EV_FOOTSPLASH",
	"EV_FOOTWADE",
	"EV_SWIM",

	"EV_STEP_4",
	"EV_STEP_8",
	"EV_STEP_12",
	"EV_STEP_16",

	"EV_FALL_SHORT",
	"EV_FALL_MEDIUM",
	"EV_FALL_FAR",

	"EV_JUMP_PAD",			// boing sound at origin", jump sound on player

	"EV_JUMP",
	"EV_WATER_TOUCH",	// foot touches
	"EV_WATER_LEAVE",	// foot leaves
	"EV_WATER_UNDER",	// head touches
	"EV_WATER_CLEAR",	// head leaves

	"EV_ITEM_PICKUP",			// normal item pickups are predictable
	"EV_GLOBAL_ITEM_PICKUP",	// powerup / team sounds are broadcast to everyone

	"EV_NOAMMO",
	"EV_CHANGE_WEAPON",
	"EV_FIRE_WEAPON",

	"EV_USE_ITEM0",
	"EV_USE_ITEM1",
	"EV_USE_ITEM2",
	"EV_USE_ITEM3",
	"EV_USE_ITEM4",
	"EV_USE_ITEM5",
	"EV_USE_ITEM6",
	"EV_USE_ITEM7",
	"EV_USE_ITEM8",
	"EV_USE_ITEM9",
	"EV_USE_ITEM10",
	"EV_USE_ITEM11",
	"EV_USE_ITEM12",
	"EV_USE_ITEM13",
	"EV_USE_ITEM14",
	"EV_USE_ITEM15",

	"EV_ITEM_RESPAWN",
	"EV_ITEM_POP",
	"EV_PLAYER_TELEPORT_IN",
	"EV_PLAYER_TELEPORT_OUT",

	"EV_GRENADE_BOUNCE",		// eventParm will be the soundindex

	"EV_GENERAL_SOUND",
	"EV_GLOBAL_SOUND",		// no attenuation
	"EV_GLOBAL_TEAM_SOUND",

	"EV_BULLET_HIT_FLESH",
	"EV_BULLET_HIT_WALL",

	"EV_MISSILE_HIT",
	"EV_MISSILE_MISS",
	"EV_MISSILE_MISS_METAL",
	"EV_RAILTRAIL",
	"EV_SHOTGUN",
	"EV_BULLET",				// otherEntity is the shooter

	"EV_PAIN",
	"EV_DEATH1",
	"EV_DEATH2",
	"EV_DEATH3",
	"EV_OBITUARY",

	"EV_POWERUP_QUAD",
	"EV_POWERUP_BATTLESUIT",
	"EV_POWERUP_REGEN",

	"EV_GIB_PLAYER",			// gib a previously living player
	"EV_SCOREPLUM",			// score plum

//#ifdef MISSIONPACK
	"EV_PROXIMITY_MINE_STICK",
	"EV_PROXIMITY_MINE_TRIGGER",
	"EV_KAMIKAZE",			// kamikaze explodes
	"EV_OBELISKEXPLODE",		// obelisk explodes
	"EV_OBELISKPAIN",		// obelisk pain
	"EV_INVUL_IMPACT",		// invulnerability sphere impact
	"EV_JUICED",				// invulnerability juiced effect
	"EV_LIGHTNINGBOLT",		// lightning bolt bounced of invulnerability sphere
//#endif

	"EV_DEBUG_LINE",
	"EV_STOPLOOPINGSOUND",
	"EV_TAUNT",
	"EV_TAUNT_YES",
	"EV_TAUNT_NO",
	"EV_TAUNT_FOLLOWME",
	"EV_TAUNT_GETFLAG",
	"EV_TAUNT_GUARDBASE",
	"EV_TAUNT_PATROL",

	"EV_USE_ABILITY",
	"EV_ABILITY_REPRESS"
};

/*
===============
BG_AddPredictableEventToPlayerstate

Handles the sequence numbers
===============
*/

void	trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

void BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps ) {

#ifdef _DEBUG
	{
		char buf[256];
		trap_Cvar_VariableStringBuffer("showevents", buf, sizeof(buf));
		if ( atof(buf) != 0 ) {
#ifdef QAGAME
			Com_Printf(" game event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#else
			Com_Printf("Cgame event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#endif
		}
	}
#endif
	ps->events[ps->eventSequence & (MAX_PS_EVENTS-1)] = newEvent;
	ps->eventParms[ps->eventSequence & (MAX_PS_EVENTS-1)] = eventParm;
	ps->eventSequence++;
}

/*
========================
BG_TouchJumpPad
========================
*/
void BG_TouchJumpPad( playerState_t *ps, entityState_t *jumppad ) {
	vec3_t	angles;
	float p;
	int effectNum;

	// spectators don't use jump pads
	if ( ps->pm_type != PM_NORMAL ) {
		return;
	}

	// a jump pad ends any crouch slide
	if ( BG_Champion( ps->stats[STAT_CHAMPION] )->moveStyle == MOVE_SLIDE ) {
		ps->stats[STAT_MOVE_STATE] = 0;
	}

	// flying characters don't hit bounce pads
	if ( ps->powerups[PW_FLIGHT] ) {
		return;
	}

	// if we didn't hit this same jumppad the previous frame
	// then don't play the event sound again if we are in a fat trigger
	if ( ps->jumppad_ent != jumppad->number ) {

		vectoangles( jumppad->origin2, angles);
		p = fabs( AngleNormalize180( angles[PITCH] ) );
		if( p < 45 ) {
			effectNum = 0;
		} else {
			effectNum = 1;
		}
		BG_AddPredictableEventToPlayerstate( EV_JUMP_PAD, effectNum, ps );
	}
	// remember hitting this jumppad this frame
	ps->jumppad_ent = jumppad->number;
	ps->jumppad_frame = ps->pmove_framecount;
	// give the player the velocity from the jumppad
	VectorCopy( jumppad->origin2, ps->velocity );
}

/*
========================
BG_PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap ) {
	int		i;

	if ( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR ) {
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_INTERPOLATE;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction
	VectorCopy( ps->velocity, s->pos.trDelta );

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;		// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config
	s->eFlags = ps->eFlags;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->loopSound = ps->loopSound;
	s->generic1 = ps->generic1;
}

/*
========================
BG_PlayerStateToEntityStateExtraPolate

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s, int time, qboolean snap ) {
	int		i;

	if ( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR ) {
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_LINEAR_STOP;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction and linear prediction
	VectorCopy( ps->velocity, s->pos.trDelta );
	// set the time for linear prediction
	s->pos.trTime = time;
	// set maximum extra polation time
	s->pos.trDuration = 16; // one server tick

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;		// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config
	s->eFlags = ps->eFlags;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->loopSound = ps->loopSound;
	s->generic1 = ps->generic1;
}
