// The weapons bots know: this game's thirteen, numbered as weapon_t in
// bg_public.h, with their inventory slots (inv.h) and the numbers of the
// weapon table in bg_misc.c (bots-audit BOT-02). Our own data.

#include "inv.h"


weaponinfo
{
	name			"Gauntlet"
	number			1
	weaponindex		INVENTORY_GAUNTLET
	ammoindex		0
	ammoamount		1
	projectile		"gauntletstrike"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			0.5
}

weaponinfo
{
	name			"Machinegun"
	number			2
	weaponindex		INVENTORY_MACHINEGUN
	ammoindex		INVENTORY_BULLETS
	ammoamount		1
	projectile		"bullet"
	numprojectiles	1
	hspread			0.6
	vspread			0.6
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			0.1
}

weaponinfo
{
	name			"Shotgun"
	number			3
	weaponindex		INVENTORY_SHOTGUN
	ammoindex		INVENTORY_SHELLS
	ammoamount		1
	projectile		"pellet"
	numprojectiles	16
	hspread			5
	vspread			5
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			0.85
}

weaponinfo
{
	name			"Tri-bolt"
	number			4
	weaponindex		INVENTORY_GRENADELAUNCHER
	ammoindex		INVENTORY_GRENADES
	ammoamount		1
	projectile		"bolt"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			1600
	offset			{0, 0, 0}
	activate		0.25
	reload			1.3
}

weaponinfo
{
	name			"Rocket Launcher"
	number			5
	weaponindex		INVENTORY_ROCKETLAUNCHER
	ammoindex		INVENTORY_ROCKETS
	ammoamount		1
	projectile		"rocket"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			1000
	offset			{0, 0, 0}
	activate		0.25
	reload			0.8
}

weaponinfo
{
	name			"Lightning Gun"
	number			6
	weaponindex		INVENTORY_LIGHTNING
	ammoindex		INVENTORY_LIGHTNINGAMMO
	ammoamount		1
	projectile		"lightning"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			0.044
}

weaponinfo
{
	name			"Railgun"
	number			7
	weaponindex		INVENTORY_RAILGUN
	ammoindex		INVENTORY_SLUGS
	ammoamount		1
	projectile		"slug"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			1.5
}

weaponinfo
{
	name			"Super Nailgun"
	number			8
	weaponindex		INVENTORY_PLASMAGUN
	ammoindex		INVENTORY_CELLS
	ammoamount		1
	projectile		"supernail"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			2000
	offset			{0, 0, 0}
	activate		0.25
	reload			0.1
}

weaponinfo
{
	name			"BFG10K"
	number			9
	weaponindex		INVENTORY_BFG10K
	ammoindex		INVENTORY_BFGAMMO
	ammoamount		1
	projectile		"slug"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			1.5
}

weaponinfo
{
	name			"Grappling Hook"
	number			10
	weaponindex		INVENTORY_GRAPPLINGHOOK
	ammoindex		0
	ammoamount		1
	projectile		"hook"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			1800
	offset			{0, 0, 0}
	activate		0.25
	reload			0.4
}

weaponinfo
{
	name			"Heavy Machinegun"
	number			11
	weaponindex		INVENTORY_HMG
	ammoindex		INVENTORY_BULLETS
	ammoamount		1
	projectile		"heavybullet"
	numprojectiles	1
	hspread			0.6
	vspread			0.6
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			0.1
}

weaponinfo
{
	name			"Super Shotgun"
	number			12
	weaponindex		INVENTORY_SSG
	ammoindex		INVENTORY_SHELLS
	ammoamount		1
	projectile		"pellet"
	numprojectiles	16
	hspread			5
	vspread			5
	speed			0
	offset			{0, 0, 0}
	activate		0.25
	reload			1
}

weaponinfo
{
	name			"Nailgun"
	number			13
	weaponindex		INVENTORY_NAILGUN
	ammoindex		INVENTORY_CELLS
	ammoamount		1
	projectile		"nail"
	numprojectiles	1
	hspread			0
	vspread			0
	speed			1800
	offset			{0, 0, 0}
	activate		0.25
	reload			0.1
}

projectileinfo
{
	name			"gauntletstrike"
	damage			75
	radius			0
	damagetype		1		// impact
	gravity			0
}

projectileinfo
{
	name			"bullet"
	damage			6
	radius			0
	damagetype		1		// impact
	gravity			0
}

projectileinfo
{
	name			"heavybullet"
	damage			9
	radius			0
	damagetype		1		// impact
	gravity			0
}

projectileinfo
{
	name			"pellet"
	damage			5
	radius			0
	damagetype		1		// impact
	gravity			0
}

projectileinfo
{
	name			"bolt"
	damage			35
	radius			120
	damagetype		3		// impact and splash
	gravity			0.5
}

projectileinfo
{
	name			"rocket"
	damage			100
	radius			120
	damagetype		3		// impact and splash
	gravity			0
}

projectileinfo
{
	name			"lightning"
	damage			6
	radius			0
	damagetype		1		// impact
	gravity			0
}

projectileinfo
{
	name			"slug"
	damage			80
	radius			0
	damagetype		1		// impact
	gravity			0
}

projectileinfo
{
	name			"nail"
	damage			15
	radius			20
	damagetype		3		// impact and splash
	gravity			0
}

projectileinfo
{
	name			"supernail"
	damage			20
	radius			20
	damagetype		3		// impact and splash
	gravity			0
}

projectileinfo
{
	name			"hook"
	damage			0
	radius			0
	damagetype		1		// impact
	gravity			0
}
