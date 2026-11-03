// The items bots know: this game's item list (bg_itemlist in bg_misc.c,
// "modelindex" its place in that list), the inventory slot each fills
// (inv.h) and when it comes back (bots-audit BOT-02). Our own data.

#include "inv.h"

iteminfo "item_armor_shard"
{
	name		"Armor Shard"
	modelindex	1
	type		0
	index		INVENTORY_ARMOR
	respawntime	25
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_armor_combat"
{
	name		"Armor"
	modelindex	2
	type		0
	index		INVENTORY_ARMOR
	respawntime	25
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_armor_body"
{
	name		"Heavy Armor"
	modelindex	3
	type		0
	index		INVENTORY_ARMOR
	respawntime	25
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_health_small"
{
	name		"5 Health"
	modelindex	4
	type		0
	index		INVENTORY_HEALTH
	respawntime	35
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_health"
{
	name		"25 Health"
	modelindex	5
	type		0
	index		INVENTORY_HEALTH
	respawntime	35
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_health_large"
{
	name		"50 Health"
	modelindex	6
	type		0
	index		INVENTORY_HEALTH
	respawntime	35
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_health_mega"
{
	name		"Mega Health"
	modelindex	7
	type		0
	index		INVENTORY_HEALTH
	respawntime	35
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_gauntlet"
{
	name		"Gauntlet"
	modelindex	8
	type		0
	index		INVENTORY_GAUNTLET
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_shotgun"
{
	name		"Shotgun"
	modelindex	9
	type		0
	index		INVENTORY_SHOTGUN
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_machinegun"
{
	name		"Machinegun"
	modelindex	10
	type		0
	index		INVENTORY_MACHINEGUN
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_grenadelauncher"
{
	name		"Tri-bolt"
	modelindex	11
	type		0
	index		INVENTORY_GRENADELAUNCHER
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_rocketlauncher"
{
	name		"Rocket Launcher"
	modelindex	12
	type		0
	index		INVENTORY_ROCKETLAUNCHER
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_lightning"
{
	name		"Lightning Gun"
	modelindex	13
	type		0
	index		INVENTORY_LIGHTNING
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_railgun"
{
	name		"Railgun"
	modelindex	14
	type		0
	index		INVENTORY_RAILGUN
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_plasmagun"
{
	name		"Super Nailgun"
	modelindex	15
	type		0
	index		INVENTORY_PLASMAGUN
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_bfg"
{
	name		"BFG10K"
	modelindex	16
	type		0
	index		INVENTORY_BFG10K
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_grapplinghook"
{
	name		"Grappling Hook"
	modelindex	17
	type		0
	index		INVENTORY_GRAPPLINGHOOK
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_shells"
{
	name		"Shells"
	modelindex	18
	type		0
	index		INVENTORY_SHELLS
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_bullets"
{
	name		"Bullets"
	modelindex	19
	type		0
	index		INVENTORY_BULLETS
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_grenades"
{
	name		"Bolts"
	modelindex	20
	type		0
	index		INVENTORY_GRENADES
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_cells"
{
	name		"Nails"
	modelindex	21
	type		0
	index		INVENTORY_CELLS
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_lightning"
{
	name		"Lightning"
	modelindex	22
	type		0
	index		INVENTORY_LIGHTNINGAMMO
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_rockets"
{
	name		"Rockets"
	modelindex	23
	type		0
	index		INVENTORY_ROCKETS
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_slugs"
{
	name		"Slugs"
	modelindex	24
	type		0
	index		INVENTORY_SLUGS
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "ammo_bfg"
{
	name		"Bfg Ammo"
	modelindex	25
	type		0
	index		INVENTORY_BFGAMMO
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "holdable_teleporter"
{
	name		"Personal Teleporter"
	modelindex	26
	type		0
	index		INVENTORY_TELEPORTER
	respawntime	60
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "holdable_medkit"
{
	name		"Medkit"
	modelindex	27
	type		0
	index		INVENTORY_MEDKIT
	respawntime	60
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_quad"
{
	name		"Quad Damage"
	modelindex	28
	type		0
	index		INVENTORY_QUAD
	respawntime	120
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_enviro"
{
	name		"Battle Suit"
	modelindex	29
	type		0
	index		INVENTORY_ENVIRONMENTSUIT
	respawntime	120
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_haste"
{
	name		"Speed"
	modelindex	30
	type		0
	index		INVENTORY_HASTE
	respawntime	120
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_invis"
{
	name		"Invisibility"
	modelindex	31
	type		0
	index		INVENTORY_INVISIBILITY
	respawntime	120
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_regen"
{
	name		"Regeneration"
	modelindex	32
	type		0
	index		INVENTORY_REGEN
	respawntime	120
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_flight"
{
	name		"Flight"
	modelindex	33
	type		0
	index		INVENTORY_FLIGHT
	respawntime	120
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "team_CTF_redflag"
{
	name		"Red Flag"
	modelindex	34
	type		0
	index		INVENTORY_REDFLAG
	respawntime	0
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "team_CTF_blueflag"
{
	name		"Blue Flag"
	modelindex	35
	type		0
	index		INVENTORY_BLUEFLAG
	respawntime	0
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_hmg"
{
	name		"Heavy Machinegun"
	modelindex	36
	type		0
	index		INVENTORY_HMG
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_ssg"
{
	name		"Super Shotgun"
	modelindex	37
	type		0
	index		INVENTORY_SSG
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "weapon_nailgun"
{
	name		"Nailgun"
	modelindex	38
	type		0
	index		INVENTORY_NAILGUN
	respawntime	5
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_hourglass"
{
	name		"Hourglass"
	modelindex	39
	type		0
	index		INVENTORY_HOURGLASS
	respawntime	30
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}

iteminfo "item_ammo_box"
{
	name		"Ammo Box"
	modelindex	40
	type		0
	index		INVENTORY_AMMOBOX
	respawntime	40
	mins		{-15,-15,-15}
	maxs		{15,15,15}
}
